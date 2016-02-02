
# Design
# ======
#
# Normal commands like `execute` wait on the `_wait_for` method.  This method creates a new
# Future for each call and waits on it before returning.  A reader or writer is added to the
# libpq socket that completes the future.  When `_wait_for` wakes up from the future it removes
# the reader and writer.
#
# This is designed for executing a single command at a time.  Since the methods are designed
# for "yield from" this should be fine.
#
# Notifications, however, require a reader to be attached at all times.  If it is possible for
# the reader/writer callback to determine if it was triggered due to notifications or not,
# notifications and execute could be mixed.  In the current design, a connection can either be
# used for executing or listening.

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

    USE_IDLE    = 0
    USE_COMMAND = 1
    USE_LISTEN  = 2

    _USE_TEXT = [ 'idle', 'command', 'listening' ]

    # We count on being able to use these as bitflags.
    assert(PGRES_POLLING_READING == 1 and PGRES_POLLING_WRITING == 2)

    def __init__(self, cnxn, loop):
        self.cnxn = cnxn
        self.loop = loop
        self.sock = cnxn.socket

        self.use = self.USE_IDLE
        # Is the connection in-use and what is it being used for?

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
    def listen(self, *channels):
        """
        Call to listen for the given channel or channels.  An asyncio.Queue is returned which the
        notifications will be written into.
        """
        if self.use:
            raise Exception('The connection is already in use (%s)' % self._USE_TEXT[self.use])

        queue = asyncio.Queue()

        for channel in channels:
            yield from self.execute('listen ' + channel)

        self.use = self.USE_LISTEN
        self.loop.add_reader(self.sock, self._on_notify, queue)

        # It's possible notifications came in while we were issuing LISTEN commands, so make a
        # check now.
        self._on_notify(queue)

        return queue

    @asyncio.coroutine
    def notify(self, channel, payload=None):
        yield from self.execute('select pg_notify($1, $2)', channel, payload)

    def _on_notify(self, queue):
        while self.cnxn._consumeInput():
            n = self.cnxn._notifies()
            if not n:
                break
            for item in n:
                queue.put_nowait(item)


    @asyncio.coroutine
    def execute(self, *args):
        if self.use:
            raise Exception('The connection is already in use (%s)' % self._USE_TEXT[self.use])

        self.use = self.USE_COMMAND

        c = self.cnxn

        flush = c._sendQueryParams(*args)

        # `flush` is the result if a PQflush call.  See the docs at the bottom
        # of chapter 31.4. Asynchronous Command Processing.

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

        self.use = self.USE_IDLE

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
        if self.use:
            raise Exception('The connection is already in use (%s)' % self._USE_TEXT[self.use])

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
