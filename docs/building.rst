
Building
========

Before you build you may want to see if there is are
`pre-built binaries <http://github.com/mkleehammer/pglib/releases>`_.

Otherwise you'll need

* Python 3.3 or greater
* the pglib source
* the compiler Python was built with
* PostgreSQL header files and lib files

Once these installed and paths are configured correctly, building is (supposed to be) as simple
as running ``python3 setup.py build`` in the pglib source directory.

OS/X
----

Install PostgreSQL.  The `Postgres.app <http://postgresapp.com>`_ installation is particularly
convenient.

Ensure that ``pg_config`` is in the path so setup.py can use it to determine the include and
lib directories.  If you are using Postgres.app, you can add the following to your ~/.bashrc
file::

    export PATH=$PATH:/Applications/Postgres.app/Contents/MacOS/bin

You will also need Xcode installed *and* you'll need to download the Xcode command line tools.
Do this in the Xcode preferences on the Downloads tab.  The setup script will add the latest
SDK directory to the header and library directories.

Once you have built pglib, you can install it into your global Python site directory using
``sudo python3 setup.py install`` or into your venv directory using ``python3 setup.py
install``.

Linux
-----

You will need some header files and pg_config.  The binary package names differ by
distribution.

==========================   ================  ============
Component                    RedHat            Debian
==========================   ================  ============
Python headers and libs      python-devel      python-dev
PostreSQL headers and libs   postgresql-devel  postgres-dev
==========================   ================  ============

Make sure pg_config is in your path before building.  On RedHat-based systems, you'll
find it in /usr/pgsql-*version*/bin.

Once you have the necessary files and a compiler installed, download the pglib source and
run ``python3 setup.py build`` in the pglib source directory.  Install into your site
directory using ``sudo python3 setup.py install`` or into your venv directory using
``python3 setup.py install``.

Windows
-------

This library has only been tested with 64-bit Python, not 32-bit Python on 64-bit Windows.
It *may* work but it won't be tested until someone `requests it <http://github.com/mkleehammer/pglib/issues>`_.

There are two complications on Windows not present on the other operating systems:

* The PostgreSQL header files have an old copy of the Python header files.
* The resulting library uses libpq.dll which must be in the path to import.

First you will need Visual Studio 10 if building for Python 3.3.  The setup.py scripts always
look for the version of Visual Studio they were built with.

The PostgreSQL files can come from an installed version or by downloading a 
`zipped version <http://www.enterprisedb.com/products-services-training/pgbindownload>`_.
There will be 3 directories you will need: pgsql\\bin, pgsql\\lib, and pgsql\\include.

Unfortunately, as of PostgreSQL 9.3, the PostgreSQL download includes the Python 2.7 header
files.  If these are used you will get lots of errors about undefined items.  To fix this, you
must manually force setup.py to include the Python 3.3 include files *before* including
the PostgreSQL include files.  The Python header files are in the 'include' directory under the
Python installation.

To do this, create a setup.cfg file in the pglib directory and configure it with your
directories::

    [build_ext]
    include_dirs=c:\bin\python33-64\include;c:\bin\pgsql\include
    library-dirs=c:\bin\pgsql\lib

It is very important that the Python include directory is before the pgsql include directory.

Once this is done, make sure Python 3.3 is in your path and run: ``python setup.py
build install``.  If successful, a pglib.pyd file will have been created.

Since pglib dynamically links to libpq.dll, you will need the DLL in your path and the DLLs
that it needs.  This means you will need both pgsql\\lib and pgsql\\bin in your path.
