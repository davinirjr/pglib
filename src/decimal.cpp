
#include "pglib.h"
#include "decimal.h"

PyObject* decimal_type;

static PyObject* NaN;

bool Decimal_Init()
{
    PyObject* decimalmod = PyImport_ImportModule("decimal");
    if (!decimalmod)
    {
        PyErr_SetString(PyExc_RuntimeError, "Unable to import decimal");
        return false;
    }

    decimal_type = PyObject_GetAttrString(decimalmod, "Decimal");
    Py_DECREF(decimalmod);

    if (decimal_type == 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "Unable to import decimal.Decimal.");
        return false;
    }
    
    NaN = PyObject_CallFunction(decimal_type, (char*)"s", "NaN");
    if (NaN == 0)
    {
        Py_DECREF(decimal_type);
        return 0;
    }

    return true;
}

PyObject* Decimal_FromASCII(const char* sz)
{
    Object str(PyUnicode_DecodeASCII(sz, strlen(sz), 0));
    if (!str)
        return 0;

    return PyObject_CallFunction(decimal_type, (char*)"O", str.Get());
}

PyObject* Decimal_NaN()
{
    Py_INCREF(NaN);
    return NaN;
}
