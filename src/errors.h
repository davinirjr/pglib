
#ifndef ERRORS_H
#define ERRORS_H

class Connection;

PyObject* SetConnectionError(Connection* cnxn);
PyObject* SetResultError(PGresult* result);

#endif // ERRORS_H

