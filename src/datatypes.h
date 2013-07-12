
#ifndef DATATYPES_H
#define DATATYPES_H

void Datatypes_Init();

struct Connection;
struct Params;

bool BindDate(Connection* cnxn, Params& params, PyObject* param);
PyObject* GetDate(const char* p);


#endif //  DATATYPES_H
