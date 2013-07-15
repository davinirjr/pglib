#!/usr/bin/env python3

import sys, os, re, platform
from os.path import join, dirname, abspath, basename
import unittest
from decimal import Decimal
from datetime import date, time, datetime, timedelta
# from testutils import *
from argparse import ArgumentParser, ArgumentTypeError

_TESTSTR = '0123456789-abcdefghijklmnopqrstuvwxyz-'

def _generate_test_string(length):
    """
    Returns a string of composed of `seed` to make a string `length` characters long.

    To enhance performance, there are 3 ways data is read, based on the length of the value, so most data types are
    tested with 3 lengths.  This function helps us generate the test data.

    We use a recognizable data set instead of a single character to make it less likely that "overlap" errors will
    be hidden and to help us manually identify where a break occurs.
    """
    if length <= len(_TESTSTR):
        return _TESTSTR[:length]

    c = (length + len(_TESTSTR)-1) // len(_TESTSTR)
    v = _TESTSTR * c
    return v[:length]


class PGTestCase(unittest.TestCase):

    # These are from the C++ code.  Keep them up to date.

    # # If we are reading a binary, string, or unicode value and do not know how large it is, we'll try reading 2K into a
    # # buffer on the stack.  We then copy into a new Python object.
    # SMALL_READ  = 2048
    #  
    # # A read guaranteed not to fit in the MAX_STACK_STACK stack buffer, but small enough to be used for varchar (4K max).
    # LARGE_READ = 4000

    STR_FENCEPOST_SIZES = [ 0, 1, 255, 256, 510, 511, 512, 1023, 1024, 2047, 2048, 4000, 4095, 4096, 4097, 10 * 1024, 20 * 1024 ]

    STR_FENCEPOSTS = [ _generate_test_string(size) for size in STR_FENCEPOST_SIZES ]

    def __init__(self, conninfo, method_name):
        unittest.TestCase.__init__(self, method_name)
        self.conninfo = conninfo

    def setUp(self):
        self.cnxn = pglib.connect(self.conninfo)

        for i in range(3):
            try:
                self.cnxn.execute("drop table t%d" % i)
            except:
                pass
        
        # self.cnxn.rollback()


    def tearDown(self):
        try:
            self.cnxn.close()
        except:
            # If we've already closed the cursor or connection, exceptions are thrown.
            pass


    def _test_strtype(self, sqltype, value, resulttype=None, colsize=None):
        assert colsize is None or isinstance(colsize, int), colsize
        assert colsize is None or (value is None or colsize >= len(value))

        if colsize:
            sql = "create table t1(s %s(%s))" % (sqltype, colsize)
        else:
            sql = "create table t1(s %s)" % sqltype

        if resulttype is None:
            resulttype = type(value)

        self.cnxn.execute(sql)
        self.cnxn.execute("insert into t1 values($1)", value)
        v = self.cnxn.scalar("select * from t1")
        self.assertEqual(type(v), resulttype)

        if value is not None:
            self.assertEqual(len(v), len(value))

        # To allow buffer --> db --> bytearray tests, always convert the input to the expected result type before
        # comparing.
        if type(value) is not resulttype:
            value = resulttype(value)

        self.assertEqual(v, value)


    def test_execute_ddl(self):
        """
        Ensure we can execute a DDL command and that it returns None.
        """
        r = self.cnxn.execute("create table t1(a int, b int)")
        self.assertEqual(r, None)

    def test_iter(self):
        self.cnxn.execute("create table t1(a varchar(20))")
        self.cnxn.execute("insert into t1 values ('abc')")
        rset = self.cnxn.execute("select * from t1")
        for row in rset:
            self.assertEqual(row.a, 'abc')
            self.assertEqual(row[0], 'abc')

    def test_version(self):
        self.assertEquals(3, len(pglib.version.split('.'))) # 1.3.1 etc.

    #
    # row
    #

    def test_row_zero(self):
        self.cnxn.execute("create table t1(a int)")
        value = self.cnxn.row("select a from t1")
        self.assertEqual(value, None)
        
    def test_row_one(self):
        self.cnxn.execute("create table t1(a int)")
        self.cnxn.execute("insert into t1 values (1)")
        value = self.cnxn.row("select a from t1")
        self.assertEqual(value[0], 1)

    def test_row_many(self):
        self.cnxn.execute("create table t1(a int)")
        self.cnxn.execute("insert into t1 values (1), (2)")
        with self.assertRaises(pglib.Error):
            value = self.cnxn.row("select a from t1")

    #
    # resultset
    #

    def test_rset_length(self):
        """
        Ensure the len(ResultSet) returns the number of rows.
        """
        self.cnxn.execute("create table t1(i int)")
        count = 4
        for i in range(count):
            self.cnxn.execute("insert into t1 values ($1)", i)
        rset = self.cnxn.execute("select * from t1")
        self.assertEquals(len(rset), 4)

    def test_rset_index(self):
        """
        Ensure we can indexing into the ResultSet returns a Row.
        """
        self.cnxn.execute("create table t1(i int)")
        count = 4
        for i in range(count):
            self.cnxn.execute("insert into t1 values ($1)", i)
        rset = self.cnxn.execute("select * from t1")
        row = rset[2]
        self.assertEquals(row[0], 2)

    #
    # scalar
    #

    def test_scalar_zero(self):
        """
        Ensure cnxn.scalar() returns None if no rows are selected.
        """
        self.cnxn.execute("create table t1(a int)")
        value = self.cnxn.scalar("select a from t1")
        self.assertEqual(value, None)
        
    def test_scalar_one(self):
        """
        Ensure cnxn.scalar() returns the first column if one row is selected.
        """
        self.cnxn.execute("create table t1(a int)")
        self.cnxn.execute("insert into t1 values (1)")
        value = self.cnxn.scalar("select a from t1")
        self.assertEqual(value, 1)

    def test_scalar_many(self):
        """
        Ensure cnxn.scalar() raises an exception if multiple rows are selected.
        """
        self.cnxn.execute("create table t1(a int)")
        self.cnxn.execute("insert into t1 values (1), (2)")
        with self.assertRaises(pglib.Error):
            value = self.cnxn.scalar("select a from t1")

    #
    # tuple
    #

    def test_row_to_tuple(self):
        self.cnxn.execute("create table t1(a varchar(20), b int)")
        self.cnxn.execute("insert into t1 values ('one', 1)")
        rset = self.cnxn.execute("select a,b from t1")
        for row in rset:
            t = tuple(row)
            self.assertEqual(t, ('one', 1))

    #
    # boolean
    #

    def test_boolean(self):
        self.cnxn.execute("create table t1(a boolean)")
        for value in [True, False]:
            self.cnxn.execute("insert into t1 values($1)", value)
            result = self.cnxn.scalar("select a from t1 where a=$1", value)
            self.assertEqual(result, value)

    #
    # numeric
    #

    def test_smallint(self):
        self.cnxn.execute("create table t1(a smallint)")
        for value in [-32768, -1, 0, 1, 32767]:
            self.cnxn.execute("insert into t1 values ($1)", value)
        for value in [-32768, -1, 0, 1, 32767]:
            result = self.cnxn.scalar("select a from t1 where a=$1", value)
            self.assertEqual(result, value)

    def test_integer(self):
        self.cnxn.execute("create table t1(a integer)")
        for value in [-2147483648, -32768, -1, 0, 1, 32767, 2147483647]:
            self.cnxn.execute("insert into t1 values ($1)", value)
        for value in [-2147483648, -32768, -1, 0, 1, 32767, 2147483647]:
            result = self.cnxn.scalar("select a from t1 where a=$1", value)
            self.assertEqual(result, value)

    def test_bigint(self):
        self.cnxn.execute("create table t1(a bigint)")
        for value in [-9223372036854775808, -2147483648, -32768, -1, 0, 1, 32767, 2147483647, 9223372036854775807]:
            self.cnxn.execute("insert into t1 values ($1)", value)
        for value in [-2147483648, -32768, -1, 0, 1, 32767, 2147483647]:
            result = self.cnxn.scalar("select a from t1 where a=$1", value)
            self.assertEqual(result, value)

    def test_serial(self):
        self.cnxn.execute("create table t1(a serial, b varchar(20))")
        self.cnxn.execute("insert into t1(b) values ('one')")
        self.cnxn.execute("insert into t1(a, b) values (2147483647, 'max')")
        for value in [1, 2147483647]:
            result = self.cnxn.scalar("select a from t1 where a=$1", value)
            self.assertEqual(result, value)

    def test_decimal(self):
        self.cnxn.execute("create table t1(a decimal(100,7))")
        for s in ['-3.0000000', '123456.7890']:
            value = Decimal(s)
            self.cnxn.execute("delete from t1")
            self.cnxn.execute("insert into t1 values($1)", value)
            result = self.cnxn.scalar("select a from t1")
            self.assertEqual(result, value)


    def test_money(self):
        self.cnxn.execute("create table t1(a money)")
        for s in ['1.23', '0.0', '123.45']:
            value = Decimal(s)
            self.cnxn.execute("delete from t1")
            self.cnxn.execute("insert into t1 values($1)", value)
            result = self.cnxn.scalar("select a from t1")
            self.assertEqual(result, value)


    def test_decimal_nan(self):
        dec = Decimal('NaN')
        self.cnxn.execute("create table t1(a decimal(100,7))")
        self.cnxn.execute("insert into t1 values($1)", dec)
        result = self.cnxn.scalar("select a from t1")
        self.assertEqual(type(result), Decimal)
        self.assert_(result.is_nan())

    #
    # varchar
    #

    def test_varchar_null(self):
        self._test_strtype('varchar', None, colsize=100)

    # Generate a test for each fencepost size: test_varchar_0, etc.
    def _maketest(value):
        def t(self):
            self._test_strtype('varchar', value, colsize=len(value))
        return t
    for value in STR_FENCEPOSTS:
        locals()['test_varchar_%s' % len(value)] = _maketest(value)

    #
    # char
    #

    def test_varchar_null(self):
        self._test_strtype('char', None, colsize=100)

    # Generate a test for each fencepost size: test_char_1, etc.
    #
    # Note: There is no test_char_0 since they are blank padded.
    def _maketest(value):
        def t(self):
            self._test_strtype('char', value, colsize=len(value))
        return t
    for value in [v for v in STR_FENCEPOSTS if len(v) > 0]:
        locals()['test_char_%s' % len(value)] = _maketest(value)

    #
    # date / timestamp
    #

    def test_date(self):
        self.cnxn.execute("create table t1(a date)")
        value = date(2001, 2, 3)
        self.cnxn.execute("insert into t1 values ($1)", value)
        result = self.cnxn.scalar("select a from t1")
        self.assertEqual(result, value)

    def test_time(self):
        value = time(12, 34, 56)
        self.cnxn.execute("create table t1(a time)")
        self.cnxn.execute("insert into t1 values ($1)", value)
        result = self.cnxn.scalar("select a from t1")
        self.assertEqual(result, value)

    def test_timestamp(self):
        self.cnxn.execute("create table t1(a timestamp)")
        value = datetime(2001, 2, 3, 4, 5, 6, 7)
        self.cnxn.execute("insert into t1 values ($1)", value)
        result = self.cnxn.scalar("select a from t1")
        self.assertEqual(result, value)


    def test_dev(self):
        # A placeholder for development.
        pass

def _check_conninfo(value):
    value = value.strip()
    if not re.match(r'^\w+=\w+$', value):
        raise ArgumentTypeError('conninfo must be key=value ("{}")'.format(value))
    return value

def main():
    parser = ArgumentParser()
    parser.add_argument('-v', '--verbose', action='count', help='Increment verbosity', default=0)
    parser.add_argument('-t', '--test', help='Run only the named test')
    parser.add_argument('conninfo', nargs='*', type=_check_conninfo, help='connection string component')

    args = parser.parse_args()

    conninfo = {
        'host'   : 'localhost',
        'dbname' : 'test'
    }

    for part in args.conninfo:
        match = re.match(r'^(\w+)=(\w+)$', part)
        conninfo[match.group(1)] = match.group(2)

    conninfo = ' '.join('{}={}'.format(key, value) for (key, value) in conninfo.items())

    if args.verbose:
        print('Version:', pglib.version)

    if args.test:
        # Run a single test
        if not args.test.startswith('test_'):
            args.test = 'test_%s' % (args.test)

        s = unittest.TestSuite([ PGTestCase(conninfo, args.test) ])
    else:
        # Run all tests in the class

        methods = [ m for m in dir(PGTestCase) if m.startswith('test_') ]
        methods.sort()
        s = unittest.TestSuite([ PGTestCase(conninfo, m) for m in methods ])

    testRunner = unittest.TextTestRunner(verbosity=args.verbose)
    result = testRunner.run(s)


def add_to_path():
    """
    Prepends the build directory to the path so that newly built pglib libraries are used, allowing it to be tested
    without installing it.
    """
    # Put the build directory into the Python path so we pick up the version we just built.
    #
    # To make this cross platform, we'll search the directories until we find the .pyd file.

    import imp

    library_exts  = [ t[0] for t in imp.get_suffixes() if t[-1] == imp.C_EXTENSION ]
    library_names = [ 'pglib%s' % ext for ext in library_exts ]

    # Only go into directories that match our version number. 

    dir_suffix = '-%s.%s' % (sys.version_info[0], sys.version_info[1])

    build = join(dirname(abspath(__file__)), 'build')

    for root, dirs, files in os.walk(build):
        for d in dirs[:]:
            if not d.endswith(dir_suffix):
                dirs.remove(d)

        for name in library_names:
            if name in files:
                sys.path.insert(0, root)
                return
                
    print('Did not find the pglib library in the build directory.  Will use an installed version.')


if __name__ == '__main__':

    # Add the build directory to the path so we're testing the latest build, not the installed version.

    add_to_path()

    import pglib
    main()
