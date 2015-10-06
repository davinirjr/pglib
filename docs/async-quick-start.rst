
Asynchronous Quick Start
========================

Most of the API is the same as the synchronous one, but any that communicate with the server
require the `yield` keyword.

Connecting
----------

To connect, pass a
`libpq connection string <http://www.postgresql.org/docs/9.3/static/libpq-connect.html#LIBPQ-CONNSTRING>`_
to the :func:`async_connect` function. ::

    import asyncio
    import pglib
    cnxn = yield from pglib.connect_async('host=localhost dbname=test')

Selecting
---------

There are asynchronous versions of `execute`, `row`, and `scalar`::

    rset  = yield from cnxn.execute("select id, name from users")
    row   = yield from cnxn.row("select count(*) as cnt from users")
    count = yield from cnxn.scalar("select count(*) from users")

The ResultSet and Row objects don't require yielding.
