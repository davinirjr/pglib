
#include "pglib.h"
#include "getdata.h"
#include "resultset.h"
#include "row.h"
#include "byteswap.h"
#include "decimal.h"

#define NBASE       10000
#define HALF_NBASE  5000
#define DEC_DIGITS  4           /* decimal digits per NBASE digit */

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


static PyObject* NumericToDecimal(const char* p, int len)
{
    // Converts a PostgreSQL numeric column to a Python decimal.Decimal object.

    int16_t* pi = (int16_t*)p;

    int16_t ndigits = signed_ntohs(pi[0]);
    int16_t weight  = signed_ntohs(pi[1]);
    int16_t sign    = signed_ntohs(pi[2]);
    int16_t dscale  = signed_ntohs(pi[3]);
    const char* digits = &p[8];

    /*
    printf("ndigits = %d\n", ndigits);
    printf("weight  = %d\n", weight );
    printf("sign    = %d\n", sign   );
    printf("dscale  = %d\n", dscale );
    */

    if (sign == -16384)
    {
        return Decimal_NaN();
    }

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
            int digit = (iDigit < ndigits) ? signed_ntohs(pi[4 + iDigit]) : 0;

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
            int digit = (iDigit < ndigits) ? signed_ntohs(pi[4 + iDigit]) : 0;
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

static void DumpBytes(const char* p, int len)
{
    printf("len=%d\n", len);
    for (int i = 0; i < len; i++)
    {
        if (i > 0 && (i % 10) == 0)
            printf("\n");
        printf("%02x", *(unsigned char*)&p[i]);
    }
    printf("\n");
}


PyObject* ConvertValue(PGresult* result, int iRow, int iCol)
{
    if (PQgetisnull(result, iRow, iCol))
        Py_RETURN_NONE;

    int format = PQfformat(result, iCol);
    Oid oid = PQftype(result, iCol);

    // printf("ConvertValue: col=%d fmt=%d oid=%d\n", iCol, format, (int)oid);

    const char* p = PQgetvalue(result, iRow, iCol);

    switch (oid)
    {
    case INT2OID:
        if (format == 0)
            return PyLong_FromString((char*)p, 0, 10);
        return PyLong_FromLong(signed_ntohs(*(int16_t*)p));

    case INT4OID:
        if (format == 0)
            return PyLong_FromString((char*)p, 0, 10);
        return PyLong_FromLong(signed_ntohl(*(long*)p));

    case INT8OID:
        if (format == 0)
            return PyLong_FromString((char*)p, 0, 10);
        return PyLong_FromLongLong(signed_ntohll(*(long long*)p));

    case NUMERICOID:
        if (format == 0)
            return PyLong_FromString((char*)p, 0, 10);
        return NumericToDecimal(p, PQgetlength(result, iRow, iCol));

    case TEXTOID:
    case BPCHAROID:
    case VARCHAROID:
        return PyUnicode_DecodeUTF8((const char*)p, strlen((const char*)p), 0);
    }

    return PyErr_Format(Error, "ConvertValue: Unhandled OID %d\n", (int)oid);
}


