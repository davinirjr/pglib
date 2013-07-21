
#ifndef DATATYPES_H
#define DATATYPES_H

extern PyObject* decimal_type;
extern PyObject* uuid_type;

bool DataTypes_Init();

inline bool UUID_Check(PyObject* p)
{
    return Py_TYPE(p) == (_typeobject*)uuid_type;
}

PyObject* UUID_FromBytes(const char* pch);

inline bool Decimal_Check(PyObject* p)
{
    return Py_TYPE(p) == (_typeobject*)decimal_type;
}

PyObject* Decimal_FromASCII(const char* sz);

PyObject* Decimal_NaN();

#endif // DATATYPES_H
