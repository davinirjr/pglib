
#include "pglib.h"
#include "resultset.h"
#include "connection.h"
#include "row.h"

static PyObject* AllocateColumns(PGresult* result)
{
    int count = PQnfields(result);

    Tuple cols(count);
    if (!cols)
        return 0;

    for (int i = 0; i < count; i++)
    {
        const char* szName = PQfname(result, i);
        PyObject* col = PyUnicode_DecodeUTF8(szName, strlen(szName), 0);
        if (col == 0)
            return 0;
        cols.SetItem(i, col);
    }

    return cols.Detach();
}

static int* AllocateFormats(PGresult* result)
{
    int count = PQnfields(result);
    if (count == 0)
        return 0;

    int* p = (int*)malloc(sizeof(int) * count);
    if (p == 0)
    {
        PyErr_NoMemory();
        return 0;
    }

    for (int i = 0; i < count; i++)
        p[i] = PQfformat(result, i);

    return p;
}

PyObject* ResultSet_New(Connection* cnxn, PGresult* result)
{
    ResultSet* rset = PyObject_NEW(ResultSet, &ResultSetType);
    if (rset == 0)
    {
        PQclear(result);
        return 0;
    }

    rset->result            = result;
    rset->formats           = AllocateFormats(result);
    rset->cFetched          = 0;
    rset->columns           = AllocateColumns(result);
    rset->integer_datetimes = cnxn->integer_datetimes;

    if (PyErr_Occurred())
    {
        Py_DECREF(rset);
        rset = 0;
    }

    return reinterpret_cast<PyObject*>(rset);
}

static void ResultSet_dealloc(PyObject* self)
{
    ResultSet* rset = (ResultSet*)self;
    if (rset->result)
        PQclear(rset->result);

    if (rset->formats)
        free(rset->formats);

    Py_XDECREF(rset->columns);
    PyObject_Del(self);
}


static PyObject* ResultSet_iter(PyObject* self)
{
    // You can iterate over results multiple times, but not at the same time.
    ResultSet* rset = (ResultSet*)self;
    rset->cFetched = 0;
    Py_INCREF(self);
    return self;
}

static PyObject* ResultSet_iternext(PyObject* o)
{
    ResultSet* self = reinterpret_cast<ResultSet*>(o);

    if (self->cFetched >= PQntuples(self->result))
        return 0;

    return Row_New(self, self->cFetched++);
}

static Py_ssize_t ResultSet_length(PyObject* self)
{
    ResultSet* rset = (ResultSet*)self;
    return PQntuples(rset->result);
}

static PyObject* ResultSet_item(PyObject* o, Py_ssize_t i)
{
    // Apparently, negative indexes are handled by magic ;) -- they never make it here.

    ResultSet* self = (ResultSet*)o;

    if (i < 0 || i >= PQntuples(self->result))
        return PyErr_Format(PyExc_IndexError, "Index %d out of range.  ResultSet has %d rows", (int)i, (int)PQntuples(self->result));

    return Row_New(self, i);
}

static PyObject* ResultSet_getcolumns(ResultSet* self, void* closure)
{
    UNUSED(closure);

    if (self->columns == 0)
    {
        Py_RETURN_NONE;
    }

    Py_INCREF(self->columns);
    return self->columns;
}

static PyObject* ResultSet_rowcount(ResultSet* self, void* closure)
{
    UNUSED(closure);
    const char* sz = PQcmdTuples(self->result);
    long count = (!sz || sz[0] == 0) ? -1 : strtol(sz, 0, 10);
    return PyLong_FromLong(count);
}

static PyGetSetDef ResultSet_getsetters[] = 
{
    { (char*)"columns",  (getter)ResultSet_getcolumns, 0, (char*)"tuple of column names", 0 },
    { (char*)"rowcount", (getter)ResultSet_rowcount,   0, (char*)"Returns the number of rows affected by the SQL command", 0 },
    { 0 }
};

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
    ResultSet_getsetters,        // tp_getset
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
