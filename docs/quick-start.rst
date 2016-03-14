
Quick Start
===========

Connecting
----------

To connect, pass a
`libpq connection string <http://www.postgresql.org/docs/9.3/static/libpq-connect.html#LIBPQ-CONNSTRING>`_
to the :func:`connect` function. ::

    import pglib
    cnxn = pglib.connect('host=localhost dbname=test')

Basic Selecting
---------------

:meth:`Connection.execute <execute>` accepts a SQL statement and optional parameters.  What it
returns depends on the kind of SQL statement:

* A select statement will return :class:`ResultSet` with all of the rows.
* An insert, update, or delete statement will return the number of rows modified.
* Any other statement (e.g. "create table") will return None.

If the SQL was a select statement, :data:`ResultSet.columns` will be a tuple containing the
column names selected.

The Row objects can be accessed by indexing into the ResultSet or iterating over it. ::

    rset = cnxn.execute("select id, name from users")

    print('columns:', rset.columns) # ('id', 'name')

    print('count:', len(rset))
    print('first:', rset[0])

    for row in rset:
        print(row)

The PostgreSQL client library, libps, stores all row data in memory while the ResultSet exists.
This means that result sets can be iterated over multiple times, but it also means large result
sets use a lot of memory and should be discarded as soon as possible.

Row objects are similar to tuples, but they also allow access to columns by name. ::

    rset = cnxn.execute("select id, name from users limit 1")
    row = rset[0]
    print('id:', row[0]) # access id by column index
    print('id:', row.id) # access id by column name

The SQL 'as' keyword makes it easy to set the name::

    rset = cnxn.execute("select count(*) as cnt from users")
    row = rset[0]
    print(row.cnt)

The Connection.row method is a convenience method that returns the first result Row.  If
there are no results it returns None. ::

    row = cnxn.row("select count(*) as cnt from users")
    print(row.cnt)

The Connection.scalar method, another convenience method, returns the first column of the
first row.  If there are no results it returns None. ::

    count = cnxn.scalar("select count(*) from users")
    print(count)

Each row is a Python sequence, so it can be used in many places that a tuple or list can.
To convert the values into a tuple use `tuple(row)`.

Finally, to make it convenient to pass a Row around to functions, the columns are also available
from the row object.  Note that a column actually named 'columns' will override this. ::

    print('columns:', row.columns)

    # Convert to dictionary:
    d = dict(zip(row.columns, row))

Parameters
----------

PostgreSQL supports parameters using $1, $2, etc. as a place holder in the SQL.  Values for these
are passed after the SQL.  The first parameter passed is used for $1, the second for $2, etc. ::

    cnxn.execute("""
                 select id, name
                   from users
                  where id > $1
                        and bill_overdue = $2
                 """, 100, 1)  # 100 -> $1, 1 -> $2
