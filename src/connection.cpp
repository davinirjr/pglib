
#include "pglib.h"
#include "connection.h"
#include "resultset.h"
#include "errors.h"
#include "params.h"
#include "getdata.h"
#include "row.h"

static const char* EXEC_STATUS_TEXT[] = 
{
    "PGRES_EMPTY_QUERY",
    "PGRES_COMMAND_OK",
    "PGRES_TUPLES_OK",
    "PGRES_COPY_OUT",
    "PGRES_COPY_IN",
    "PGRES_BAD_RESPONSE",
    "PGRES_NONFATAL_ERROR",
    "PGRES_FATAL_ERROR",
    "PGRES_COPY_BOTH"
};


static void notice_receiver(void *arg, const PGresult* res)
{
}

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

    PQsetNoticeReceiver(pgconn, notice_receiver, 0);

    Connection* cnxn = PyObject_NEW(Connection, &ConnectionType);
    if (cnxn == 0)
    {
        PQfinish(pgconn);
        return 0;
    }

    cnxn->pgconn = pgconn;

    return reinterpret_cast<PyObject*>(cnxn);
}

static PGresult* internal_execute(PyObject* self, PyObject* args)
{
    Connection* cnxn = (Connection*)self;
    
    // TODO: Check connection state.

    Py_ssize_t cParams = PyTuple_Size(args) - 1;
    if (cParams < 0)
    {
        PyErr_SetString(PyExc_TypeError, "Expected at least 1 argument (0 given)");
        return 0;
    }

    PyObject* pSql = PyTuple_GET_ITEM(args, 0);
    if (!PyUnicode_Check(pSql))
    {
        PyErr_SetString(PyExc_TypeError, "The first argument must be a string.");
        return 0;
    }

    // printf("SQL: %s\n", PyUnicode_AsUTF8(pSql));

    PGresult* result = 0;
    if (cParams == 0)
    {
        // result = PQexec(cnxn->pgconn, PyUnicode_AsUTF8(pSql));

        result = PQexecParams(cnxn->pgconn, PyUnicode_AsUTF8(pSql),
                              0, 0, 0, 0, 0, 
                              1); // binary format

    }
    else
    {
        Params params(cParams);
        if (!BindParams(cnxn, params, args))
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

    return result;
}


static PyObject* Connection_execute(PyObject* self, PyObject* args)
{
    PGresult* result = internal_execute(self, args);
    if (result == 0)
        return 0;
    
    ExecStatusType status = PQresultStatus(result);

    // printf("status: %s\n", EXEC_STATUS_TEXT[status]);

    if (status == PGRES_TUPLES_OK)
    {
        // Result_New will take ownership of `result`.
        return ResultSet_New(result);
    }
    
    if (status == PGRES_COMMAND_OK)
    {
        const char* sz = PQcmdTuples(result);
        if (sz == 0 || *sz == 0)
            Py_RETURN_NONE;

        return PyLong_FromLong(atoi(sz));
    }
    
    switch (status)
    {
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

static PyObject* Connection_row(PyObject* self, PyObject* args)
{
    ResultHolder result = internal_execute(self, args);
    if (result == 0)
        return 0;

    ExecStatusType status = PQresultStatus(result);

    // printf("status: %s\n", EXEC_STATUS_TEXT[status]);

    if (status != PGRES_TUPLES_OK)
    {
        PyErr_SetString(Error, "SQL wasn't a query");
        return 0;
    }

    int cRows = PQntuples(result);

    if (cRows == 0)
    {
        Py_RETURN_NONE;
    }

    if (cRows != 1)
        return PyErr_Format(Error, "row query returned %d rows, not 1", cRows);
            
    Object rset = ResultSet_New(result);
    if (rset == 0)
        return 0;

    result.Detach();

    return Row_New((ResultSet*)rset.Get(), 0);
}


static PyObject* Connection_scalar(PyObject* self, PyObject* args)
{
    ResultHolder result = internal_execute(self, args);
    if (result == 0)
        return 0;

    ExecStatusType status = PQresultStatus(result);

    // printf("status: %s\n", EXEC_STATUS_TEXT[status]);

    if (status != PGRES_TUPLES_OK)
    {
        PyErr_SetString(Error, "SQL wasn't a query");
        return 0;
    }

    int cRows = PQntuples(result);

    if (cRows == 0)
    {
        Py_RETURN_NONE;
    }
            
    if (cRows != 1)
        return PyErr_Format(Error, "scalar query returned %d rows, not 1", cRows);

    return ConvertValue(result, 0, 0);
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

static PyObject* Connection_server_encoding(PyObject* self, void* closure)
{
    UNUSED(closure);
    Connection* cnxn = (Connection*)self;
    const char* sz = PQparameterStatus(cnxn->pgconn, "server_encoding");
    if (sz == 0)
        return PyErr_NoMemory();
    return PyUnicode_DecodeUTF8(sz, strlen(sz), 0);
}

static PyObject* Connection_client_encoding(PyObject* self, void* closure)
{
    UNUSED(closure);
    Connection* cnxn = (Connection*)self;
    const char* sz = PQparameterStatus(cnxn->pgconn, "client_encoding");
    if (sz == 0)
        return PyErr_NoMemory();
    return PyUnicode_DecodeUTF8(sz, strlen(sz), 0);
}

static PyGetSetDef Connection_getset[] = {
    { (char*)"server_version",   (getter)Connection_server_version,   0, (char*)"The server version", 0 },
    { (char*)"protocol_version", (getter)Connection_protocol_version, 0, (char*)"The protocol version", 0 },
    { (char*)"server_encoding",  (getter)Connection_server_encoding,  0, (char*)0, 0 },
    { (char*)"client_encoding",  (getter)Connection_client_encoding,  0, (char*)0, 0 },
    { 0 }
};

static struct PyMethodDef Connection_methods[] =
{
    { "execute", Connection_execute, METH_VARARGS, 0 },
    { "row",     Connection_row,     METH_VARARGS, 0 },
    { "scalar",  Connection_scalar,  METH_VARARGS, 0 },
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
