
from pprint import pprint
import traceback
from testutils import *
add_to_path()

import pglib

# pprint(pglib.defaults());

cnxn = pglib.connect("host=localhost dbname=test");
# print('cnxn:', cnxn, cnxn.server_version, cnxn.protocol_version)

try:
    # cnxn.execute("select * from test where test_id=$1", "testing")
    cnxn.execute("select * from testx")
except Exception as e:
    traceback.print_exc()
    pprint(e.__dict__)
