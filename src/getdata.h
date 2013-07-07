
#ifndef GETDATA_H
#define GETDATA_H

struct Row;

PyObject* ConvertValue(PGresult* result, int iRow, int iCol);

#endif // GETDATA_H
