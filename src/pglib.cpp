
#include "pglib.h"
#include "connection.h"
#include "resultset.h"
#include "row.h"
#include "datatypes.h"
#include "getdata.h"
#include "params.h"
#include "errors.h"

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

static char connect_doc[] = 
"connect(connection_string) --> Connection";

static PyObject* mod_connect(PyObject* self, PyObject* args, PyObject* kwargs)
{
    UNUSED(self);

    const char* conninfo = 0;
    if (!PyArg_ParseTuple(args, "s", &conninfo))
        return 0;

    PGconn* pgconn;
    Py_BEGIN_ALLOW_THREADS
    pgconn = PQconnectdb(conninfo);
    Py_END_ALLOW_THREADS
    if (pgconn == 0)
        return PyErr_NoMemory();

    if (PQstatus(pgconn) != CONNECTION_OK)
    {
        const char* szError = PQerrorMessage(pgconn);
        PyErr_SetString(Error, szError);
        Py_BEGIN_ALLOW_THREADS
        PQfinish(pgconn);
        Py_END_ALLOW_THREADS
        return 0;
    }

    return Connection_New(pgconn, false);
}

static PyObject* mod_async_connect(PyObject* self, PyObject* args, PyObject* kwargs)
{
    // TODO: I don't know why, but the documentation says that timeouts are not
    // enforced for an async connection.  We'll need to pick out the timeout
    // from the connection string and implement our own.
    //
    // We might be able to get the requested value from PQconninfo.

    UNUSED(self);

    const char* conninfo = 0;
    if (!PyArg_ParseTuple(args, "s", &conninfo))
        return 0;

    PGconn* pgconn = PQconnectStart(conninfo);
    if (pgconn == 0)
        return PyErr_NoMemory();

    if (PQstatus(pgconn) == CONNECTION_BAD)
    {
        SetConnectionError(pgconn);
        PQfinish(pgconn);
        return 0;
    }

    return Connection_New(pgconn, true);
}



// static PyObject* mod_test(PyObject* self, PyObject* args)
// {
//     return 0;
// }

static PyMethodDef pglib_methods[] =
{
    // { "test",  (PyCFunction)mod_test,  METH_VARARGS, 0 },
    { "connect",  (PyCFunction)mod_connect,  METH_VARARGS, connect_doc },
    { "async_connect",  (PyCFunction)mod_async_connect,  METH_VARARGS, connect_doc },
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

static bool InitStringConstants()
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

struct ConstantDef
{
    const char* szName;
    int value;
};

#define MAKECONST(v) { #v, v }
static const ConstantDef aConstants[] = {
    MAKECONST(PQTRANS_IDLE),
    MAKECONST(PQTRANS_ACTIVE),
    MAKECONST(PQTRANS_INTRANS),
    MAKECONST(PQTRANS_INERROR),
    MAKECONST(PQTRANS_UNKNOWN),
    MAKECONST(PGRES_POLLING_READING),
    MAKECONST(PGRES_POLLING_WRITING),
    MAKECONST(PGRES_POLLING_FAILED),
    MAKECONST(PGRES_POLLING_OK),
};

PyMODINIT_FUNC PyInit__pglib()
{
    if (PQisthreadsafe() == 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "Postgres libpq is not multithreaded");
        return 0;
    }

    if (PyType_Ready(&ConnectionType) < 0 || PyType_Ready(&ResultSetType) < 0 || PyType_Ready(&RowType) < 0)
        return 0;

    if (!DataTypes_Init())
        return 0;

    if (!GetData_Init())
        return 0;

    Params_Init();

    if (!InitStringConstants())
        return 0;

    Error = PyErr_NewException("_pglib.Error", 0, 0);
    if (!Error)
        return 0;

    Object module(PyModule_Create(&moduledef));
    
    if (!module)
        return 0;

    for (unsigned int i = 0; i < _countof(aConstants); i++)
        PyModule_AddIntConstant(module, (char*)aConstants[i].szName, aConstants[i].value);

    const char* szVersion = TOSTRING(PGLIB_VERSION);
    PyModule_AddStringConstant(module, "version", (char*)szVersion);

    PyModule_AddObject(module, "Error", Error);

    PyModule_AddObject(module, "Connection", (PyObject*)&ConnectionType);
    Py_INCREF((PyObject*)&ConnectionType);
    PyModule_AddObject(module, "Row", (PyObject*)&RowType);
    Py_INCREF((PyObject*)&RowType);
    PyModule_AddObject(module, "ResultSet", (PyObject*)&ResultSetType);
    Py_INCREF((PyObject*)&ResultSetType);

    return module.Detach();
}
