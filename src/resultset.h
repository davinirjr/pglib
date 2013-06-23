
#ifndef RESULTSET_H
#define RESULTSET_H

extern PyTypeObject ResultSetType;

struct ResultSet
{
    PyObject_HEAD
    PGresult* result;
};

PyObject* ResultSet_New(PGresult* result);

#endif // RESULTSET_H
