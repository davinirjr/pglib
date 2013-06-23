
#ifndef PGLIB_H
#define PGLIB_H

extern "C" 
{
#include <libpq-fe.h>
}

#define PY_SSIZE_T_CLEAN 1

#include <Python.h>
#include <floatobject.h>
#include <longobject.h>
#include <boolobject.h>
#include <unicodeobject.h>
#include <structmember.h>

#include <sql.h>
#include <sqlext.h>

#if PY_VERSION_HEX < 0x03030000
#error This branch is for Python 3.3+
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

extern PyObject* Error;


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


#endif // PGLIB_H
