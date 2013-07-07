
#ifndef RESULTSET_H
#define RESULTSET_H

extern PyTypeObject ResultSetType;

struct ResultSet
{
    PyObject_HEAD

    PGresult* result;

    Py_ssize_t cRows;
    Py_ssize_t cCols;

    Py_ssize_t cFetched;
};

PyObject* ResultSet_New(PGresult* result);

#endif // RESULTSET_H
