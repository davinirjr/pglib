
#include "pglib.h"
#include "connection.h"
#include "resultset.h"
#include "errors.h"

PyObject* Connection_New(const char* conninfo)
{
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

    Connection* cnxn = PyObject_NEW(Connection, &ConnectionType);
    if (cnxn == 0)
    {
        PQfinish(pgconn);
        return 0;
    }

    cnxn->pgconn = pgconn;

    return reinterpret_cast<PyObject*>(cnxn);
}

struct Params
{
    Oid*   types;
    char** values;
    int*   lengths;
    int*   formats;
    
    Params(int count)
    {
        types   = (Oid*)  malloc(count * sizeof(Oid));
        values  = (char**)malloc(count * sizeof(char*));
        lengths = (int*)  malloc(count * sizeof(int));
        formats = (int*)  malloc(count * sizeof(int));
    }

    bool valid() const
    {
        return types && values && lengths && formats;
    }

    ~Params()
    {
        free(types);
        free(values);
        free(lengths);
        free(formats);
    }
};

static bool BindParams(Params& params, PyObject* args)
{
    // Binds arguments 1-n.  Argument zero is expected to be the SQL statement itself.

    if (!params.valid())
    {
        PyErr_NoMemory();
        return false;
    }

    for (int i = 0, c = PyTuple_GET_SIZE(args)-1; i < c; i++)
    {
        PyObject* param = PyTuple_GET_ITEM(args, i+1);
        if (param == Py_None)
        {
            params.types[i]   = 0;
            params.values[i]  = 0;
            params.lengths[i] = 0;
            params.formats[i] = 0;
        }
        else
        {
            PyErr_Format(Error, "Unable to bind parameter %d: unhandled object type %s", (i+1), param->ob_type->tp_name);
            return false;
        }
    }
}

static PyObject* Connection_execute(PyObject* self, PyObject* args)
{
    Connection* cnxn = (Connection*)self;

    // TODO: Check connection state.

    Py_ssize_t cParams = PyTuple_Size(args) - 1;
    if (cParams < 0)
    {
        PyErr_SetString(PyExc_TypeError, "execute() takes at least 1 argument (0 given)");
        return 0;
    }

    PyObject* pSql = PyTuple_GET_ITEM(args, 0);
    if (!PyUnicode_Check(pSql))
    {
        PyErr_SetString(PyExc_TypeError, "The first argument to execute must be a string.");
        return 0;
    }

    // Note: PQexec allows multiple statements separated by semicolons, but PQexecParams does not.  This means we
    // support multiple when no parameters are passed.

    PGresult* result = 0;
    if (cParams == 0)
    {
        result = PQexec(cnxn->pgconn, PyUnicode_AsUTF8(pSql));
    }
    else
    {
        Params params(cParams);
        if (!BindParams(params, args))
            return 0;

        result = PQexecParams(cnxn->pgconn, PyUnicode_AsUTF8(pSql),
                              cParams,
                              params.types,
                              params.values,
                              params.lengths,
                              params.formats,
                              1); // binary format
    }
    
    if (result == 0)
    {
        // Apparently this only happens for very serious errors, but the docs aren't terribly clear.
        PyErr_SetString(Error, "Fatal error");
        return 0;
    }

    ExecStatusType status = PQresultStatus(result);
    if (status == PGRES_TUPLES_OK)
        // Result_New will take ownership of `result`.
        return ResultSet_New(result);

    switch (status)
    {
    case PGRES_COMMAND_OK:
	case PGRES_COPY_OUT:
	case PGRES_COPY_IN:
    case PGRES_COPY_BOTH:
    case PGRES_NONFATAL_ERROR: // ?
        PQclear(result);
        Py_RETURN_NONE;
    }

    // SetResultError will take ownership of `result`.
    return SetResultError(result);
}


static void Connection_dealloc(PyObject* self)
{
    Connection* cnxn = (Connection*)self;
    if (cnxn->pgconn)
    {
        Py_BEGIN_ALLOW_THREADS
        PQfinish(cnxn->pgconn);
        Py_END_ALLOW_THREADS
    }
    
    PyObject_Del(self);
}

static PyObject* Connection_repr(PyObject* self)
{
    Connection* cnxn = (Connection*)self;
    return PyUnicode_FromFormat("Connection { dbname=%s user=%s }", PQdb(cnxn->pgconn), PQuser(cnxn->pgconn));
}

static PyObject* Connection_server_version(PyObject* self, void* closure)
{
    UNUSED(closure);
    Connection* cnxn = (Connection*)self;
    return PyLong_FromLong(PQserverVersion(cnxn->pgconn));
}
static PyObject* Connection_protocol_version(PyObject* self, void* closure)
{
    UNUSED(closure);
    Connection* cnxn = (Connection*)self;
    return PyLong_FromLong(PQprotocolVersion(cnxn->pgconn));
}

static PyGetSetDef Connection_getset[] = {
    { (char*)"server_version",   (getter)Connection_server_version,   0, (char*)"The server version", 0 },
    { (char*)"protocol_version", (getter)Connection_protocol_version, 0, (char*)"The protocol version", 0 },
    { 0 }
};

static struct PyMethodDef Connection_methods[] =
{
    { "execute", Connection_execute, METH_VARARGS, 0 },
    { 0, 0, 0, 0 }
};

PyTypeObject ConnectionType =
{
    PyVarObject_HEAD_INIT(0, 0)
    "pglib.Connection",         // tp_name
    sizeof(Connection),         // tp_basicsize
    0,                          // tp_itemsize
    Connection_dealloc,         // destructor tp_dealloc
    0,                          // tp_print
    0,                          // tp_getattr
    0,                          // tp_setattr
    0,                          // tp_compare
    Connection_repr,            // tp_repr
    0,                          // tp_as_number
    0,                          // tp_as_sequence
    0,                          // tp_as_mapping
    0,                          // tp_hash
    0,                          // tp_call
    0,                          // tp_str
    0,                          // tp_getattro
    0,                          // tp_setattro
    0,                          // tp_as_buffer
    Py_TPFLAGS_DEFAULT,         // tp_flags
    0, //connection_doc,             // tp_doc
    0,                          // tp_traverse
    0,                          // tp_clear
    0,                          // tp_richcompare
    0,                          // tp_weaklistoffset
    0,                          // tp_iter
    0,                          // tp_iternext
    Connection_methods,         // tp_methods
    0,                          // tp_members
    Connection_getset,          // tp_getset
    0,                          // tp_base
    0,                          // tp_dict
    0,                          // tp_descr_get
    0,                          // tp_descr_set
    0,                          // tp_dictoffset
    0,                          // tp_init
    0,                          // tp_alloc
    0,                          // tp_new
    0,                          // tp_free
    0,                          // tp_is_gc
    0,                          // tp_bases
    0,                          // tp_mro
    0,                          // tp_cache
    0,                          // tp_subclasses
    0,                          // tp_weaklist
};
