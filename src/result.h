
#ifndef RESULT_H
#define RESULT_H

extern PyTypeObject ResultType;

struct Result
{
    PyObject_HEAD
    PGresult* result;
};

PyObject* Result_New(PGresult* result);

#endif // RESULT_H
