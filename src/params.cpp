
#include "pglib.h"
#include "connection.h"
#include "params.h"

struct Pool
{
    Pool* next;
    size_t total;
    size_t remaining;
    char buffer[1];
};

void Dump(Params& params)
{
    // printf("===============\n");
    // printf("pools\n");
    int count = 0;
    Pool* p = (Pool*)params.pool;
    while (p != 0)
    {
        count += 1;
        // printf(" [ 0x%p total=%d remaining=%d ]\n", p, (int)p->total, (int)p->remaining);
        // // printf(" [ 0x%p total=%d remaining=%d ]\n", p, (int)p->total, (int)p->remaining);
        p = p->next;
    }
    // printf("---------------\n");
}

Params::Params(int _count)
{
    count = _count;
    bound = 0;
    
    types   = (Oid*)  malloc(count * sizeof(Oid));
    values  = (char**)malloc(count * sizeof(char*));
    lengths = (int*)  malloc(count * sizeof(int));
    formats = (int*)  malloc(count * sizeof(int));

    pool = 0;
}

Params::~Params()
{
    free(types);
    free(values);
    free(lengths);
    free(formats);

    Pool* p = reinterpret_cast<Pool*>(pool);
    while (p)
    {
        Pool* tmp = p->next;
        free(p);
        p = tmp;
    }
}

bool Params::Bind(Oid type, char* value, int length, int format)
{
    types[bound]   = type;
    values[bound]  = value;
    lengths[bound] = length;
    formats[bound] = format;

    bound += 1;

    return true;
}

char* Allocate(Params& params, size_t amount)
{
    Pool** pp = reinterpret_cast<Pool**>(&params.pool);

    // printf("pool: %p\n", params.pool);
    // printf("pp:   %p\n", pp);
    // printf("*pp:  %p\n", *pp);

    // See if we have a pool that is large enough.

    while (*pp != 0)
    {
        if ((*pp)->remaining >= amount)
            break;
    }

    // If we didn't find a large enough pool, make one and link it in.
    
    if (*pp == 0)
    {
        /*
        // Now decide on the size.  Let's round the request up to the nearest 2K.
        const size_t multiple = 1024 * 2;
        size_t total = ((amount-1) / multiple) * multiple;
        */
        size_t total = amount + 1024;
        *pp = reinterpret_cast<Pool*>(malloc(total));

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

static const char FALSEBYTE = 1;
static const char TRUEBYTE = 1;

bool BindBool(Connection* cnxn, Params& params, PyObject* param)
{
    const char* p = (param == Py_True) ? &TRUEBYTE : &FALSEBYTE;
    return params.Bind(BOOLOID, (char*)p, 1, 1);
}

bool BindUnicode(Connection* cnxn, Params& params, PyObject* param)
{
    // TODO: Right now we *require* the encoding to be UTF-8.

    Py_ssize_t cb;
    const char* p = PyUnicode_AsUTF8AndSize(param, &cb);
    if (p == 0)
        return false;

    return params.Bind(TEXTOID, (char*)p, cb, 0);
}

bool BindLong(Connection* cnxn, Params& params, PyObject* param)
{
    // Note: Integers must be in network order.

    const long    MIN_SMALLINT = -32768; 
    const long    MAX_SMALLINT = 32767; 
    const long    MIN_INTEGER  = -2147483647; // actually -2147483648, but generates warnings
    const long    MAX_INTEGER  = 2147483647;
    const int64_t MIN_BIGINT   = -9223372036854775807LL; // -9223372036854775808LL actually
    const int64_t MAX_BIGINT   = 9223372036854775807LL;

    // Try a 32-bit integer.

    int overflow = 0;

    long lvalue = PyLong_AsLongAndOverflow(param, &overflow);

    if (overflow == 0)
    {
        if (MIN_SMALLINT <= lvalue && lvalue <= MAX_SMALLINT)
        {
            int16_t* p = reinterpret_cast<int16_t*>(Allocate(params, 2));
            if (p == 0)
                return false;
            *p = htons(lvalue);
            return params.Bind(INT2OID, (char*)p, 2, 1); // 2=16 bit, 1=binary
        }

        if (MIN_INTEGER <= lvalue && lvalue <= MAX_INTEGER)
        {
            int32_t* p = reinterpret_cast<int32_t*>(Allocate(params, 4));
            if (p == 0)
                return false;
            *p = htonl(lvalue);
            return params.Bind(INT4OID, (char*)p, 4, 1); // 2=16 bit, 1=binary
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
    char* pch = Allocate(params, cb + 1);
    if (!pch)
        return 0;
    memcpy(pch, sz, cb+1);
    return params.Bind(NUMERICOID, pch, cb+1, 0);
}


bool BindParams(Connection* cnxn, Params& params, PyObject* args)
{
    // Binds arguments 1-n.  Argument zero is expected to be the SQL statement itself.

    if (!params.valid())
    {
        PyErr_NoMemory();
        return false;
    }

    for (int i = 0, c = PyTuple_GET_SIZE(args)-1; i < c; i++)
    {
        // printf("parameter %d\n", i+1);
        Dump(params);

        PyObject* param = PyTuple_GET_ITEM(args, i+1);
        if (param == Py_None)
        {
            params.types[i]   = 0;
            params.values[i]  = 0;
            params.lengths[i] = 0;
            params.formats[i] = 0;
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
        else if (PyBool_Check(param))
        {
            if (!BindBool(cnxn, params, param))
                return false;
        }
        
        /*
    if (PyBytes_Check(param))
        return GetBytesInfo(cur, index, param, info);

    if (PyDateTime_Check(param))
        return GetDateTimeInfo(cur, index, param, info);

    if (PyDate_Check(param))
        return GetDateInfo(cur, index, param, info);

    if (PyTime_Check(param))
        return GetTimeInfo(cur, index, param, info);

    if (PyLong_Check(param))
        return GetLongInfo(cur, index, param, info);

    if (PyFloat_Check(param))
        return GetFloatInfo(cur, index, param, info);

    if (PyDecimal_Check(param))
        return GetDecimalInfo(cur, index, param, info);

#if PY_VERSION_HEX >= 0x02060000
    if (PyByteArray_Check(param))
        return GetByteArrayInfo(cur, index, param, info);
#endif
         */
        else
        {
            PyErr_Format(Error, "Unable to bind parameter %d: unhandled object type %s", (i+1), param->ob_type->tp_name);
            return false;
        }
    }

    // printf("after\n");
    Dump(params);

    return true;
}
