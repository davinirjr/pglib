
// Sending and receiving arrays.  Right now we only support strings and integers, but there is
// no reason we can't add more types.
//
// I'm not sure what to do when the array is empty or all NULLs.  I'm sending as TEXT but this
// can't be written into an int[] column.
//
// Array Wire Format
// -----------------
//
// The format was determined from array_recv in src/backend/utils/adt/arrayfuncs.c
//
// The array starts with a header which I've created a struct for below: ArrayHeader.
//
// Each item is written after the header.  Each starts with a 4-byte length.  If the item is
// NULL, set the length to -1 and do not write any data.  Otherwise write the length followed
// by the data.  There does not appear to be any padding after elements (possibly because the
// header is already 32-bit aligned).
//
// Remember that all integers are in network order, so use ntohl and htonl appropriately.  All
// OSs don't seem to have an 8-byte version of this, so there are some equivalent macros in
// byteswap.h

#include "pglib.h"
#include "connection.h"
#include "params.h"
#include "errors.h"
#include "debug.h"
#include "byteswap.h"

struct ArrayHeader
{
    uint32_t ndim;   // number of dimensions - we support 1
    uint32_t flags;  // binary format? 0 or 1, but unused by the server?
    Oid oid;         // type of elements
    uint32_t dim;    // length of array
    uint32_t lbound; // lower bound - set to 1
    char buffer[0];
};

static PyObject* FindFirstParam(Object& seq)
{
    // Returns a *borrowed* reference to the first non-None parameter in the sequence.  Returns
    // zero if the list is empty or all are None.

    Py_ssize_t cItems = PySequence_Length(seq);
    for (int i = 0; i < cItems; i++)
    {
        PyObject* item = PySequence_Fast_GET_ITEM(seq.Get(), i);
        if (item != Py_None)
            return item;
    }
    return 0;
}

bool BindUnicodeArray(Params& params, Object& seq, Py_ssize_t cItems)
{
    // First loop through the array and add up the memory we'll need.  This isn't quite as
    // inefficient as it looks since the string objects will cache the UTF8 version.

    Py_ssize_t cb = sizeof(ArrayHeader);
    for (Py_ssize_t iItem = 0; iItem < cItems; iItem++)
    {
        PyObject* item = PySequence_Fast_GET_ITEM(seq.Get(), iItem);
        cb += 4;                // length indicator
        if (item != Py_None)
        {
            if (!PyUnicode_Check(item))
            {
                SetStringError(Error, "array parameters elements must all be the same type");
                return false;
            }

            Py_ssize_t len = 0;
            if (!PyUnicode_AsUTF8AndSize(item, &len))
                return false;
            cb += len;
        }
    }

    char* p = params.Allocate(cb);
    if (!p)
        return false;

    ArrayHeader* phdr = (ArrayHeader*)p;
    phdr->ndim   = htonl(1);
    phdr->flags  = htonl(1);
    phdr->oid    = htonl(TEXTOID);
    phdr->dim    = htonl(cItems);
    phdr->lbound = htonl(1);

    char* pT = &phdr->buffer[0];

    for (Py_ssize_t iItem = 0; iItem < cItems; iItem++)
    {
        PyObject* item = PySequence_Fast_GET_ITEM(seq.Get(), iItem);
        if (item != Py_None)
        {
            Py_ssize_t len;
            const char* sz = PyUnicode_AsUTF8AndSize(item, &len);
            if (!sz)
                return false;

            (*(uint32_t*)pT) = htonl(len);
            pT += 4;

            memcpy(pT, sz, len);
            pT += len;
        }
        else
        {
            (*(uint32_t*)pT) = htonl(-1);
            pT += 4;
        }
    }

    return params.Bind(TEXTARRAYOID, p, cb, FORMAT_BINARY);
}

const long MIN_SMALLINT = -32768;
const long MAX_SMALLINT = 32767;
const long MIN_INTEGER  = -2147483648;
const long MAX_INTEGER  = 2147483647;

inline int MinLongSize(PyObject* item)
{
    // Returns the number of bytes required to hold this value.  Will return 2, 4, or 8.

    int overflow;
    PY_LONG_LONG lvalue = PyLong_AsLongLongAndOverflow(item, &overflow);
    if (overflow != 0)
        return 0;

    if (lvalue < MIN_INTEGER || lvalue > MAX_INTEGER)
        return 8;
    if (lvalue < MIN_SMALLINT || lvalue > MAX_SMALLINT)
        return 4;
    return 2;
}

static const Oid MAP_INTSIZE_TO_OID[] = { 0, 0, INT2OID, 0, INT4OID, 0, 0, 0, INT8OID };
static const Oid MAP_INTSIZE_TO_ARRAYOID[] = { 0, 0, INT2ARRAYOID, 0, INT4ARRAYOID, 0, 0, 0, INT8ARRAYOID };


bool BindLongArray(Params& params, Object& seq, Py_ssize_t cItems)
{
    // Loop through the array to determine the size of integers we'll need (we send the
    // smallest possible) and how many NULLs there are so we can determine the memory needed.

    int cNonNull = 0;           // count of non-null items
    int cbItem  = 0;            // byte-size of each item

    for (Py_ssize_t iItem = 0; iItem < cItems; iItem++)
    {
        PyObject* item = PySequence_Fast_GET_ITEM(seq.Get(), iItem);
        if (item != Py_None)
        {
            if (!PyLong_Check(item))
            {
                SetStringError(Error, "array parameters elements must all be the same type");
                return false;
            }

            cNonNull += 1;
            cbItem = MAX(cbItem, MinLongSize(item));
        }
    }

    Py_ssize_t cb = sizeof(ArrayHeader) +
        (4 * cItems) +          // length indicators
        (cbItem * cNonNull);    // values

    char* p = params.Allocate(cb);
    if (!p)
        return false;

    ArrayHeader* phdr = (ArrayHeader*)p;
    phdr->ndim   = htonl(1);
    phdr->flags  = htonl(1);
    phdr->oid    = htonl(MAP_INTSIZE_TO_OID[cbItem]);
    phdr->dim    = htonl(cItems);
    phdr->lbound = htonl(1);

    char* pT = &phdr->buffer[0];

    for (Py_ssize_t iItem = 0; iItem < cItems; iItem++)
    {
        PyObject* item = PySequence_Fast_GET_ITEM(seq.Get(), iItem);
        if (item != Py_None)
        {
            (*(uint32_t*)pT) = htonl(cbItem); // could be swapped and cached
            pT += 4;

            PY_LONG_LONG lvalue = PyLong_AsLongLong(item);

            if (cbItem == 2)
            {
                (*(uint16_t*)pT) = htons((uint16_t)lvalue);
            }
            else if (cbItem == 4)
            {
                (*(uint32_t*)pT) = htonl((uint32_t)lvalue);
            }
            else
            {
                (*(uint64_t*)pT) = swapu8((uint64_t)lvalue);
            }

            pT += cbItem;
        }
        else
        {
            (*(uint32_t*)pT) = htonl(-1);
            pT += 4;
        }
    }

    return params.Bind(MAP_INTSIZE_TO_ARRAYOID[cbItem], p, cb, FORMAT_BINARY);
}


bool BindArray(Params& params, PyObject* param)
{
    // Binds a list or tuple as an array.  All elements must be of the same type, though None
    // (NULL) is supported.

    Object seq = PySequence_Fast(param, "a list or tuple is required");
    if (!seq)
        return 0;

    Py_ssize_t cItems = PySequence_Length(param);

    // Figure out what kind of elements are in the array.  Find the first non-None item.

    PyObject* first = FindFirstParam(seq);

    if (!first || PyUnicode_Check(first))
        return BindUnicodeArray(params, seq, cItems);

    if (PyLong_Check(first))
        return BindLongArray(params, seq, cItems);

    SetStringError(Error, "Unhandled type in parameter array");

    return false;
}

PyObject* GetInt4Array(const char* p)
{
    // Reads an INT4ARRAYOID array result and returns a list of integers.

    const ArrayHeader* phdr = (const ArrayHeader*)p;

    if (ntohl(phdr->ndim) != 1)
        return SetStringError(Error, "pglib can only read single dimensional arrays");

    Py_ssize_t dim = ntohl(phdr->dim);
    Object list(PyList_New(dim));
    if (!list)
        return 0;

    if (dim > 0)
    {
        int32_t* pT = (int32_t*)&phdr->buffer[0];

        for (int i = 0; i < dim; i++)
        {
            int32_t len = ntohl(*pT++);
            if (len == -1)
            {
                Py_INCREF(Py_None);
                PyList_SET_ITEM(list.Get(), i, Py_None);
            }
            else
            {
                int32_t val = ntohl(*pT++);
                PyObject* l = PyLong_FromLong(val);
                if (!l)
                    return 0;
                PyList_SET_ITEM(list.Get(), i, l);
            }
        }
    }

    return list.Detach();
}

PyObject* GetInt8Array(const char* p)
{
    // Reads an INT4ARRAYOID array result and returns a list of integers.

    const ArrayHeader* phdr = (const ArrayHeader*)p;

    if (ntohl(phdr->ndim) != 1)
        return SetStringError(Error, "pglib can only read single dimensional arrays");

    Py_ssize_t dim = ntohl(phdr->dim);
    Object list(PyList_New(dim));
    if (!list)
        return 0;

    if (dim > 0)
    {
        const char* pT = &phdr->buffer[0];

        for (int i = 0; i < dim; i++)
        {
            int32_t len = ntohl(*(int32_t*)pT);
            pT += 4;

            if (len == -1)
            {
                Py_INCREF(Py_None);
                PyList_SET_ITEM(list.Get(), i, Py_None);
            }
            else
            {
                int64_t val = swapu8(*(uint64_t*)pT);
                pT += 8;
                PyObject* l = PyLong_FromLongLong(val);
                if (!l)
                    return 0;
                PyList_SET_ITEM(list.Get(), i, l);
            }
        }
    }

    return list.Detach();
}


PyObject* GetTextArray(const char* p)
{
    const ArrayHeader* phdr = (const ArrayHeader*)p;

    if (ntohl(phdr->ndim) != 1)
        return SetStringError(Error, "pglib can only read single dimensional arrays");

    Py_ssize_t dim = ntohl(phdr->dim);
    Object list(PyList_New(dim));
    if (!list)
        return 0;

    if (dim > 0)
    {
        const char* pT = &phdr->buffer[0];

        for (int i = 0; i < dim; i++)
        {
            int32_t len = ntohl(*(uint32_t*)pT);
            pT += 4;

            if (len == -1)
            {
                Py_INCREF(Py_None);
                PyList_SET_ITEM(list.Get(), i, Py_None);
            }
            else
            {
                PyObject* str = PyUnicode_DecodeUTF8(pT, len, "strict");
                if (!str)
                    return 0;
                pT += len;
                PyList_SET_ITEM(list.Get(), i, str);
            }
        }
    }

    return list.Detach();
}
