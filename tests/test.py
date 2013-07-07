#!/usr/bin/env python3

from pprint import pprint
import traceback
from testutils import *
add_to_path()

import pglib

# pprint(pglib.defaults());

cnxn = pglib.connect("host=localhost dbname=test");
print('dir:', dir(pglib))
print('version:', pglib.version)
# print('cnxn:', cnxn, repr(cnxn.server_encoding), repr(cnxn.client_encoding))

try:
    # cnxn.execute("select * from test where test_id=$1", "testing")
    # cnxn.execute("select * from testx")

    # value = 0
    # value = 41234
    # value = 3123456789
    # n = cnxn.execute("insert into test values ($1, 'zero')", value)

    value = 'test'
    n = cnxn.execute("insert into test values (4, $1)", value)

    print(n, type(n))

    # n = cnxn.execute("delete from test where id = $1", 0)
    # print(n, type(n))

except Exception as e:
    traceback.print_exc()
    pprint(e.__dict__)
