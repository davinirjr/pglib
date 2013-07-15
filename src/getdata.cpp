
#include "pglib.h"
#include <datetime.h>
#include "getdata.h"
#include "resultset.h"
#include "row.h"
#include "byteswap.h"
#include "decimal.h"
#include "juliandate.h"

#include "debug.h"

static PyObject* GetCash(const char* p);
static PyObject* GetDate(const char* p);
static PyObject* GetTime(const char* p);
static PyObject* GetFloat4(const char* p);
static PyObject* GetFloat8(const char* p);
static PyObject* GetNumeric(const char* p, int len);
static PyObject* GetTimestamp(const char* p, bool integer_datetimes);

bool GetData_init()
{
    PyDateTime_IMPORT;
    return true;
}

PyObject* ConvertValue(PGresult* result, int iRow, int iCol, bool integer_datetimes)
{
    // Used to read a column from the database and return a Python object.

    if (PQgetisnull(result, iRow, iCol))
        Py_RETURN_NONE;

    // int format = PQfformat(result, iCol);
    Oid oid = PQftype(result, iCol);

    // printf("ConvertValue: col=%d fmt=%d oid=%d\n", iCol, format, (int)oid);

    const char* p = PQgetvalue(result, iRow, iCol);


    switch (oid)
    {
    case TEXTOID:
    case BPCHAROID:
    case VARCHAROID:
        return PyUnicode_DecodeUTF8((const char*)p, strlen((const char*)p), 0);

    case INT2OID:
        return PyLong_FromLong(swaps2(*(int16_t*)p));

    case INT4OID:
        return PyLong_FromLong(swaps4(*(int32_t*)p));

    case INT8OID:
        return PyLong_FromLongLong(swaps8(*(int64_t*)p));

    case NUMERICOID:
        return GetNumeric(p, PQgetlength(result, iRow, iCol));

    case CASHOID:
        return GetCash(p);

    case DATEOID:
        return GetDate(p);

    case TIMEOID:
        return GetTime(p);

    case FLOAT4OID:
        return GetFloat4(p);

    case FLOAT8OID:
        return GetFloat8(p);

    case TIMESTAMPOID:
        return GetTimestamp(p, integer_datetimes);

    case BOOLOID:
        return PyBool_FromLong(*p);
    }

    return PyErr_Format(Error, "ConvertValue: Unhandled OID %d\n", (int)oid);
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


static PyObject* GetCash(const char* p)
{
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


static PyObject* GetNumeric(const char* p, int len)
{
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

static PyObject* GetFloat4(const char* p)
{
    return PyFloat_FromDouble(swapfloat(*(float*)p));
}

static PyObject* GetFloat8(const char* p)
{
    return PyFloat_FromDouble(swapdouble(*(double*)p));
}

static PyObject* GetDate(const char* p)
{
    uint32_t value = swapu4(*(uint32_t*)p) + JULIAN_START;
    int year, month, date;
    julianToDate(value, year, month, date);
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
