
# All synchronous classes and functions are implemented in C in _pglib.
from _pglib import *

# The asynchronous code is in Python.
from .asyncpglib import connect_async
