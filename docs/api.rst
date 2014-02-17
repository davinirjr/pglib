
API
===

pglib
-----

.. module:: pglib

.. data:: version

The pglib module version as a string, such as "1.3.0".

.. function:: connect(conninfo : string) --> Connection

Accepts a
`connection string <http://www.postgresql.org/docs/9.3/static/libpq-connect.html#LIBPQ-CONNSTRING>`_
and returns a new :py:class:`Connection`.  Raises an :py:class:`Error` if an error occurs. ::

  cnxn = pglib.connect('host=localhost dbname=test')

.. function:: defaults() --> dict

Returns a dictionary of default connection string values.

.. data:: PQTRANS_*

Constants returned by :py:meth:`Connection.transaction_status`:

* PQTRANS_ACTIVE
* PQTRANS_INTRANS
* PQTRANS_INERROR
* PQTRANS_UNKNOWN

Connection
----------

.. class:: Connection

Represents a connection to the database.  Internally this wraps a ``PGconn*``.  The database
connection is closed when the Connection object is destroyed.

.. attribute:: Connection.client_encoding

   The client encoding as a string such as "UTF8".

.. attribute:: Connection.protocol_version

   An integer representing the protocol version returned by
   `PQprotocolVersion <http://www.postgresql.org/docs/9.3/static/libpq-status.html#LIBPQ-PQPROTOCOLVERSION">`_.

.. attribute:: Connection.server_version

   An integer representing the server version returned by
   `PQserverVersion <http://www.postgresql.org/docs/9.3/static/libpq-status.html#LIBPQ-PQSERVERVERSION>`_.

.. attribute:: Connection.server_encoding

   The server encoding as a string such as "UTF8".

.. attribute:: Connection.status

   True if the connection is valid and False otherwise.

   Accessing this property calls 
   `PQstatus <http://www.postgresql.org/docs/9.3/static/libpq-status.html#LIBPQ-PQSTATUS>`_
   and returns True if the status is CONNECTION_OK and False otherwise.  Note that this returns
   the last status of the connection but does not actually test the connection.  If you are caching
   connections, consider executing something like 'select 1;' to test an old connection.

.. attribute:: Connection.transaction_status

   Returns the current in-transaction status of the server via
   `PQtransactionStatus <http://www.postgresql.org/docs/9.3/static/libpq-status.html#LIBPQ-PQTRANSACTIONSTATUS>`_
   as one of PQTRANS_IDLE, PQTRANS_ACTIVE, PQTRANS_INTRANS, PQTRANS_INERROR, or PQTRANS_UNKNOWN.

.. method:: Connection.execute(sql [, param, ...]) --> ResultSet | int | None

   Submits a command to the server and waits for the result.::

      rset = cnxn.execute(
                 """
                 select id, name
                   from users
                  where id > $1
                        and bill_overdue = $2
                 """, 100, 1)  # 100 -> $1, 1 -> $2

      count = cnxn.execute("delete from articles where expired <= now()")

   If the command was a select statement a :class:`ResultSet` (collection of rows) is returned.
   If it was an UPDATE or DELETE statement, the number of rows affected is returned.  Otherwise
   None is returned.

   Parameters may be passed as arguments after the SQL statement.  Use $1, $2, etc. as markers
   for these in the SQL.  Parameters must be Python types that pglib can convert to appropriate
   SQL types.  See :ref:`paramtypes`.

   Parameters are always passed to the server separately from the SQL statement
   using `PQexecParams <http://www.postgresql.org/docs/9.3/static/libpq-exec.html#LIBPQ-PQEXECPARAMS>`_
   and pglib *never* modifies the SQL passed to it.  You should *always* pass parameters separately to
   protect against `SQL injection attacks <http://en.wikipedia.org/wiki/SQL_injection>`_.

.. method: Connection.row(sql [, param, ...]) --> Row | None

   A convenience method that submits a command returns the first row of the result.  If the
   result has no rows, None is returned.


ResultSet
---------

.. class:: ResultSet

   Holds the results of a select statement: the column names and a collection of :class:`Row`
   objects.  ResultSets behave as simple sequences, so the number of rows it contains can be
   determined using ``len(rset)`` and individual rows can be accessed by index: ``row =
   rset[3]``.

   ResultSets can also be iterated over::

     for row in rset:
         print(row)

   A ResultSet is a wrapper around a ``PGresult`` pointer and contains data for *all* of the
   rows selected in PostgreSQL's raw, binary format.  Iterating over the rows converts the raw
   data into Python objects and returns them as :class:`Row` objects, but does not "use up" the
   raw data.  The ``PGresult`` memory is not freed until the ResultSet is freed.

.. attribute:: ResultSet.columns

   The column names from the select statement.  Each :class:`Row` from the result set
   will have one element for each column.

Row
---

.. class:: Row

   Row objects are sequence objects that hold query results.  All rows from the same
   result set will have the same number of columns, one for each column in the
   result set's ``columns`` attribute.  Values are converted from PostgreSQL's raw
   format to Python objects as they are accessed.  See :ref:`resulttypes`.

   Rows are similar to tuples; ``len`` returns the number of columns and they can be
   indexed into and iterated over::

     row = rset[0]
     print('col count:', len(row))
     print('first col:', row[0])
     for index, value in enumerate(row):
         print('value', index, 'is', value)

   Columns can also be accessed by name.  (Non-alphanumeric characters are replaced with an
   underscore.)  Use the SQL `as` keyword to change a column's name ::

      rset = cnxn.execute("select cust_id, cust_name from cust limit 1")
      row = rset[0]
      print(row.cust_id, row.cust_name)

      rset = cnxn.execute("select count(*) as total from cust")
      print(rset[0].total)

   Unlike tuples, Row values can be replaced.  This is particularly handy for "fixing up"
   values after fetching them. ::

      row.ctime = row.ctime.replace(tzinfo=timezone)

.. attribute:: Row.columns

   A tuple of column names in the Row, shared with the ResultSet that the Row is from.

   If you select a column actually named "columns", the column will override this attribute.

   To create a dictionary of column names and values, use zip::

     obj = dict(zip(row.columns, row))

Error
-----

.. class:: Error

   The error class raised for all errors.

   Errors generated by pglib itself are rare, but only contain a message.

   Errors reported by the database will contain a message with the format "[sqlstate] database
   message" and the following attributes:

   =================   ===========================   
   attribute           libpq field code
   =================   ===========================   
   severity            PG_DIAG_SEVERITY          
   sqlstate            PG_DIAG_SQLSTATE          
   detail              PG_DIAG_MESSAGE_DETAIL    
   hint                PG_DIAG_MESSAGE_HINT      
   position            PG_DIAG_STATEMENT_POSITION
   internal_position   PG_DIAG_INTERNAL_POSITION 
   internal_query      PG_DIAG_INTERNAL_QUERY    
   context             PG_DIAG_CONTEXT           
   file                PG_DIAG_SOURCE_FILE       
   line                PG_DIAG_SOURCE_LINE       
   function            PG_DIAG_SOURCE_FUNCTION  
   =================   ===========================   

   The most most useful attribute for processing errors is usually
   the `SQLSTATE <http://www.postgresql.org/docs/9.3/static/errcodes-appendix.html>`_.
