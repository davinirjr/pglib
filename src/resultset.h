
#ifndef RESULTSET_H
#define RESULTSET_H

struct Connection;

extern PyTypeObject ResultSetType;

struct ResultSet
{
    PyObject_HEAD

    PGresult* result;

    int* formats;
    // An array containing a format type for each column.  The type can be 0
    // (text) or 1 (binary).  I don't know why yet, but PostgreSQL can send
    // columns in text even if you ask for binary.  (I may punt and always ask
    // for text since I have to handle every OID's text format anyway.)

    PyObject* columns;
    // A tuple of column names, shared among rows.  Will be 0 if there are no column names.

    Py_ssize_t cFetched;

    bool integer_datetimes;
    // Obtained from the connection, but needed when reading timestamps at which time we won't have access to the
    // connection.
};

PyObject* ResultSet_New(Connection* cnxn, PGresult* result);

#endif // RESULTSET_H
