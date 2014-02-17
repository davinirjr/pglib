
#include "pglib.h"
#include "datatypes.h"

PyObject* decimal_type;
PyObject* uuid_type;

static PyObject* NaN;

bool DataTypes_Init()
{
    PyObject* mod = PyImport_ImportModule("decimal");
    if (!mod)
    {
        PyErr_SetString(PyExc_RuntimeError, "Unable to import decimal");
        return false;
    }

    decimal_type = PyObject_GetAttrString(mod, "Decimal");
    Py_DECREF(mod);

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

    // UUID

    mod = PyImport_ImportModule("uuid");
    if (!mod)
    {
        PyErr_SetString(PyExc_RuntimeError, "Unable to import uuid module");
        return false;
    }

    uuid_type = PyObject_GetAttrString(mod, "UUID");
    Py_DECREF(mod);

    if (uuid_type == 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "Unable to import uuid.UUID.");
        return false;
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

PyObject* UUID_FromBytes(const char* pch)
{
    char szBuffer[33];
    for (int i = 0; i < 16; i++)
        sprintf(&szBuffer[i * 2], "%02x", (int)(unsigned char)pch[i]);
    return PyObject_CallFunction(uuid_type, (char*)"s#", szBuffer, 32);
}
