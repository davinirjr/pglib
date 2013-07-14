
#ifndef GETDATA_H
#define GETDATA_H

bool GetData_init();
PyObject* ConvertValue(PGresult* result, int iRow, int iCol, bool integer_datetimes);

#endif // GETDATA_H
