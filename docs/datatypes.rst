
Data Types
==========

Right now there is a limited set of data types and text is always encoded as UTF-8.  Feel
free to open an `issue <https://github.com/mkleehammer/pglib/issues>`_ to request new ones.

.. _paramtypes:

Parameter Types
---------------

Parameters of the following types are accepted:

+-----------------------+------------------+
| Python Type           | SQL Type         |
+=======================+==================+
| None                  | NULL             |
+-----------------------+------------------+
| bool                  | boolean          |
+-----------------------+------------------+
| bytes                 | bytea            |
+-----------------------+------------------+
| bytearray             | bytea            |
+-----------------------+------------------+
| datetime.date         | date             |
+-----------------------+------------------+
| datetime.datetime     | timestamp        |
+-----------------------+------------------+
| datetime.time         | time             |
+-----------------------+------------------+
| datetime.timedelta    | interval         |
+-----------------------+------------------+
| decimal.Decimal       | numeric          |
+-----------------------+------------------+
| float                 | float8           |
+-----------------------+------------------+
| int                   | int64 or numeric |
+-----------------------+------------------+
| str                   | text (UTF-8)     |
+-----------------------+------------------+
| uuid.UUID             | uuid             |
+-----------------------+------------------+
| tuple<int>, list<int> | array<int>       |
+-----------------------+------------------+
| tuple<str>, list<str> | array<str>       |
+-----------------------+------------------+

Arrays can only contain one type, so tuples and lists must contain elements of all of the same
type.  Only strings and integers are supported at this time.  Note that a list or tuple can
contain None, but it must contain at least one string or integer so the type of array can be
determined.

.. _resulttypes:

Result Types
------------

The following data types are recognized in results:

+---------------------------+--------------------+
| SQL Type                  | Python Type        |
+===========================+====================+
| NULL                      | None               |
+---------------------------+--------------------+
| boolean                   | bool               |
+---------------------------+--------------------+
| bytea                     | bytes              |
+---------------------------+--------------------+
| char, varchar, text       | str (UTF-8)        |
+---------------------------+--------------------+
| float4, float8            | float              |
+---------------------------+--------------------+
| smallint, integer, bigint | int                |
+---------------------------+--------------------+
| money                     | decimal.Decimal    |
+---------------------------+--------------------+
| numeric                   | decimal.Decimal    |
+---------------------------+--------------------+
| date                      | datetime.date      |
+---------------------------+--------------------+
| time                      | datetime.time      |
+---------------------------+--------------------+
| timestamp                 | datetime.datetime  |
+---------------------------+--------------------+
| uuid                      | uuid.UUID          |
+---------------------------+--------------------+
| array<int>                | list<int>          |
+---------------------------+--------------------+
| array<text>               | list<str>          |
+---------------------------+--------------------+

Python's ``timedelta`` only stores days, seconds, and microseconds internally, so intervals
with year and month are not supported.
