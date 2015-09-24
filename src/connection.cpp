
#include "pglib.h"
#include "connection.h"
#include "resultset.h"
#include "errors.h"
#include "params.h"
#include "getdata.h"
#include "row.h"

struct ConstantDef
{
    const char* szName;
    int value;
};

#define MAKECONST(v) { #v, v }
static const ConstantDef aTxnFlags[] = {
    MAKECONST(PQTRANS_IDLE),
    MAKECONST(PQTRANS_ACTIVE),
    MAKECONST(PQTRANS_INTRANS),
    MAKECONST(PQTRANS_INERROR),
    MAKECONST(PQTRANS_UNKNOWN),
};

enum {
    REQUIRE_OPEN            = 0x01,
    REQUIRE_SYNC            = 0x02,
    REQUIRE_ASYNC           = 0x04,
    REQUIRE_ASYNC_CONNECTED = 0x08 | REQUIRE_OPEN | REQUIRE_ASYNC,
};

inline Connection* CastConnection(PyObject* self, int flags=0)
{
    // Casts the Python pointer to our Connection structure.  If flags are
    // passed and any of the requirements are not met, the appropriate exception
    // is raised and zero is returned.

    Connection* cnxn = (Connection*)self;

    if ((flags & REQUIRE_OPEN) && (!cnxn->pgconn))
    {
        SetStringError(Error, "The connection is not open");
        return 0;
    }

    if ((flags & REQUIRE_SYNC) && (cnxn->async_status != ASYNC_STATUS_SYNC))
    {
        SetStringError(Error, "The connection is not synchronous");
        return 0;
    }

    if ((flags & REQUIRE_ASYNC) && (cnxn->async_status == ASYNC_STATUS_SYNC))
    {
        SetStringError(Error, "The connection is not async");
        return 0;
    }

    if ((flags & REQUIRE_ASYNC_CONNECTED) == REQUIRE_ASYNC_CONNECTED && (cnxn->async_status == ASYNC_STATUS_CONNECTING))
    {
        SetStringError(Error, "The async connection has not yet connected");
        return 0;
    }

    return cnxn;
}

const char* NameFromTxnFlag(int flag)
{
    for (size_t i = 0; i < _countof(aTxnFlags); i++)
        if (aTxnFlags[i].value == flag)
            return aTxnFlags[i].szName;
    return "invalid";
}

static void notice_receiver(void *arg, const PGresult* res)
{
}

static void OnCompleteConnection(Connection* cnxn)
{
    // Initialization that can't happen until after the connection is complete.
    // Separated because sync and async connections complete in different code
    // paths.

    const char* szID = PQparameterStatus(cnxn->pgconn, "integer_datetimes");
    cnxn->integer_datetimes = (szID == 0) || (strcmp(szID, "on") == 0);
}

PyObject* Connection_New(PGconn* pgconn, bool async)
{
    Connection* cnxn = PyObject_NEW(Connection, &ConnectionType);

    if (cnxn == 0)
    {
        PQfinish(pgconn);
        return 0;
    }

    // TODO: Does this need to be done after connecting?
    PQsetNoticeReceiver(pgconn, notice_receiver, 0);

    cnxn->pgconn = pgconn;
    cnxn->tracefile = 0;


    cnxn->async_status = async ? ASYNC_STATUS_CONNECTING : ASYNC_STATUS_SYNC;

    if (!async)
        OnCompleteConnection(cnxn);
    else
        PQsetnonblocking(cnxn->pgconn, 1);

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

    Params params(cParams);
    if (!BindParams(cnxn, params, args))
        return 0;

    PGresult* result;
    Py_BEGIN_ALLOW_THREADS
    result = PQexecParams(cnxn->pgconn, PyUnicode_AsUTF8(pSql),
                          cParams,
                          params.types,
                          params.values,
                          params.lengths,
                          params.formats,
                          1); // binary format
    Py_END_ALLOW_THREADS

    if (result == 0)
    {
        // Apparently this only happens for very serious errors, but the docs aren't terribly clear.
        PyErr_SetString(Error, "Fatal error");
        return 0;
    }

    return result;
}

static const char doc_script[] = "Connection.script(sql) --> None\n\n"
    "Executes a script which can contain multiple statements separated by semicolons.";

static PyObject* Connection_script(PyObject* self, PyObject* args)
{
    PyObject* pScript;
    if (!PyArg_ParseTuple(args, "U", &pScript))
        return 0;

    Connection* cnxn = (Connection*)self;
    ResultHolder result = PQexec(cnxn->pgconn, PyUnicode_AsUTF8(pScript));
    if (result == 0)
        return 0;

    switch (PQresultStatus(result)) {
    case PGRES_BAD_RESPONSE:
    case PGRES_NONFATAL_ERROR:
    case PGRES_FATAL_ERROR:
        return SetResultError(result.Detach());

    default:
        Py_RETURN_NONE;
    }
}

const char* doc_copy_from_csv = 
    "Connection.copy_from_csv(table, source, header=0) --> None\n"
    "\n"
    "Executes a COPY FROM command.\n"
    "\n"
    "table\n"
    "  The table to copy to.  This can also contain the columns to populate.\n"
    "\n"
    "source\n"
    "  The data to copy from.  This can be a string formatted as CSV or a file-like\n"
    "  object (anything with a read method that returns a string or bytes object).\n"
    "\n"
    "Examples:\n"
    "  cnxn.copy_from_csv('t1', open('test.csv'), header=1)\n"
    "  cnxn.copy_from_csv('t1(a,b,c)', open('test.csv'), header=1)\n"
    "  cnxn.copy_from_csv('t1', gzip.open('test.csv'), header=1)\n"
    "  cnxn.copy_from_csv('t1', \"1,'one'\\n2,'two'\")\n";

static PyObject* Connection_copy_from_csv(PyObject* self, PyObject* args, PyObject* kwargs)
{
    static const char *kwlist[] = { "table", "source", "header", 0 };

    PyObject* table;
    PyObject* source;
    int header = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "UO|p", (char**)kwlist, &table, &source, &header))
        return 0;

    char header_token[] = "header";
    if (header == 0)
        header_token[0] = 0;
    PyObject* sql = PyUnicode_FromFormat("copy %U from stdin with csv %s", table, header_token);

    // If source is a string (Unicode), store the UTF-encoded value in buffer. If a byte
    // object, store directly in buffer.  Otherwise, buffer will be zero and `source` must be
    // an object with a read method (e.g. file).
    const char* buffer = 0;
    Py_ssize_t buffer_size = 0;
    PyObject* read_method = 0;

    if (PyUnicode_Check(source))
    {
        buffer = PyUnicode_AsUTF8AndSize(source, &buffer_size);
        if (buffer == 0)
            return 0;
    }
    else
    {
        if (!PyObject_HasAttrString(source, "read"))
            return PyErr_Format(Error, "CSV source must be a string or file-like object.");
        read_method = PyObject_GetAttrString(source, "read");
    }

    Connection* cnxn = (Connection*)self;
    const char* szSQL = PyUnicode_AsUTF8(sql);
    ResultHolder result;
    Py_BEGIN_ALLOW_THREADS
    result = PQexec(cnxn->pgconn, szSQL);
    Py_END_ALLOW_THREADS

    if (result == 0)
        return 0;

    switch (PQresultStatus(result)) {
    case PGRES_COPY_IN:
        // This is what we are expecting.
        break;

    case PGRES_BAD_RESPONSE:
    case PGRES_NONFATAL_ERROR:
    case PGRES_FATAL_ERROR:
        return SetResultError(result.Detach());

    default:
        return PyErr_Format(Error, "Result was not PGRES_COPY_IN: %d", (int)PQresultStatus(result));
    }

    if (buffer != 0)
    {
        int copyStatus = 0;
        Py_BEGIN_ALLOW_THREADS
        copyStatus = PQputCopyData(cnxn->pgconn, buffer, (int)buffer_size);
        Py_END_ALLOW_THREADS
        if (copyStatus != 1)
            return SetConnectionError(cnxn);
    }
    else
    {
        Object read_args(Py_BuildValue("(l)", 20));
        if (read_args == 0)
            return 0;

        for (;;)
        {
            Object s(PyObject_CallObject(read_method, read_args));
            if (s == 0)
                return 0;
            if (PyBytes_Check(s))
            {
                buffer = PyBytes_AS_STRING(s.Get());
                buffer_size = PyBytes_GET_SIZE(s.Get());
            }
            else if (PyUnicode_Check(s.Get()))
            {
                buffer = PyUnicode_AsUTF8AndSize(s.Get(), &buffer_size);
            }
            else
            {
                return PyErr_Format(Error, "Result of reading is not a bytes object: %R", s.Get());
            }
            if (buffer == 0)
                return 0;
            if (buffer_size == 0)
                break;
            int copyStatus = 0;
            Py_BEGIN_ALLOW_THREADS
            copyStatus = PQputCopyData(cnxn->pgconn, buffer, (int)buffer_size);
            Py_END_ALLOW_THREADS
            if (copyStatus != 1)
                return SetConnectionError(cnxn);
        }
    }

    if (PQputCopyEnd(cnxn->pgconn, 0) != 1)
        return SetConnectionError(cnxn);

    Py_RETURN_NONE;
}

static PyObject* ReturnResult(Connection* cnxn, ResultHolder& result)
{
    // An internal function for handling a result set so we can share the sync
    // and async implementations.

    ExecStatusType status = PQresultStatus(result);

    switch (status)
    {
    case PGRES_TUPLES_OK:
        return ResultSet_New(cnxn, result.Detach());

    case PGRES_COMMAND_OK:
    {
        const char* sz = PQcmdTuples(result);
        if (sz == 0 || *sz == 0)
            Py_RETURN_NONE;
        return PyLong_FromLong(atoi(sz));
    }

    case PGRES_EMPTY_QUERY:
        // This means an empty string was passed, but we check that already so we should never get here.
        Py_RETURN_NONE;

    case PGRES_COPY_OUT:
    case PGRES_COPY_IN:
    case PGRES_COPY_BOTH:
        Py_RETURN_NONE;

    case PGRES_BAD_RESPONSE:
    case PGRES_NONFATAL_ERROR:
    case PGRES_FATAL_ERROR:
    default:
        // Fall through and return an error.
        break;
    }

    // SetResultError will take ownership of `result`.
    return SetResultError(result.Detach());
}

static PyObject* Connection_execute(PyObject* self, PyObject* args)
{
    Connection* cnxn = (Connection*)self;

    ResultHolder result = internal_execute(self, args);
    if (result == 0)
        return 0;

    return ReturnResult(cnxn, result);
}

static PyObject* Connection_row(PyObject* self, PyObject* args)
{
    Connection* cnxn = (Connection*)self;

    ResultHolder result = internal_execute(self, args);
    if (result == 0)
        return 0;

    ExecStatusType status = PQresultStatus(result);

    if (status != PGRES_TUPLES_OK)
    {
        switch (status)
        {
            case PGRES_COMMAND_OK:
            case PGRES_EMPTY_QUERY:
            case PGRES_COPY_OUT:
            case PGRES_COPY_IN:
            // case PGRES_COPY_BOTH:
                PyErr_SetString(Error, "SQL wasn't a query");
                return 0;

            case PGRES_BAD_RESPONSE:
            case PGRES_NONFATAL_ERROR:
            case PGRES_FATAL_ERROR:
            default:
                // SetResultError will take ownership of `result`.
                return SetResultError(result.Detach());
        }
    }

    int cRows = PQntuples(result);
    if (cRows == 0)
    {
        Py_RETURN_NONE;
    }

    if (cRows != 1)
        return PyErr_Format(Error, "row query returned %d rows, not 1", cRows);

    Object rset = ResultSet_New(cnxn, result);
    if (rset == 0)
        return 0;

    result.Detach();

    return Row_New((ResultSet*)rset.Get(), 0);
}

static PyObject* Connection_reset(PyObject* self, PyObject* args)
{
    Connection* cnxn = (Connection*)self;
    PQreset(cnxn->pgconn);
    Py_RETURN_NONE;
}

static PyObject* Connection_trace(PyObject* self, PyObject* args)
{
    const char* filename;
    const char* mode = 0;
    if (!PyArg_ParseTuple(args, "z|z", &filename, &mode))
        return 0;

    Connection* cnxn = (Connection*)self;

    if (cnxn->tracefile)
    {
        PQuntrace(cnxn->pgconn);
        fclose(cnxn->tracefile);
        cnxn->tracefile = 0;
    }

    if (filename)
    {
        cnxn->tracefile = fopen(filename, mode ? mode : "w");
        if (cnxn->tracefile == 0)
            return PyErr_SetFromErrnoWithFilename(Error, filename);
        PQtrace(cnxn->pgconn, cnxn->tracefile);
    }

    Py_RETURN_NONE;
}

static PyObject* Connection_scalar(PyObject* self, PyObject* args)
{
    Connection* cnxn = (Connection*)self;

    ResultHolder result = internal_execute(self, args);
    if (result == 0)
        return 0;

    ExecStatusType status = PQresultStatus(result);

    if (status != PGRES_TUPLES_OK)
    {
        switch (status)
        {
            case PGRES_COMMAND_OK:
            case PGRES_EMPTY_QUERY:
            case PGRES_COPY_OUT:
            case PGRES_COPY_IN:
            // case PGRES_COPY_BOTH:
                PyErr_SetString(Error, "SQL wasn't a query");
                return 0;

            case PGRES_BAD_RESPONSE:
            case PGRES_NONFATAL_ERROR:
            case PGRES_FATAL_ERROR:
            default:
                // SetResultError will take ownership of `result`.
                return SetResultError(result.Detach());
        }
    }

    int cRows = PQntuples(result);

    if (cRows == 0)
    {
        Py_RETURN_NONE;
    }

    if (cRows != 1)
        return PyErr_Format(Error, "scalar query returned %d rows, not 1", cRows);

    return ConvertValue(result, 0, 0, cnxn->integer_datetimes, PQfformat(result, 0));
}

static const char doc_begin[] = "Connection.begin() --> None\n\n"
    "Begins a transaction.  Raises an error if already in a transaction.";

static PyObject* Connection_begin(PyObject* self, PyObject* args)
{
    UNUSED(args);
    Connection* cnxn = (Connection*)self;
    
    PGTransactionStatusType txnstatus;
    ExecStatusType status = PGRES_COMMAND_OK;
    ResultHolder result;

    Py_BEGIN_ALLOW_THREADS
    txnstatus = PQtransactionStatus(cnxn->pgconn);
    if (txnstatus == PQTRANS_IDLE)
    {
        result = PQexec(cnxn->pgconn, "BEGIN");
        status = PQresultStatus(result);
    }
    Py_END_ALLOW_THREADS

    if (txnstatus != PQTRANS_IDLE)
        return PyErr_Format(Error, "Connection transaction status is not idle: %s", NameFromTxnFlag(txnstatus));

    if (status != PGRES_COMMAND_OK)
        return SetResultError(result);

    Py_RETURN_NONE;
}

static const char doc_commit[] = "Connection.commit() --> None\n\n"
    "Commits a transaction if one is active.  It is not an error to call outside of a transaction.";

static PyObject* Connection_commit(PyObject* self, PyObject* args)
{
    UNUSED(args);
    Connection* cnxn = (Connection*)self;
    
    PGTransactionStatusType txnstatus;
    ExecStatusType status = PGRES_COMMAND_OK;
    ResultHolder result;

    Py_BEGIN_ALLOW_THREADS
    txnstatus = PQtransactionStatus(cnxn->pgconn);
    if (txnstatus == PQTRANS_INTRANS)
    {
        result = PQexec(cnxn->pgconn, "COMMIT");
        status = PQresultStatus(result);
    }
    Py_END_ALLOW_THREADS

    if (txnstatus != PQTRANS_IDLE && txnstatus != PQTRANS_INTRANS)
        return PyErr_Format(Error, "Connection transaction status is invalid: %s", NameFromTxnFlag(txnstatus));

    if (status != PGRES_COMMAND_OK)
        return SetResultError(result);

    Py_RETURN_NONE;
}

static const char doc_rollback[] = "Connection.rollback() --> None\n\n"
    "Rolls back a transaction if one is active.  It is not an error to call outside of a transaction.";

static PyObject* Connection_rollback(PyObject* self, PyObject* args)
{
    UNUSED(args);
    Connection* cnxn = (Connection*)self;
    
    PGTransactionStatusType txnstatus;
    ExecStatusType status = PGRES_COMMAND_OK;
    ResultHolder result;

    Py_BEGIN_ALLOW_THREADS
    txnstatus = PQtransactionStatus(cnxn->pgconn);
    if (txnstatus == PQTRANS_INTRANS)
    {
        result = PQexec(cnxn->pgconn, "ROLLBACK");
        status = PQresultStatus(result);
    }
    Py_END_ALLOW_THREADS

    if (txnstatus != PQTRANS_IDLE && txnstatus != PQTRANS_INTRANS)
        return PyErr_Format(Error, "Connection transaction status is invalid: %s", NameFromTxnFlag(txnstatus));

    if (status != PGRES_COMMAND_OK)
        return SetResultError(result);

    Py_RETURN_NONE;
}

static void Connection_dealloc(PyObject* self)
{
    Connection* cnxn = (Connection*)self;

    Py_BEGIN_ALLOW_THREADS
    if (cnxn->pgconn)
        PQfinish(cnxn->pgconn);
    if (cnxn->tracefile)
        fclose(cnxn->tracefile);
    Py_END_ALLOW_THREADS

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

static PyObject* Connection_status(PyObject* self, void* closure)
{
    UNUSED(closure);
    Connection* cnxn = (Connection*)self;
    return PyBool_FromLong(PQstatus(cnxn->pgconn) == CONNECTION_OK);
}

static PyObject* Connection_socket(PyObject* self, void* closure)
{
    UNUSED(closure);
    Connection* cnxn = CastConnection(self);
    if (!cnxn->pgconn)
        return PyLong_FromLong(-1);

    return PyLong_FromLong(PQsocket(cnxn->pgconn));
}

static PyObject* Connection_transaction_status(PyObject* self, void* closure)
{
    UNUSED(closure);
    Connection* cnxn = (Connection*)self;
    return PyLong_FromLong(PQtransactionStatus(cnxn->pgconn));
}

static PyObject* Connection_sendQuery(PyObject* self, PyObject* args)
{
    PyObject* pScript;
    if (!PyArg_ParseTuple(args, "U", &pScript))
        return 0;

    Connection* cnxn = CastConnection(self, REQUIRE_ASYNC_CONNECTED);
    if (!cnxn)
        return 0;

    int sent;
    Py_BEGIN_ALLOW_THREADS
    sent = PQsendQuery(cnxn->pgconn, PyUnicode_AsUTF8(pScript));
    Py_END_ALLOW_THREADS

    if (!sent)
        return SetConnectionError(cnxn->pgconn);

    int result = PQflush(cnxn->pgconn);

    if (result == -1)
        return SetConnectionError(cnxn->pgconn);

    return PyLong_FromLong(result);
}

static PyObject* Connection_sendQueryParams(PyObject* self, PyObject* args)
{
    Connection* cnxn = CastConnection(self, REQUIRE_ASYNC_CONNECTED);
    if (!cnxn)
        return 0;

    Py_ssize_t cParams = PyTuple_Size(args) - 1;
    if (cParams < 0)
    {
        PyErr_SetString(PyExc_TypeError, "Expected at least 1 argument (0 given)");
        return 0;
    }

    PyObject* pSql = PyTuple_GET_ITEM(args, 0);
    if (!PyUnicode_Check(pSql))
    {
        PyErr_SetString(PyExc_TypeError, "The first argument must be the SQL string.");
        return 0;
    }

    int sent;

    Params params(cParams);
    if (!BindParams(cnxn, params, args))
        return 0;

    Py_BEGIN_ALLOW_THREADS
    sent = PQsendQueryParams(cnxn->pgconn, PyUnicode_AsUTF8(pSql),
                             cParams,
                             params.types,
                             params.values,
                             params.lengths,
                             params.formats,
                             1); // binary format
    Py_END_ALLOW_THREADS

    if (!sent)
        return SetConnectionError(cnxn->pgconn);

    int result = PQflush(cnxn->pgconn);

    if (result == -1)
        return SetConnectionError(cnxn->pgconn);

    return PyLong_FromLong(result);
}

static PyObject* Connection_flush(PyObject* self, PyObject* args)
{
    UNUSED(args);

    Connection* cnxn = CastConnection(self, REQUIRE_ASYNC || REQUIRE_OPEN);
    if (!cnxn)
        return 0;

    int result = PQflush(cnxn->pgconn);

    if (result == -1)
        return SetConnectionError(cnxn->pgconn);

    return PyLong_FromLong(result);
}

static PyObject* Connection_consumeInput(PyObject* self, PyObject* args)
{
    // Consumes input (obviously) and returns True if data is ready to be read
    // with _getResult and False if data is not ready.  If an error occurs an
    // exception is raised.

    Connection* cnxn = CastConnection(self, REQUIRE_ASYNC || REQUIRE_OPEN);
    if (!cnxn)
        return 0;

    int result = PQconsumeInput(cnxn->pgconn);
    if (result == 0)
        return SetConnectionError(cnxn->pgconn);

    return PyBool_FromLong(PQisBusy(cnxn->pgconn) == 0);
}

static PyObject* Connection_getResult(PyObject* self, PyObject* args)
{
    UNUSED(args);

    Connection* cnxn = CastConnection(self, REQUIRE_ASYNC || REQUIRE_OPEN);
    if (!cnxn)
        return 0;

    ResultHolder result;
    Py_BEGIN_ALLOW_THREADS
    result = PQgetResult(cnxn->pgconn);
    Py_END_ALLOW_THREADS

    if (result.p == 0)
    {
        // This is normal and is how libpq tells us we've retrieved all of the
        // results.
        PyErr_SetNone(PyExc_StopIteration);
        return 0;
    }

    return ReturnResult(cnxn, result);
}

static PyObject* Connection_connectPoll(PyObject* self, PyObject* args)
{
    // A wrapper around PQconnectPoll.
    //
    // Returns the polling constants OK, READING, and WRITING.  If an error
    // occurs it will be raised.

    Connection* cnxn = CastConnection(self, REQUIRE_ASYNC || REQUIRE_OPEN);
    if (!cnxn)
        return 0;

    if (cnxn->async_status != ASYNC_STATUS_CONNECTING)
        return SetStringError(Error, "Already connected");

    PostgresPollingStatusType status = PQconnectPoll(cnxn->pgconn);
    if (status == PGRES_POLLING_OK)
    {
        cnxn->async_status = ASYNC_STATUS_IDLE;
        OnCompleteConnection(cnxn);
    }

    if (status == PGRES_POLLING_READING || status == PGRES_POLLING_WRITING || status == PGRES_POLLING_OK)
        return PyLong_FromLong(status);

    SetConnectionError(cnxn);

    PQfinish(cnxn->pgconn);
    cnxn->pgconn = 0;

    return 0;
}

static PyGetSetDef Connection_getset[] = {
    { (char*)"server_version",     (getter)Connection_server_version,     0, (char*)"The server version", 0 },
    { (char*)"protocol_version",   (getter)Connection_protocol_version,   0, (char*)"The protocol version", 0 },
    { (char*)"server_encoding",    (getter)Connection_server_encoding,    0, (char*)0, 0 },
    { (char*)"client_encoding",    (getter)Connection_client_encoding,    0, (char*)0, 0 },
    { (char*)"status",             (getter)Connection_status,             0, (char*)"True if status is CONNECTION_OK, False otherwise", 0 },
    { (char*)"transaction_status", (getter)Connection_transaction_status, 0, (char*)"Returns PQtransactionStatus constants", 0 },
    { (char*)"socket",             (getter)Connection_socket,             0, (char*)"Returns the socket fileno", 0 },
    { 0 }
};

static struct PyMethodDef Connection_methods[] =
{
    { "execute", Connection_execute, METH_VARARGS, 0 },
    { "row",     Connection_row,     METH_VARARGS, 0 },
    { "scalar",  Connection_scalar,  METH_VARARGS, 0 },
    { "trace",   Connection_trace,   METH_VARARGS, 0 },
    { "reset",   Connection_reset,   METH_NOARGS,  0 },
    { "script",  Connection_script,  METH_VARARGS, doc_script },
    { "copy_from_csv", (PyCFunction) Connection_copy_from_csv, METH_VARARGS | METH_KEYWORDS, doc_copy_from_csv },
    { "begin",    Connection_begin,   METH_NOARGS, doc_begin },
    { "commit",   Connection_commit,   METH_NOARGS, doc_commit },
    { "rollback", Connection_rollback,   METH_NOARGS, doc_rollback },
    { "_connectPoll", Connection_connectPoll, METH_NOARGS, 0 },
    { "_sendQuery", Connection_sendQuery, METH_VARARGS, 0 },
    { "_sendQueryParams", Connection_sendQueryParams, METH_VARARGS, 0 },
    { "_consumeInput", Connection_consumeInput, METH_VARARGS, 0 },
    { "_getResult", Connection_getResult, METH_VARARGS, 0 },
    { "_flush", Connection_flush, METH_VARARGS, 0 },
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
