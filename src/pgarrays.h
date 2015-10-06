
#ifndef PGARRAYS_H
#define PGARRAYS_H

struct Params;

bool BindArray(Params& params, PyObject* param);

PyObject* GetInt4Array(const char* p);
// Reads an INT4ARRAYOID array result and returns a list of integers.

PyObject* GetInt8Array(const char* p);
PyObject* GetTextArray(const char* p);

#endif
