
#include "pglib.h"
#include <datetime.h>
#include "connection.h"
#include "params.h"
#include "datatypes.h"
#include "juliandate.h"
#include "byteswap.h"

#ifdef MS_WINDOWS
#include <Winsock2.h>
#else
#ifndef __APPLE__
#include <arpa/inet.h>
#endif
#endif

static bool BindBool(Connection* cnxn, Params& params, PyObject* param);
static bool BindByteArray(Connection* cnxn, Params& params, PyObject* param);
static bool BindBytes(Connection* cnxn, Params& params, PyObject* param);
static bool BindDate(Connection* cnxn, Params& params, PyObject* param);
static bool BindDateTime(Connection* cnxn, Params& params, PyObject* param);
static bool BindDecimal(Connection* cnxn, Params& params, PyObject* param);
static bool BindFloat(Connection* cnxn, Params& params, PyObject* param);
static bool BindLong(Connection* cnxn, Params& params, PyObject* param);
static bool BindNone(Connection* cnxn, Params& params, PyObject* param);
static bool BindTime(Connection* cnxn, Params& params, PyObject* param);
static bool BindUnicode(Connection* cnxn, Params& params, PyObject* param);
static bool BindUUID(Connection* cnxn, Params& params, PyObject* param);

void Params_Init()
{
    PyDateTime_IMPORT;
};

struct Pool
{
    Pool* next;
    size_t total;
    size_t remaining;
    char buffer[1];
};

void Dump(Params& params)
{
    printf("===============\n");
    printf("pools\n");
    int count = 0;
    Pool* p = (Pool*)params.pool;
    while (p != 0)
    {
        count += 1;
        printf(" [ 0x%p total=%d remaining=%d ]\n", p, (int)p->total, (int)p->remaining);
        p = p->next;
    }
    printf("---------------\n");
}

Params::Params(int _count)
{
    count = _count;
    bound = 0;
    
    if (count == 0)
    {
        types   = 0;
        values  = 0;
        lengths = 0;
        formats = 0;
    }
    else
    {
        types   = (Oid*)  malloc(count * sizeof(Oid));
        values  = (const char**)malloc(count * sizeof(char*));
        lengths = (int*)  malloc(count * sizeof(int));
        formats = (int*)  malloc(count * sizeof(int));
    }
    pool = 0;
}

Params::~Params()
{
    free(types);
    free(values);
    free(lengths);
    free(formats);

    Pool* p = pool;
    while (p)
    {
        Pool* tmp = p->next;
        free(p);
        p = tmp;
    }
}

bool Params::Bind(Oid type, const void* value, int length, int format)
{
    types[bound]   = type;
    values[bound]  = (const char*)value;
    lengths[bound] = length;
    formats[bound] = format;

    bound += 1;

    return true;
}

char* Params::Allocate(size_t amount)
{
    // See if we have a pool that is large enough.

    Pool** pp = &pool;

    while (*pp != 0)
    {
        if ((*pp)->remaining >= amount)
            break;

        pp = &((*pp)->next);
    }

    // If we didn't find a large enough pool, make one and link it in.
    
    if (*pp == 0)
    {
        size_t total = amount + 1024;
        *pp = (Pool*)malloc(total);

        if (*pp == 0)
        {
            PyErr_NoMemory();
            return 0;
        }

        (*pp)->next = 0;
        (*pp)->total = (*pp)->remaining = total;
    }
    
    // Reserve the area and return it.

    size_t offset = (*pp)->total - (*pp)->remaining;
    char*  p      = &(*pp)->buffer[offset];
    (*pp)->remaining -= amount;

    return p;
}

bool BindParams(Connection* cnxn, Params& params, PyObject* args)
{
    // Binds arguments 1-n.  Argument zero is expected to be the SQL statement itself.

    if (params.count == 0)
        return true;

    if (!params.valid())
    {
        PyErr_NoMemory();
        return false;
    }

    for (int i = 0, c = PyTuple_GET_SIZE(args)-1; i < c; i++)
    {
        // Remember that a bool is a long, a datetime is a date, etc, so the order we check them in is important.

        PyObject* param = PyTuple_GET_ITEM(args, i+1);
        if (param == Py_None)
        {
            if (!BindNone(cnxn, params, param))
                return false;
        }
        else if (PyBool_Check(param))
        {
            if (!BindBool(cnxn, params, param))
                return false;
        }
        else if (PyLong_Check(param))
        {
            if (!BindLong(cnxn, params, param))
                return false;
        }
        else if (PyUnicode_Check(param))
        {
            if (!BindUnicode(cnxn, params, param))
                return false;
        }
        else if (Decimal_Check(param))
        {
            if (!BindDecimal(cnxn, params, param))
                return false;
        }
        else if (PyDateTime_Check(param))
        {
            if (!BindDateTime(cnxn, params, param))
                return false;
        }
        else if (PyDate_Check(param))
        {
            if (!BindDate(cnxn, params, param))
                return false;
        }
        else if (PyTime_Check(param))
        {
            if (!BindTime(cnxn, params, param))
                return false;
        }
        else if (PyFloat_Check(param))
        {
            if (!BindFloat(cnxn, params, param))
                return false;
        }
        else if (PyBytes_Check(param))
        {
            if (!BindBytes(cnxn, params, param))
                return false;
        }
        else if (PyByteArray_Check(param))
        {
            if (!BindByteArray(cnxn, params, param))
                return false;
        }
        else if (UUID_Check(param))
        {
            if (!BindUUID(cnxn, params, param))
                return false;
        }
        else
        {
            PyErr_Format(Error, "Unable to bind parameter %d: unhandled object type %R", (i+1), param);
            return false;
        }
    }

    return true;
}

static bool BindUnicode(Connection* cnxn, Params& params, PyObject* param)
{
    // TODO: Right now we *require* the encoding to be UTF-8.

    Py_ssize_t cb;
    const char* p = PyUnicode_AsUTF8AndSize(param, &cb);
    if (p == 0)
        return false;

    return params.Bind(TEXTOID, p, cb, 0);
}

static bool BindDecimal(Connection* cnxn, Params& params, PyObject* param)
{
    // TODO: How are we going to deal with NaN, etc.?

    // This is probably wasteful, but most decimals are probably small as strings and near the size of the binary
    // format.
    //
    // We are going to allocate as UTF8 but we fully expect the results to be ASCII.
    
    Object s(PyObject_Str(param));
    if (!s)
        return false;

    Py_ssize_t cch;
    const char* pch = PyUnicode_AsUTF8AndSize(s, &cch);
    char* p = params.Allocate(cch + 1);
    if (p == 0)
        return 0;

    strcpy(p, pch);

    return params.Bind(NUMERICOID, p, cch + 1, 0);
}


static bool BindLong(Connection* cnxn, Params& params, PyObject* param)
{
    // Note: Integers must be in network order.

    const long    MIN_SMALLINT = -32768; 
    const long    MAX_SMALLINT = 32767; 
    const long    MIN_INTEGER  = -2147483647; // actually -2147483648, but generates warnings
    const long    MAX_INTEGER  = 2147483647;
    // const int64_t MIN_BIGINT   = -9223372036854775807LL; // -9223372036854775808LL actually
    // const int64_t MAX_BIGINT   = 9223372036854775807LL;

    // Try a 32-bit integer.

    int overflow = 0;

    long lvalue = PyLong_AsLongAndOverflow(param, &overflow);

    if (overflow == 0)
    {
        if (MIN_SMALLINT <= lvalue && lvalue <= MAX_SMALLINT)
        {
            int16_t* p = reinterpret_cast<int16_t*>(params.Allocate(2));
            if (p == 0)
                return false;
            *p = htons(lvalue);
            return params.Bind(INT2OID, p, 2, 1); // 2=16 bit, 1=binary
        }

        if (MIN_INTEGER <= lvalue && lvalue <= MAX_INTEGER)
        {
            int32_t* p = reinterpret_cast<int32_t*>(params.Allocate(4));
            if (p == 0)
                return false;
            *p = htonl(lvalue);
            return params.Bind(INT4OID, p, 4, 1); // 2=16 bit, 1=binary
        }
    }
    
    /* Commenting out since I don't have a portable htonll
    // Now try 64-bit

    PY_LONG_LONG llvalue = PyLong_AsLongLongAndOverflow(param, &overflow);
    if (overflow == 0 && llvalue >= MIN_BIGINT && llvalue <= MAX_BIGINT)
    {
        int64_t* p = reinterpret_cast<int64_t*>(Allocate(params, 8));
        *p = static_cast<int64_t>(value);
        if (p == 0)
            return false;
        return true;
    }
    */

    // At this point fall back to binding as a string.  (Normal string binding binds directly into the parameter, but
    // I'll copy for now.  Perhaps I should pool Python objects too, depending on many object types I eventually
    // convert to Python strings.)

    Object str(PyObject_Str(param));
    if (!str)
        return false;
    Py_ssize_t cb = 0;
    const char* sz = PyUnicode_AsUTF8AndSize(str, &cb);
    char* pch = params.Allocate(cb + 1);
    if (!pch)
        return 0;
    memcpy(pch, sz, cb+1);
    return params.Bind(NUMERICOID, pch, cb+1, 0);
}


static const char FALSEBYTE = 0;
static const char TRUEBYTE  = 1;

static bool BindBool(Connection* cnxn, Params& params, PyObject* param)
{
    const char* p = (param == Py_True) ? &TRUEBYTE : &FALSEBYTE;
    return params.Bind(BOOLOID, p, 1, 1);
}

static bool BindDate(Connection* cnxn, Params& params, PyObject* param)
{
    uint32_t julian = dateToJulian(PyDateTime_GET_YEAR(param), PyDateTime_GET_MONTH(param), PyDateTime_GET_DAY(param));
    julian -= JULIAN_START;

    uint32_t* p = (uint32_t*)params.Allocate(sizeof(julian));
    if (p == 0)
        return false;

    *p = swapu4(julian);
    params.Bind(DATEOID, p, 4, 1);
    return true;
}

static bool BindDateTime(Connection* cnxn, Params& params, PyObject* param)
{
    uint64_t timestamp = dateToJulian(PyDateTime_GET_YEAR(param), PyDateTime_GET_MONTH(param), PyDateTime_GET_DAY(param)) - JULIAN_START;
    timestamp *= 24;
    timestamp += PyDateTime_DATE_GET_HOUR(param);
    timestamp *= 60;
    timestamp += PyDateTime_DATE_GET_MINUTE(param);
    timestamp *= 60;
    timestamp += PyDateTime_DATE_GET_SECOND(param);
    timestamp *= 1000000;
    timestamp += PyDateTime_DATE_GET_MICROSECOND(param);

    uint64_t* p = (uint64_t*)params.Allocate(8);
    if (p == 0)
        return false;

    *p = swapu8(timestamp);

    params.Bind(TIMESTAMPOID, p, 8, 1);
    return true;
}

static bool BindNone(Connection* cnxn, Params& params, PyObject* param)
{
    params.Bind(0,0,0,0);
    return true;
}

static bool BindTime(Connection* cnxn, Params& params, PyObject* param)
{
    uint64_t value = PyDateTime_TIME_GET_HOUR(param);
    value *= 60;
    value += PyDateTime_TIME_GET_MINUTE(param);
    value *= 60;
    value += PyDateTime_TIME_GET_SECOND(param);
    value *= 1000000;
    value += PyDateTime_TIME_GET_MICROSECOND(param);

    uint64_t* p = (uint64_t*)params.Allocate(8);
    if (p == 0)
        return false;

    *p = swapu8(value);

    params.Bind(TIMEOID, p, 8, 1);
    return true;
}

static bool BindBytes(Connection* cnxn, Params& params, PyObject* param)
{
    char* p = PyBytes_AS_STRING(param);
    Py_ssize_t cb = PyBytes_GET_SIZE(param);
    params.Bind(BYTEAOID, p, cb, 1);
    return true;
}


static bool BindByteArray(Connection* cnxn, Params& params, PyObject* param)
{
    char* p = PyByteArray_AS_STRING(param);
    Py_ssize_t cb = PyByteArray_GET_SIZE(param);
    params.Bind(BYTEAOID, p, cb, 1);
    return true;
}


static bool BindFloat(Connection* cnxn, Params& params, PyObject* param)
{
    double value = PyFloat_AS_DOUBLE(param);
    
    double* p = (double*)params.Allocate(8);
    if (p == 0)
        return false;
    
    *p = swapdouble(value);

    params.Bind(FLOAT8OID, p, 8, 1);
    return true;
}

static bool BindUUID(Connection* cnxn, Params& params, PyObject* param)
{
    Object bytes(PyObject_GetAttrString(param, "bytes"));
    if (!bytes)
        return false;

    Py_ssize_t cch = PyBytes_GET_SIZE(bytes.Get());
    char* pch = params.Allocate(cch);
    if (!pch)
        return 0;
    memcpy(pch, PyBytes_AS_STRING(bytes.Get()), cch);

    return params.Bind(UUIDOID, pch, cch, 1);
}
