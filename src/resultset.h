
#ifndef RESULTSET_H
#define RESULTSET_H

struct Connection;

extern PyTypeObject ResultSetType;

struct ResultSet
{
    PyObject_HEAD

    PGresult* result;

    Py_ssize_t cRows;
    Py_ssize_t cCols;

    Py_ssize_t cFetched;

    // Obtained from the connection, but needed when reading timestamps at which time we won't have access to the
    // connection.
    bool integer_datetimes;
};

PyObject* ResultSet_New(Connection* cnxn, PGresult* result);

#endif // RESULTSET_H
