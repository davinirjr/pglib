
import asyncio
from _pglib import *

@asyncio.coroutine
def connect_async(conninfo, loop=None):
    if not loop:
        loop = asyncio.get_event_loop()
    cnxn = AsyncConnection(async_connect(conninfo), loop)
    yield from cnxn._connectPoll()
    return cnxn


class AsyncConnection:

    # We count on being able to use these as bitflags.
    assert(PGRES_POLLING_READING == 1 and PGRES_POLLING_WRITING == 2)

    def __init__(self, cnxn, loop):
        self.cnxn = cnxn
        self.loop = loop
        self.sock = cnxn.socket

        self._waiting = 0
        # Debug flags only used for repr indicating the PGRES_POLLING_READING
        # flags we're waiting for.

    def __repr__(self):
        return '<AsyncConnection socket={} waiting=0x{:x}>'.format(self.cnxn.socket, self._waiting)

    @asyncio.coroutine
    def _wait_for(self, flags):
        """
        Creates a future and waits until the socket is in the specified flags.

        flags

          A combination of PGRES_POLLING_READING and PGRES_POLLING_WRITING.
          These have values 1 and 2 so you can provide both using A|B.
        """
        assert flags in (1,2,3), flags

        self._waiting = flags   # for repr

        future = asyncio.Future()
        if flags & PGRES_POLLING_READING:
            self.loop.add_reader(self.sock, self._wait_callback, future, PGRES_POLLING_READING)
        if flags & PGRES_POLLING_WRITING:
            self.loop.add_writer(self.sock, self._wait_callback, future, PGRES_POLLING_WRITING)

        # print('WAITING: %r', self)
        which = yield from future

        if flags & PGRES_POLLING_READING:
            self.loop.remove_reader(self.sock)
        if flags & PGRES_POLLING_WRITING:
            self.loop.remove_writer(self.sock)

        self._waiting = 0

        return which

    def _wait_callback(self, future, state):
        # This is called when the socket indicates it is ready to read or write.
        # The 'state' is which is ready and will be PGRES_POLLING_READING or
        # PGRES_POLLING_WRITING.
        future.set_result(state)

    @asyncio.coroutine
    def _connectPoll(self):
        """
        Called internally by connect_async to wait for the connection to complete.
        """
        # If an error occurs, cnxn._connectPoll() will raise an error.
        while 1:
            flags = self.cnxn._connectPoll()
            if flags == PGRES_POLLING_OK:
                return
            yield from self._wait_for(flags)

    @asyncio.coroutine
    def execute(self, *args):
        c = self.cnxn

        flush = c._sendQueryParams(*args)

        # `flush` is the result if a PQflush call.  See the docs at the bottom
        # of the chapter 31.4. Asynchronous Command Processing.

        while flush == 1:
            which = yield from self._wait_for(PGRES_POLLING_READING | PGRES_POLLING_WRITING)
            if which == PGRES_POLLING_READING:
                c._consumeInput()
            flush = c._flush()

        results = []

        try:
            while 1:
                # We are probably ready and possibly even consumed data above, but
                # we don't really know.  I believe it is okay to call consume more
                # than once.
                while not c._consumeInput():
                    yield from self._wait_for(PGRES_POLLING_READING)
                result = c._getResult()
                results.append(result)
        except StopIteration:
            pass

        if len(results) == 1:
            return results[0]
        else:
            return results

    @asyncio.coroutine
    def script(self, SQL):
        """
        Executes multiple SQL statements separated by semicolons.  Returns None.

        Parameters are not accepted because PostgreSQL's function that will
        execute multiple statements (PQsendQuery) doesn't accept them and the
        one that does accept parameters (PQsendQueryParams) doesn't execute
        multiple statements.
        """
        c = self.cnxn
        flush = c._sendQuery(SQL)

        while flush == 1:
            which = yield from self._wait_for(PGRES_POLLING_READING | PGRES_POLLING_WRITING)
            if which == PGRES_POLLING_READING:
                c._consumeInput()
            flush = c._flush()

        results = []

        try:
            while 1:
                while not c._consumeInput():
                    yield from self._wait_for(PGRES_POLLING_READING)
                c._getResult()

        except StopIteration:
            pass

    @asyncio.coroutine
    def row(self, *args):
        """
        Executes the given SQL and returns the first row.  Returns None if no rows
        are returned.
        """
        rset = yield from self.execute(*args)
        return (rset[0] if rset else None)

    @asyncio.coroutine
    def scalar(self, *args):
        """
        Executes the given SQL and returns the first column of the first row.
        Returns None if no rows are returned.
        """
        rset = yield from self.execute(*args)
        return (rset[0][0] if rset else None)

    @asyncio.coroutine
    def begin(self):
        yield from self.execute("BEGIN")

    @asyncio.coroutine
    def commit(self):
        yield from self.execute("COMMIT")

    @asyncio.coroutine
    def rollback(self):
        yield from self.execute("ROLLBACK")
