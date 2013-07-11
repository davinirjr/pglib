
#include "pglib.h"
#include "decimal.h"

static PyObject* decimal_type;

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
        PyErr_SetString(PyExc_RuntimeError, "Unable to import decimal.Decimal.");

    return decimal_type != 0;
}

PyObject* Decimal_FromASCII(const char* sz)
{
    Object str(PyUnicode_DecodeASCII(sz, strlen(sz), 0));
    if (!str)
        return 0;

    return PyObject_CallFunction(decimal_type, (char*)"O", str.Get());
}
