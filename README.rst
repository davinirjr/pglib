
pglib is a Python 3.3+ module for working with PostgreSQL databases.  It is a C extension that
exposes the `libpq <http://www.postgresql.org/docs/9.3/static/libpq.html>`_ API.  It is
designed to be small, fast, and as convenient as possible.  It provides both synchronous and
asynchronous APIs.

Unlike some libraries, it never modifies the SQL you pass.  Parameters are passed using the
official libpq protocol for parameters.

Documentation is available in /docs and online at http://pglib.readthedocs.org
