
#ifndef DECIMAL_H
#define DECIMAL_H

extern PyObject* decimal_type;

bool Decimal_Init();

PyObject* Decimal_FromASCII(const char* sz);

inline bool PyDecimal_Check(PyObject* p)
{
    return Py_TYPE(p) == (_typeobject*)decimal_type;
}

PyObject* Decimal_NaN();

#endif // DECIMAL_H
