
#include "pglib.h"
#include "result.h"

PyObject* Result_New(PGresult* result)
{
    Result* r = PyObject_NEW(Result, &ResultType);

    if (r != 0)
    {
        r->result = result;
    }
    else
    {
        PQclear(result);
    }
    
    return reinterpret_cast<PyObject*>(r);
}

static void Result_dealloc(PyObject* self)
{
    Result* r = (Result*)self;
    PQclear(r->result);
    PyObject_Del(self);
}

PyTypeObject ResultType =
{
    PyVarObject_HEAD_INIT(0, 0)
    "pglib.Result",
    sizeof(ResultType),
    0,
    Result_dealloc,
    0,                          // tp_print
    0,                          // tp_getattr
    0,                          // tp_setattr
    0,                          // tp_compare
    0,                          // tp_repr
    0,                          // tp_as_number
    0,                          // tp_as_sequence
    0,                          // tp_as_mapping
    0,                          // tp_hash
    0,                          // tp_call
    0,                          // tp_str
    0,                          // tp_getattro
    0,                          // tp_setattro
    0,                          // tp_as_buffer
    Py_TPFLAGS_DEFAULT,         // tp_flags
    0, //result_doc,             // tp_doc
    0,                          // tp_traverse
    0,                          // tp_clear
    0,                          // tp_richcompare
    0,                          // tp_weaklistoffset
    0,                          // tp_iter
    0,                          // tp_iternext
    0, //Result_methods,         // tp_methods
    0,                          // tp_members
    0, // Result_getset,          // tp_getset
    0,                          // tp_base
    0,                          // tp_dict
    0,                          // tp_descr_get
    0,                          // tp_descr_set
    0,                          // tp_dictoffset
    0,                          // tp_init
    0,                          // tp_alloc
    0,                          // tp_new
    0,                          // tp_free
    0,                          // tp_is_gc
    0,                          // tp_bases
    0,                          // tp_mro
    0,                          // tp_cache
    0,                          // tp_subclasses
    0,                          // tp_weaklist
};
