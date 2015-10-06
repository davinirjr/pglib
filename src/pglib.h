
#ifndef PGLIB_H
#define PGLIB_H

extern "C"
{
#include <libpq-fe.h>
}

#include <stdint.h>

#define PY_SSIZE_T_CLEAN 1

#include <Python.h>
#include <floatobject.h>
#include <longobject.h>
#include <boolobject.h>
#include <unicodeobject.h>
#include <structmember.h>
// #include <sql.h>
// #include <sqlext.h>

#if PY_VERSION_HEX < 0x03030000
#error This branch is for Python 3.3+
#endif

#ifdef __APPLE__
// Turn up clang compiler warnings.
#endif

#ifndef _countof
#define _countof(a) (sizeof(a) / sizeof(a[0]))
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifdef UNUSED
#undef UNUSED
#endif
inline void UNUSED(...) { }

#define MAX(a,b) (((a)>(b))?(a):(b))

extern PyObject* Error;

// From pg_type.h

#define ANYARRAYOID     2277
#define ANYOID          2276
#define BOOLOID         16
#define BPCHAROID       1042
#define BYTEAOID        17
#define CASHOID         790
#define DATEOID         1082
#define FLOAT4OID       700
#define FLOAT8OID       701
#define INT2ARRAYOID    1005
#define INT2OID         21
#define INT4ARRAYOID    1007
#define INT4OID         23
#define INT8ARRAYOID    1016
#define INT8OID         20
#define NUMERICOID      1700
#define TEXTARRAYOID    1009
#define TEXTOID         25
#define TIMEOID         1083
#define TIMESTAMPOID    1114
#define UUIDOID         2950
#define VARCHAROID      1043


enum
{
    FORMAT_TEXT = 0,
    FORMAT_BINARY = 1
};


// -----------------------------------------------------------------------------------------------
// Debug

#if defined(PGLIB_ASSERT)
#if _MSC_VER
  #include <crtdbg.h>
#endif
  inline void FailAssert(const char* szFile, int line, const char* szExpr)
  {
      printf("assertion failed: %s(%d)\n%s\n", szFile, line, szExpr);
#if _MSC_VER
      __debugbreak(); // _CrtDbgBreak();
#else
      assert(0);
#endif
  }
  #define I(expr) if (!(expr)) FailAssert(__FILE__, __LINE__, #expr);
  #define N(expr) if (expr) FailAssert(__FILE__, __LINE__, #expr);
#else
  #define I(expr)
  #define N(expr)
#endif



// -----------------------------------------------------------------------------------------------
// C++ wrappers for objects.  No exceptions.

struct Object
{
    PyObject* p;

    // Borrows the reference (takes ownership without adding a new referencing).
    Object(PyObject* _p = 0) { p = _p; }

    // If it still has the pointer, it dereferences it.
    ~Object() { Py_XDECREF(p); }

    operator PyObject*() { return p; }
    PyObject* Get() { return p; }

    void Attach(PyObject* _p)
    {
        // I(p == 0);
        p = _p;
    }

    void AttachAndIncrement(PyObject* _p)
    {
        // I(p == 0);
        p = _p;
        Py_INCREF(p);
    }

    PyObject* Detach()
    {
        PyObject* _p = p;
        p = 0;
        return _p;
    }
};

struct Tuple : public Object
{
    Tuple(Py_ssize_t len)
        : Object(PyTuple_New(len))
    {
    }

    void SetItem(Py_ssize_t i, PyObject* item)
    {
        PyTuple_SET_ITEM(p, i, item);
    }

    operator bool() { return p != 0; }

    operator PyTupleObject*() { return (PyTupleObject*)p; }
};

struct List : public Object
{
    List(Py_ssize_t len)
        : Object(PyList_New(len))
    {
    }

    PyObject* Join(PyObject* sep)
    {
        return PyUnicode_Join(sep, p);
    }

    bool AppendAndIncrement(PyObject* o)
    {
        if (PyList_Append(p, o) == -1)
            return false;
        Py_INCREF(o);
        return true;
    }

    bool AppendAndBorrow(PyObject* o)
    {
        return PyList_Append(p, o) == 0;
    }
};

struct ResultHolder
{
    // Designed to hold a result on the stack.

    PGresult* p;
    ResultHolder(PGresult* _p = 0) { p = _p; }
    ~ResultHolder()
    {
        if (p)
            PQclear(p);
    }

    void operator=(PGresult* _p) { p = _p; }

    operator PGresult*() { return p; }

    PGresult* Detach()
    {
        PGresult* tmp = p;
        p = 0;
        return tmp;
    }
};

extern PyObject* strComma;
extern PyObject* strParens;
extern PyObject* strLeftParen;
extern PyObject* strRightParen;
extern PyObject* strEmpty;

#endif // PGLIB_H
