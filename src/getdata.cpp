
// Note: There is code for parsing text responses, but we no longer expect
// them.

#include "pglib.h"
#include <datetime.h>
#include "getdata.h"
#include "resultset.h"
#include "row.h"
#include "byteswap.h"
#include "datatypes.h"
#include "juliandate.h"
#include "pgarrays.h"

#include "debug.h"


bool GetData_Init()
{
    PyDateTime_IMPORT;
    return true;
}

struct TempBuffer
{
    // Since use of alloca is discouraged, we'll require stack buffers be passed in.

    char* p;
    bool on_heap;

    TempBuffer(char* pStack, ssize_t cbStack, ssize_t cbNeeded)
    {
        if (cbNeeded <= cbStack)
        {
            p = pStack;
            on_heap = false;
        }
        else
        {
            p = (char*)malloc(cbNeeded);
            on_heap = true;
            if (p == 0)
                PyErr_NoMemory();
        }
    }

    ~TempBuffer()
    {
        if (p && on_heap)
            free(p);
    }
};


static PyObject* GetCash(const char* p, int format)
{
    if (format == FORMAT_TEXT)
    {
        PyErr_SetString(Error, "Invalid result: 'money' data was returned as text instead of binary.");
        return 0;
    }

    // Apparently a 64-bit integer * 100.

    int64_t n = swaps8(*(int64_t*)p);

    // Use 03 to ensure we have "x.yz".  We use a buffer big enough that we know we can insert a '.'.
    char sz[30];
    sprintf(sz, "%03lld", n);

    size_t cch = strlen(sz);
    sz[cch+1] = sz[cch];
    sz[cch]   = sz[cch-1];
    sz[cch-1] = sz[cch-2];
    sz[cch-2] = '.';

    return Decimal_FromASCII(sz);
}


static PyObject* GetNumeric(const char* p, int len, int format)
{
    if (format == FORMAT_TEXT)
    {
        PyErr_SetString(Error, "Invalid result: 'numeric' data was returned as text instead of binary.");
        return 0;
    }

    int16_t* pi = (int16_t*)p;

    int16_t ndigits = swaps2(pi[0]);
    int16_t weight  = swaps2(pi[1]);
    int16_t sign    = swaps2(pi[2]);
    int16_t dscale  = swaps2(pi[3]);

    if (sign == -16384)
        return Decimal_NaN();

    // Calculate the string length.  Each 16-bit "digit" represents 4 digits.

    int slen = (ndigits * 4) + dscale + 2; // 2 == '.' and '-'

    char szBuffer[1024];
    TempBuffer buffer(szBuffer, _countof(szBuffer), slen);
    if (buffer.p == 0)
        return 0;

    char* pch = buffer.p;

    if (sign != 0)
        *pch++ = '-';

    // Digits before decimal point.

    int iDigit = 0;

    if (weight >= 0)
    {
        bool nonzero = false;

        for (iDigit = 0; iDigit <= weight; iDigit++)
        {
            int digit = (iDigit < ndigits) ? swaps2(pi[4 + iDigit]) : 0;

            int d = digit / 1000;
            digit -= d * 1000;
            if (nonzero || d > 0)
            {
                nonzero = true;
                *pch++ = (d + '0');
            }

            d = digit / 100;
            digit -= d * 100;
            if (nonzero || d > 0)
            {
                nonzero = true;
                *pch++ = (d + '0');
            }

            d = digit / 10;
            digit -= d * 10;
            if (nonzero || d > 0)
            {
                nonzero = true;
                *pch++ = (d + '0');
            }

            d = digit;
            if (nonzero || d > 0)
            {
                nonzero = true;
                *pch++ = (d + '0');
            }
        }
    }

    // Digits after the decimal.

    if (dscale > 0)
    {
        *pch++ = '.';

        int scale = 0;
        while (scale < dscale)
        {
            int digit = (iDigit < ndigits) ? swaps2(pi[4 + iDigit]) : 0;
            iDigit++;

            int d = digit / 1000;
            digit -= d * 1000;
            *pch++ = (d + '0');
            scale += 1;

            if (scale < dscale)
            {
                d = digit / 100;
                digit -= d * 100;
                *pch++ = (d + '0');
                scale += 1;
            }

            if (scale < dscale)
            {
                d = digit / 10;
                digit -= d * 10;
                *pch++ = (d + '0');
                scale += 1;
            }

            if (scale < dscale)
            {
                d = digit;
                *pch++ = (d + '0');
                scale += 1;
            }
        }
    }

    *pch = 0;

    return Decimal_FromASCII(buffer.p);
}

static PyObject* GetDate(const char* p, int format)
{
    int year, month, date;

    if (format == FORMAT_TEXT)
    {
        // YYYY-MM-DD.
        year  = strtol(&p[0], 0, 10);
        month = strtol(&p[5], 0, 10);
        date  = strtol(&p[8], 0, 10);
    }
    else
    {
        uint32_t value = swapu4(*(uint32_t*)p) + JULIAN_START;
        julianToDate(value, year, month, date);
    }

    return PyDate_FromDate(year, month, date);
}

static PyObject* GetTime(const char* p)
{
    uint64_t value = swapu8(*(uint64_t*)p);

    int microsecond = value % 1000000;
    value /= 1000000;
    int second = value % 60;
    value /= 60;
    int minute = value % 60;
    value /= 60;
    int hour = value;

    return PyTime_FromTime(hour, minute, second, microsecond);
}

inline PyObject* GetBytes(const char* p, int len)
{
    return PyBytes_FromStringAndSize(p, len);
}

static PyObject* GetTimestamp(const char* p, bool integer_datetimes)
{
    int year, month, day, hour, minute, second, microsecond;

    if (integer_datetimes)
    {
        // Number of milliseconds since the Postgres epoch.

        uint64_t n = swapu8(*(uint64_t*)p);

        microsecond = n % 1000000;
        n /= 1000000;
        second = n % 60;
        n /= 60;
        minute = n % 60;
        n /= 60;
        hour = n % 24;
        n /= 24;
        int days = n;

        julianToDate(days + JULIAN_START, year, month, day);
    }
    else
    {
        // 8-byte floating point
        PyErr_SetString(Error, "Floating unhandled!\n");
        return 0;
    }

    return PyDateTime_FromDateAndTime(year, month, day, hour, minute, second, microsecond);
}

PyObject* ConvertValue(PGresult* result, int iRow, int iCol, bool integer_datetimes, int format)
{
    // Used to read a column from the database and return a Python object.

    if (PQgetisnull(result, iRow, iCol))
        Py_RETURN_NONE;

    Oid oid = PQftype(result, iCol);

    const char* p = PQgetvalue(result, iRow, iCol);

    // printf("OID: %d -> %s\n", oid, (format == 0) ? p : "(binary)");

    switch (oid)
    {
    case TEXTOID:
    case BPCHAROID:
    case VARCHAROID:
        return PyUnicode_DecodeUTF8((const char*)p, strlen((const char*)p), 0);

    case BYTEAOID:
        return GetBytes(p, PQgetlength(result, iRow, iCol));

    case INT2OID:
        return (format == FORMAT_TEXT) ? PyLong_FromString(p, 0, 10) : PyLong_FromLong(swaps2(*(int16_t*)p));

    case INT4OID:
        return (format == FORMAT_TEXT) ? PyLong_FromString(p, 0, 10) : PyLong_FromLong(swaps4(*(int32_t*)p));

    case INT8OID:
        return (format == FORMAT_TEXT) ? PyLong_FromString(p, 0, 10) : PyLong_FromLongLong(swaps8(*(int64_t*)p));

    case NUMERICOID:
        return GetNumeric(p, PQgetlength(result, iRow, iCol), format);

    case CASHOID:
        return GetCash(p, format);

    case DATEOID:
        return GetDate(p, format);

    case TIMEOID:
        return GetTime(p);

    case FLOAT4OID:
        return PyFloat_FromDouble((format == FORMAT_TEXT) ? strtod(p, 0) : swapfloat(*(float*)p));

    case FLOAT8OID:
        return PyFloat_FromDouble((format == FORMAT_TEXT) ? strtod(p, 0) : swapdouble(*(double*)p));

    case TIMESTAMPOID:
        return GetTimestamp(p, integer_datetimes);

    case BOOLOID:
        // If format is text, we'll get 't' and 'f'.
        return PyBool_FromLong((format == FORMAT_TEXT) ? (*p == 't') : *p);

    case UUIDOID:
        return UUID_FromBytes(p);

    case INT4ARRAYOID:
        return GetInt4Array(p);

    case INT8ARRAYOID:
        return GetInt8Array(p);

    case TEXTARRAYOID:
        return GetTextArray(p);
    }

    // printf("OID: %d\n", oid);

    // I'm now going to return all unknown types as bytes.  This allows users to
    // potentially workaround missing types until I can add them.
    return GetBytes(p, PQgetlength(result, iRow, iCol));
}
