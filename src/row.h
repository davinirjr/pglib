
#ifndef ROW_H
#define ROW_H

struct ResultSet;

struct Row
{
    PyObject_HEAD

    PyObject* columns;
    // The column names, shared with the ResultSet.

    PyObject* values;
    // The values converted to Python objects.
};

extern PyTypeObject RowType;

PyObject* Row_New(ResultSet* rset, int iRow);

#define Row_Check(op) PyObject_TypeCheck(op, &RowType)
#define Row_CheckExact(op) (Py_TYPE(op) == &RowType)

#endif // ROW_H
