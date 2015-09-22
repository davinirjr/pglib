
#ifndef ERRORS_H
#define ERRORS_H

#include "connection.h"

PyObject* SetConnectionError(PGconn* pgconn);

inline PyObject* SetConnectionError(Connection* cnxn)
{
    return SetConnectionError(cnxn->pgconn);
}

PyObject* SetResultError(PGresult* result);

inline PyObject* SetStringError(PyObject* type, const char* szMsg)
{
    // A simple wrapper around PyErr_SetString that returns 0 so we use one
    // call.
    PyErr_SetString(type, szMsg);
    return 0;
}

#endif // ERRORS_H

