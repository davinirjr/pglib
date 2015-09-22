
#include "pglib.h"
#include "errors.h"
#include "connection.h"

struct ResultErrorField
{
    const char* szAttr;
    const int fieldcode;
    const int type; // 0 string, 1 int
};

static const ResultErrorField errorFields[] =
{
    { "severity",          PG_DIAG_SEVERITY,           0 },
    { "sqlstate",          PG_DIAG_SQLSTATE,           0 },
    // { "primary",           PG_DIAG_MESSAGE_PRIMARY,    0 },
    { "detail",            PG_DIAG_MESSAGE_DETAIL,     0 },
    { "hint",              PG_DIAG_MESSAGE_HINT,       0 },
    { "position",          PG_DIAG_STATEMENT_POSITION, 1 },
    { "internal_position", PG_DIAG_INTERNAL_POSITION,  1 },
    { "internal_query",    PG_DIAG_INTERNAL_QUERY,     0 },
    { "context",           PG_DIAG_CONTEXT,            0 },
    { "file",              PG_DIAG_SOURCE_FILE,        0 },
    { "line",              PG_DIAG_SOURCE_LINE,        1 },
    { "function",          PG_DIAG_SOURCE_FUNCTION,    0 }
};

PyObject* SetConnectionError(PGconn* pgconn)
{
    const char* szMessage = PQerrorMessage(pgconn);
    PyErr_SetString(Error, szMessage);
    return 0;
}

PyObject* SetResultError(PGresult* r)
{
    // Creates an exception from `result`.
    //
    // This function takes ownership of `result` and will clear it, even if an exception cannot be created.
    //
    // Always returns zero so it can be called using "return SetResultError(result);"

    // TODO: Make a new exception class that always has SQLSTATE

    ResultHolder result(r); // make sure `r` gets cleared no matter what

    const char* szMessage  = PQresultErrorMessage(result);
    const char* szSQLSTATE = PQresultErrorField(result, PG_DIAG_SQLSTATE);
    if (!szMessage || !szSQLSTATE)
        return PyErr_NoMemory();

    Object msg(PyUnicode_FromFormat("[%s] %s", szSQLSTATE, szMessage));
    if (!msg)
        return 0;

    PyObject* error = PyObject_CallFunction(Error, (char*)"O", msg.Get());
    if (!error)
        return 0;

    for (size_t i = 0; i < _countof(errorFields); i++)
    {
        const char* szValue = PQresultErrorField(result, errorFields[i].fieldcode);

        Object value;

        if (szValue == 0)
        {
            value.AttachAndIncrement(Py_None);
        }
        else
        {
            value.Attach(PyUnicode_FromString(szValue));
            if (!value)
                return 0;
        }

        if (PyObject_SetAttrString(error, errorFields[i].szAttr, value) == -1)
            return 0;
    }
    
    PyErr_SetObject(Error, error);

    return 0;
}
