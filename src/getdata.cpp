
#include "pglib.h"
#include "getdata.h"
#include "resultset.h"
#include "row.h"

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
    case INT4OID:
        if (format == 0)
            return PyLong_FromString((char*)p, 0, 10);

        return PyLong_FromLong(ntohl(*(long*)p));

    case TEXTOID:
    case VARCHAROID:
        return PyUnicode_DecodeUTF8((const char*)p, strlen((const char*)p), 0);
    }

    return PyErr_Format(Error, "Unhandled OID %d\n", (int)oid);
}
