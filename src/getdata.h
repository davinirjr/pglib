
#ifndef GETDATA_H
#define GETDATA_H

struct Row;

PyObject* ConvertValue(PGresult* result, int iRow, int iCol);

bool GetData_init();

#endif // GETDATA_H
