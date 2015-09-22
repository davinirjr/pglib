
#ifndef GETDATA_H
#define GETDATA_H

bool GetData_Init();
PyObject* ConvertValue(PGresult* result, int iRow, int iCol, bool integer_datetimes, int format);

#endif // GETDATA_H
