
#include "pglib.h"
#include "connection.h"
#include "resultset.h"

PyObject* pModule = 0;
PyObject* Error;

PyObject* strComma;
PyObject* strParens;
PyObject* strLeftParen;
PyObject* strRightParen;
PyObject* strEmpty;

static char module_doc[] = "A straightforward library for PostgreSQL";

static char doc_defaults[] = "Returns the dictionary of default conninfo values.";

static PyObject* mod_defaults(PyObject* self, PyObject* args)
{
    UNUSED(self);

    Object dict(PyDict_New());
    if (!dict)
        return 0;

    PQconninfoOption* aOptions = PQconndefaults();
    PQconninfoOption* p = aOptions;
    while (p->keyword)
    {
        if (p->val == NULL)
        {
            if (PyDict_SetItemString(dict, p->keyword, Py_None) == -1)
                return 0;
        }
        else
        {
            Object val(PyUnicode_FromString(p->val ? p->val : "NULL"));
            if (!val || PyDict_SetItemString(dict, p->keyword, val) == -1)
                return 0;
        }
        p++;
    }
    
    PQconninfoFree(aOptions);

    return dict.Detach();
}

static char connect_doc[] = "";

static PyObject* mod_connect(PyObject* self, PyObject* args, PyObject* kwargs)
{
    UNUSED(self);

    const char* conninfo = 0;
    if (!PyArg_ParseTuple(args, "s", &conninfo))
        return 0;

    return Connection_New(conninfo);
}

static PyObject* mod_test(PyObject* self, PyObject* args)
{
    PyObject* error = PyObject_CallFunction(Error, (char*)"s", "testing");
    PyErr_SetObject(Error, error);
    return 0;
}


static PyMethodDef pglib_methods[] =
{
    { "test",  (PyCFunction)mod_test,  METH_VARARGS, 0 },
    { "connect",  (PyCFunction)mod_connect,  METH_VARARGS, connect_doc },
    { "defaults", (PyCFunction)mod_defaults, METH_NOARGS,  doc_defaults },
    { 0, 0, 0, 0 }
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "pglib",                    // m_name
    module_doc,
    -1,                         // m_size
    pglib_methods,              // m_methods
    0,                          // m_reload
    0,                          // m_traverse
    0,                          // m_clear
    0,                          // m_free
};

static bool InitConstants()
{
    strComma = PyUnicode_FromString(",");
    strParens = PyUnicode_FromString("()");
    strLeftParen = PyUnicode_FromString("(");
    strRightParen = PyUnicode_FromString(")");
    strEmpty = PyUnicode_FromString("");

    return (
        strComma != 0 &&
        strParens != 0 &&
        strLeftParen != 0 &&
        strRightParen != 0 &&
        strEmpty
    );
}

PyMODINIT_FUNC PyInit_pglib()
{
    if (PyType_Ready(&ConnectionType) < 0 || PyType_Ready(&ResultSetType) < 0)
        return 0;

    if (!InitConstants())
        return 0;

    Error = PyErr_NewException("pglib.Error", 0, 0);
    if (!Error)
        return 0;

    Object module(PyModule_Create(&moduledef));
    
    if (!module)
        return 0;

    const char* szVersion = TOSTRING(PGLIB_VERSION);
    PyModule_AddStringConstant(module, "version", (char*)szVersion);

    PyModule_AddObject(module, "Error", Error);

    return module.Detach();
}
