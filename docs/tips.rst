
Tips
====

Where X In Parameters
---------------------

You should never embed user-provided data in a SQL statement and most of the time you can use a
simple parameter.  To perform a query like "where x in $1" use the following form::

  rset = cnxn.execute("select * from t where id = ANY($1)", [1,2,3])
