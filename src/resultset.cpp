
#include "pglib.h"
#include "resultset.h"
#include "row.h"

PyObject* ResultSet_New(PGresult* result)
{
    ResultSet* rset = PyObject_NEW(ResultSet, &ResultSetType);
    if (rset == 0)
    {
        PQclear(result);
        return 0;
    }

    rset->result   = result;
    rset->cRows    = PQntuples(result);
    rset->cCols    = PQnfields(result);
    rset->cFetched = 0;

    return reinterpret_cast<PyObject*>(rset);
}

static void ResultSet_dealloc(PyObject* self)
{
    ResultSet* rset = (ResultSet*)self;
    if (rset->result)
        PQclear(rset->result);
    PyObject_Del(self);
}

static PyObject* ResultSet_iter(PyObject* self)
{
    Py_INCREF(self);
    return self;
}

static PyObject* ResultSet_iternext(PyObject* self)
{
    ResultSet* rset = reinterpret_cast<ResultSet*>(self);

    if (rset->cFetched >= PQntuples(rset->result))
    {
        PyErr_SetNone(PyExc_StopIteration);
        return 0;
    }

    return Row_New(rset, rset->cFetched++);
}

static Py_ssize_t ResultSet_length(PyObject* self)
{
    ResultSet* rset = (ResultSet*)self;
    return rset->cRows;
}

static PyObject* ResultSet_item(PyObject* self, Py_ssize_t i)
{
    // Apparently, negative indexes are handled by magic ;) -- they never make it here.

    ResultSet* rset = (ResultSet*)self;

    if (i < 0 || i >= rset->cRows)
        return PyErr_Format(PyExc_IndexError, "Index %d out of range.  ResultSet has %d rows", (int)i, (int)rset->cRows);

    return Row_New(rset, i);
}

/*
static PyObject* ResultSet_getcolumns(ResultSet* self, void* closure)
{
    UNUSED(closure);

    if (self->columns == 0)
    {
    }

    Py_INCREF(self->first);
    return self->first;
}

static PyMemberDef ResultSet_members[] =
{
    { (char*)"columns", T_OBJECT_EX, offsetof(ResultSet, columns), READONLY, 0 },
    { 0 }
};
*/

static PySequenceMethods rset_as_sequence =
{
    ResultSet_length,           // sq_length
    0,                          // sq_concat
    0,                          // sq_repeat
    ResultSet_item,             // sq_item
    0,                          // was_sq_slice
    0,                          // sq_ass_item
    0,                          // sq_ass_slice
    0,                          // sq_contains
};

PyTypeObject ResultSetType =
{
    PyVarObject_HEAD_INIT(0, 0)
    "pglib.ResultSet",
    sizeof(ResultSetType),
    0,
    ResultSet_dealloc,
    0,                          // tp_print
    0,                          // tp_getattr
    0,                          // tp_setattr
    0,                          // tp_compare
    0,                          // tp_repr
    0,                          // tp_as_number
    &rset_as_sequence,          // tp_as_sequence
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
    ResultSet_iter,             // tp_iter
    ResultSet_iternext,         // tp_iternext
    0, // ResultSet_methods,         // tp_methods
    0, // ResultSet_members,                          // tp_members
    0,          // tp_getset
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
