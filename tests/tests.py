#!/usr/bin/env python3

import sys, os, re
import unittest
from decimal import Decimal
from testutils import *
from argparse import ArgumentParser

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

    # If we are reading a binary, string, or unicode value and do not know how large it is, we'll try reading 2K into a
    # buffer on the stack.  We then copy into a new Python object.
    SMALL_READ  = 2048

    # A read guaranteed not to fit in the MAX_STACK_STACK stack buffer, but small enough to be used for varchar (4K max).
    LARGE_READ = 4000

    SMALL_STRING = _generate_test_string(SMALL_READ)
    LARGE_STRING = _generate_test_string(LARGE_READ)

    def __init__(self, conninfo, method_name):
        unittest.TestCase.__init__(self, method_name)
        self.conninfo = conninfo

    def setUp(self):
        self.cnxn = pglib.connect(self.conninfo)

        for i in range(3):
            try:
                self.cnxn.execute("drop table t%d" % i)
                self.cnxn.commit()
            except:
                pass
        
        # self.cnxn.rollback()


    def tearDown(self):
        try:
            self.cnxn.close()
        except:
            # If we've already closed the cursor or connection, exceptions are thrown.
            pass

    def test_insert(self):
        r = self.cnxn.execute("create table t1(a int, b int)")
        self.assertEqual(r, None)

    def test_iter(self):
        self.cnxn.execute("create table t1(a varchar(20))")
        self.cnxn.execute("insert into t1 values ('abc')")
        rset = self.cnxn.execute("select * from t1")
        for row in rset:
            self.assertEqual(row.a, 'abc')
            self.assertEqual(row[0], 'abc')



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

    def test_scalar_zero(self):
        self.cnxn.execute("create table t1(a int)")
        value = self.cnxn.scalar("select a from t1")
        self.assertEqual(value, None)
        
    def test_scalar_one(self):
        self.cnxn.execute("create table t1(a int)")
        self.cnxn.execute("insert into t1 values (1)")
        value = self.cnxn.scalar("select a from t1")
        self.assertEqual(value, 1)

    def test_scalar_many(self):
        self.cnxn.execute("create table t1(a int)")
        self.cnxn.execute("insert into t1 values (1), (2)")
        with self.assertRaises(pglib.Error):
            value = self.cnxn.scalar("select a from t1")

    def test_row_to_tuple(self):
        self.cnxn.execute("create table t1(a varchar(20), b int)")
        self.cnxn.execute("insert into t1 values ('one', 1)")
        rset = self.cnxn.execute("select a,b from t1")
        for row in rset:
            t = tuple(row)
            self.assertEqual(t, ('one', 1))

    # def test_iter_direct(self):
    #     count = 0
    #     for row in self.cnxn.execute(
    #             """
    #             select 1 a, 2 b
    #             union all
    #             select 3 a, 4 b
    #             """):
    #         count += 1
    #         print(row)
    #     self.assertEqual(2, count)
    # 
    # def test_columns(self):
    #     rset = self.cnxn.execute("select 1 a, 2 b")
    #     self.assertEqual(rset.columns, ("a", "b"))
    # 
    # def test_negative_float(self):
    #     value = -200
    #     self.cnxn.execute("create table t1(n float)")
    #     self.cnxn.execute("insert into t1 values ($1)", value)
    #     result  = self.cnxn.execute("select n from t1").fetchone()[0]
    #     self.assertEqual(value, result)
    # 
    # 
    # def _test_strtype(self, sqltype, value, colsize=None):
    #     """
    #     The implementation for string, Unicode, and binary tests.
    #     """
    #     assert colsize is None or (value is None or colsize >= len(value))
    # 
    #     if colsize:
    #         sql = "create table t1(s %s(%s))" % (sqltype, colsize)
    #     else:
    #         sql = "create table t1(s %s)" % sqltype
    # 
    #     self.cnxn.execute(sql)
    #     self.cnxn.execute("insert into t1 values($1)", value)
    #     v = self.cnxn.execute("select * from t1").fetchone()[0]
    #     self.assertEqual(type(v), type(value))
    # 
    #     if value is not None:
    #         self.assertEqual(len(v), len(value))
    # 
    #     self.assertEqual(v, value)
    # 
    # #
    # # varchar
    # #
    # 
    # def test_empty_varchar(self):
    #     self._test_strtype('varchar', '', self.SMALL_READ)
    # 
    # def test_null_varchar(self):
    #     self._test_strtype('varchar', None, self.SMALL_READ)
    # 
    # def test_large_null_varchar(self):
    #     # There should not be a difference, but why not find out?
    #     self._test_strtype('varchar', None, self.LARGE_READ)
    # 
    # def test_small_varchar(self):
    #     self._test_strtype('varchar', self.SMALL_STRING, self.SMALL_READ)
    # 
    # def test_large_varchar(self):
    #     self._test_strtype('varchar', self.LARGE_STRING, self.LARGE_READ)
    # 
    # def test_varchar_many(self):
    #     self.cnxn.execute("create table t1(c1 varchar(300), c2 varchar(300), c3 varchar(300))")
    # 
    #     v1 = 'ABCDEFGHIJ' * 30
    #     v2 = '0123456789' * 30
    #     v3 = '9876543210' * 30
    # 
    #     self.cnxn.execute("insert into t1(c1, c2, c3) values ($1,$2,$3)", v1, v2, v3);
    #     row = self.cnxn.execute("select c1, c2, c3 from t1").fetchone()
    # 
    #     self.assertEqual(v1, row.c1)
    #     self.assertEqual(v2, row.c2)
    #     self.assertEqual(v3, row.c3)
    # 
    # 
    # 
    # def test_small_decimal(self):
    #     # value = Decimal('1234567890987654321')
    #     value = Decimal('100010')       # (I use this because the ODBC docs tell us how the bytes should look in the C struct)
    #     self.cnxn.execute("create table t1(d numeric(19))")
    #     self.cnxn.execute("insert into t1 values($1)", value)
    #     v = self.cnxn.execute("select * from t1").fetchone()[0]
    #     self.assertEqual(type(v), Decimal)
    #     self.assertEqual(v, value)
    # 
    # 
    # def test_small_decimal_scale(self):
    #     # The same as small_decimal, except with a different scale.  This value exactly matches the ODBC documentation
    #     # example in the C Data Types appendix.
    #     value = '1000.10'
    #     value = Decimal(value)
    #     self.cnxn.execute("create table t1(d numeric(20,6))")
    #     self.cnxn.execute("insert into t1 values($1)", value)
    #     v = self.cnxn.execute("select * from t1").fetchone()[0]
    #     self.assertEqual(type(v), Decimal)
    #     self.assertEqual(v, value)
    # 
    # 
    # def test_negative_decimal_scale(self):
    #     value = Decimal('-10.0010')
    #     self.cnxn.execute("create table t1(d numeric(19,4))")
    #     self.cnxn.execute("insert into t1 values($1)", value)
    #     v = self.cnxn.execute("select * from t1").fetchone()[0]
    #     self.assertEqual(type(v), Decimal)
    #     self.assertEqual(v, value)
    # 
    # 
    # def _exec(self):
    #     self.cnxn.execute(self.sql)
    #     
    # def test_close_cnxn(self):
    #     """Make sure using a Cursor after closing its connection doesn't crash."""
    # 
    #     self.cnxn.execute("create table t1(id integer, s varchar(20))")
    #     self.cnxn.execute("insert into t1 values ($1,$2)", 1, 'test')
    #     self.cnxn.execute("select * from t1")
    # 
    #     self.cnxn.close()
    #     
    #     # Now that the connection is closed, we expect an exception.  (If the code attempts to use
    #     # the HSTMT, we'll get an access violation instead.)
    #     self.sql = "select * from t1"
    #     self.assertRaises(pglib.ProgrammingError, self._exec)
    # 
    # def test_empty_string(self):
    #     self.cnxn.execute("create table t1(s varchar(20))")
    #     self.cnxn.execute("insert into t1 values($1)", "")
    # 
    # def test_fixed_str(self):
    #     value = "testing"
    #     self.cnxn.execute("create table t1(s char(7))")
    #     self.cnxn.execute("insert into t1 values($1)", "testing")
    #     v = self.cnxn.execute("select * from t1").fetchone()[0]
    #     self.assertEqual(type(v), str)
    #     self.assertEqual(len(v), len(value)) # If we alloc'd wrong, the test below might work because of an embedded NULL
    #     self.assertEqual(v, value)
    # 
    # def test_negative_row_index(self):
    #     self.cnxn.execute("create table t1(s varchar(20))")
    #     self.cnxn.execute("insert into t1 values($1)", "1")
    #     row = self.cnxn.execute("select * from t1").fetchone()
    #     self.assertEquals(row[0], "1")
    #     self.assertEquals(row[-1], "1")
    # 
    # def test_version(self):
    #     self.assertEquals(3, len(pglib.version.split('.'))) # 1.3.1 etc.
    # 
    # def test_rowcount_delete(self):
    #     self.assertEquals(self.cnxn.rowcount, -1)
    #     self.cnxn.execute("create table t1(i int)")
    #     count = 4
    #     for i in range(count):
    #         self.cnxn.execute("insert into t1 values ($1)", i)
    #     self.cnxn.execute("delete from t1")
    #     self.assertEquals(self.cnxn.rowcount, count)
    # 
    # def test_rowcount_nodata(self):
    #     """
    #     This represents a different code path than a delete that deleted something.
    # 
    #     The return value is SQL_NO_DATA and code after it was causing an error.  We could use SQL_NO_DATA to step over
    #     the code that errors out and drop down to the same SQLRowCount code.  On the other hand, we could hardcode a
    #     zero return value.
    #     """
    #     self.cnxn.execute("create table t1(i int)")
    #     # This is a different code path internally.
    #     self.cnxn.execute("delete from t1")
    #     self.assertEquals(self.cnxn.rowcount, 0)
    # 
    # def test_rowcount_select(self):
    #     self.cnxn.execute("create table t1(i int)")
    #     count = 4
    #     for i in range(count):
    #         self.cnxn.execute("insert into t1 values ($1)", i)
    #     self.cnxn.execute("select * from t1")
    #     self.assertEquals(self.cnxn.rowcount, 4)
    # 
    # # PostgreSQL driver fails here?
    # # def test_rowcount_reset(self):
    # #     "Ensure rowcount is reset to -1"
    # # 
    # #     self.cnxn.execute("create table t1(i int)")
    # #     count = 4
    # #     for i in range(count):
    # #         self.cnxn.execute("insert into t1 values ($1)", i)
    # #     self.assertEquals(self.cnxn.rowcount, 1)
    # # 
    # #     self.cnxn.execute("create table t2(i int)")
    # #     self.assertEquals(self.cnxn.rowcount, -1)
    # 
    # def test_lower_case(self):
    #     "Ensure pglib.lowercase forces returned column names to lowercase."
    # 
    #     # Has to be set before creating the cursor, so we must recreate self.cnxn.
    # 
    #     pglib.lowercase = True
    #     self.cnxn = self.cnxn.cursor()
    # 
    #     self.cnxn.execute("create table t1(Abc int, dEf int)")
    #     self.cnxn.execute("select * from t1")
    # 
    #     names = [ t[0] for t in self.cnxn.description ]
    #     names.sort()
    # 
    #     self.assertEquals(names, [ "abc", "def" ])
    # 
    #     # Put it back so other tests don't fail.
    #     pglib.lowercase = False
    #     
    # def test_row_description(self):
    #     """
    #     Ensure Cursor.description is accessible as Row.cursor_description.
    #     """
    #     self.cnxn = self.cnxn.cursor()
    #     self.cnxn.execute("create table t1(a int, b char(3))")
    #     self.cnxn.commit()
    #     self.cnxn.execute("insert into t1 values(1, 'abc')")
    # 
    #     row = self.cnxn.execute("select * from t1").fetchone()
    #     self.assertEquals(self.cnxn.description, row.cursor_description)
    #     
    # 
    # def test_executemany(self):
    #     self.cnxn.execute("create table t1(a int, b varchar(10))")
    # 
    #     params = [ (i, str(i)) for i in range(1, 6) ]
    # 
    #     self.cnxn.executemany("insert into t1(a, b) values ($1,$2)", params)
    # 
    #     # REVIEW: Without the cast, we get the following error:
    #     # [07006] [unixODBC]Received an unsupported type from Postgres.;\nERROR:  table "t2" does not exist (14)
    # 
    #     count = self.cnxn.execute("select cast(count(*) as int) from t1").fetchone()[0]
    #     self.assertEqual(count, len(params))
    # 
    #     self.cnxn.execute("select a, b from t1 order by a")
    #     rows = self.cnxn.fetchall()
    #     self.assertEqual(count, len(rows))
    # 
    #     for param, row in zip(params, rows):
    #         self.assertEqual(param[0], row[0])
    #         self.assertEqual(param[1], row[1])
    # 
    # 
    # def test_executemany_failure(self):
    #     """
    #     Ensure that an exception is raised if one query in an executemany fails.
    #     """
    #     self.cnxn.execute("create table t1(a int, b varchar(10))")
    # 
    #     params = [ (1, 'good'),
    #                ('error', 'not an int'),
    #                (3, 'good') ]
    #     
    #     self.failUnlessRaises(pglib.Error, self.cnxn.executemany, "insert into t1(a, b) value ($1, $2)", params)
    # 
    #     
    # def test_row_slicing(self):
    #     self.cnxn.execute("create table t1(a int, b int, c int, d int)");
    #     self.cnxn.execute("insert into t1 values(1,2,3,4)")
    # 
    #     row = self.cnxn.execute("select * from t1").fetchone()
    # 
    #     result = row[:]
    #     self.failUnless(result is row)
    # 
    #     result = row[:-1]
    #     self.assertEqual(result, (1,2,3))
    # 
    #     result = row[0:4]
    #     self.failUnless(result is row)
    # 
    # 
    # def test_row_repr(self):
    #     self.cnxn.execute("create table t1(a int, b int, c int, d int)");
    #     self.cnxn.execute("insert into t1 values(1,2,3,4)")
    # 
    #     row = self.cnxn.execute("select * from t1").fetchone()
    # 
    #     result = str(row)
    #     self.assertEqual(result, "(1, 2, 3, 4)")
    # 
    #     result = str(row[:-1])
    #     self.assertEqual(result, "(1, 2, 3)")
    # 
    #     result = str(row[:1])
    #     self.assertEqual(result, "(1,)")


def _check_conninfo(value):
    value = value.strip()
    if not re.match(r'^\w+=\w+$', value):
        raise argparse.ArgumentTypeError('conninfo must be key=value ("{}")'.format(value))
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

if __name__ == '__main__':

    # Add the build directory to the path so we're testing the latest build, not the installed version.

    add_to_path()

    import pglib
    main()
