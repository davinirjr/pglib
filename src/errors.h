
#ifndef ERRORS_H
#define ERRORS_H

struct Connection;

PyObject* SetConnectionError(Connection* cnxn);
PyObject* SetResultError(PGresult* result);

#endif // ERRORS_H

