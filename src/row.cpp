
#include "pglib.h"
#include "row.h"
#include "getdata.h"
#include "resultset.h"

PyObject* Row_New(ResultSet* rset, int iRow)
{
    I(rset->columns != 0);

    int cCols = PyTuple_GET_SIZE(rset->columns);
    Tuple values(cCols);
    if (!values)
        return 0;

    for (int i = 0; i < cCols; i++)
    {
        PyObject* value = ConvertValue(rset->result, iRow, i, rset->integer_datetimes,
                                       rset->formats[i]);
        if (value == 0)
            return 0;
        values.SetItem(i, value);
    }

    Row* self = PyObject_NEW(Row, &RowType);
    if (self == 0)
        return 0;

    self->columns = rset->columns;
    Py_INCREF(self->columns);

    self->values = values.Detach();

    return (PyObject*)self;
}

static void Row_dealloc(PyObject* self)
{
    Row* row = reinterpret_cast<Row*>(self);
    Py_DECREF(row->columns);
    Py_DECREF(row->values);
    PyObject_Del(self);
}

inline int ColumnFromName(Row* self, PyObject* name)
{
    // Returns the column index from a column name.  Returns -1 if not found.

    for (int i = 0, c = PyTuple_GET_SIZE(self->columns); i < c; i++)
        if (PyUnicode_Compare(name, PyTuple_GET_ITEM(self->columns, i)) == 0)
            return i;
    return -1;
}


static PyObject* Row_getattro(PyObject* o, PyObject* name)
{
    // Called to handle 'row.colname'.

    Row* self = (Row*)o;

    if (PyUnicode_Check(name))
    {
        int iCol = ColumnFromName(self, name);
        if (iCol != -1)
        {
            PyObject* value = PyTuple_GET_ITEM(self->values, iCol);
            Py_INCREF(value);
            return value;
        }
    }

    return PyObject_GenericGetAttr(o, name);
}

static Py_ssize_t Row_length(PyObject* o)
{
    Row* self = (Row*)o;
    return PyTuple_GET_SIZE(self->columns);
}

/*
static int Row_contains(Row* self, PyObject* el)
{
    // Implementation of contains.  The documentation is not good (non-existent?), so I copied the following from the
    // PySequence_Contains documentation: Return -1 if error; 1 if ob in seq; 0 if ob not in seq.

    int cmp = 0;

    for (Py_ssize_t i = 0, c = PyTuple_GET_SIZE(self->cache); cmp == 0 && i < c; ++i) {
        PyObject* value = GetValue(self, i);
        if (value == 0)
            return -1;
        cmp = PyObject_RichCompareBool(el, value, Py_EQ);
    }
    
    return cmp;
}
*/

static PyObject* Row_item(PyObject* o, Py_ssize_t i)
{
    // Apparently, negative indexes are handled by magic ;) -- they never make it here.

    Row* self = (Row*)o;

    if (i < 0 || i >= PyTuple_GET_SIZE(self->values))
    {
        PyErr_SetString(PyExc_IndexError, "tuple index out of range");
        return NULL;
    }

    PyObject* value = PyTuple_GetItem(self->values, i);
    Py_INCREF(value);
    return value;
}

static int Row_assign(PyObject* o, Py_ssize_t i, PyObject* v)
{
    // Implements row[i] = value.

    Row* self = (Row*)o;

    if (i < 0 || i >= PyTuple_GET_SIZE(self->values))
    {
        PyErr_SetString(PyExc_IndexError, "Row assignment index out of range");
        return -1;
    }

    Py_DECREF(PyTuple_GET_ITEM(self->values, i));
    PyTuple_SET_ITEM(self->values, i, v);
    Py_INCREF(v);
    return 0;
}


static int Row_setattro(PyObject* o, PyObject *name, PyObject* v)
{
    Row* self = (Row*)o;

    int i = ColumnFromName(self, name);
    if (i == -1)
    {
        PyErr_SetString(Error, "Cannot add columns or attributes to a row");
        return -1;
    }

    Py_DECREF(PyTuple_GET_ITEM(self->values, i));
    PyTuple_SET_ITEM(self->values, i, v);
    Py_INCREF(v);
    return 0;
}

static PyObject* Row_repr(PyObject* o)
{
    Row* self = (Row*)o;
    return PyObject_Repr(self->values);
}

/*
static PyObject* Row_richcompare(PyObject* olhs, PyObject* orhs, int op)
{
    if (!Row_Check(olhs) || !Row_Check(orhs))
    {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }

    Row* lhs = (Row*)olhs;
    Row* rhs = (Row*)orhs;

    if (lhs->cValues != rhs->cValues)
    {
        // Different sizes, so use the same rules as the tuple class.
        bool result;
        switch (op)
        {
        case Py_EQ: result = (lhs->cValues == rhs->cValues); break;
        case Py_GE: result = (lhs->cValues >= rhs->cValues); break;
        case Py_GT: result = (lhs->cValues >  rhs->cValues); break;
        case Py_LE: result = (lhs->cValues <= rhs->cValues); break;
        case Py_LT: result = (lhs->cValues <  rhs->cValues); break;
        case Py_NE: result = (lhs->cValues != rhs->cValues); break;
        default:
            // Can't get here, but don't have a cross-compiler way to silence this.
            result = false;
        }
        PyObject* p = result ? Py_True : Py_False;
        Py_INCREF(p);
        return p;
    }

    for (Py_ssize_t i = 0, c = lhs->cValues; i < c; i++)
        if (!PyObject_RichCompareBool(lhs->values[i], rhs->values[i], Py_EQ))
            return PyObject_RichCompare(lhs->values[i], rhs->values[i], op);

    // All items are equal.
    switch (op)
    {
    case Py_EQ:
    case Py_GE:
    case Py_LE:
        Py_RETURN_TRUE;

    case Py_GT:
    case Py_LT:
    case Py_NE:
        break;
    }

    Py_RETURN_FALSE;
}


static PyObject* Row_subscript(PyObject* o, PyObject* key)
{
    Row* row = (Row*)o;

    if (PyIndex_Check(key))
    {
        Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
        if (i == -1 && PyErr_Occurred())
            return 0;
        if (i < 0)
            i += row->cValues;

        if (i < 0 || i >= row->cValues)
            return PyErr_Format(PyExc_IndexError, "row index out of range index=%d len=%d", (int)i, (int)row->cValues);

        Py_INCREF(row->values[i]);
        return row->values[i];
    }

    if (PySlice_Check(key))
    {
        Py_ssize_t start, stop, step, slicelength;
#if PY_VERSION_HEX >= 0x03020000
        if (PySlice_GetIndicesEx(key, row->cValues, &start, &stop, &step, &slicelength) < 0)
            return 0;
#else
        if (PySlice_GetIndicesEx((PySliceObject*)key, row->cValues, &start, &stop, &step, &slicelength) < 0)
            return 0;
#endif

        if (slicelength <= 0)
            return PyTuple_New(0);

        if (start == 0 && step == 1 && slicelength == row->cValues)
        {
            Py_INCREF(o);
            return o;
        }

        Object result(PyTuple_New(slicelength));
        if (!result)
            return 0;
        for (Py_ssize_t i = 0, index = start; i < slicelength; i++, index += step)
        {
            PyTuple_SET_ITEM(result.Get(), i, row->values[index]);
            Py_INCREF(row->values[index]);
        }
        return result.Detach();
    }

    return PyErr_Format(PyExc_TypeError, "row indices must be integers, not %.200s", Py_TYPE(key)->tp_name);
}
*/

static PyMemberDef Row_members[] = 
{
    { (char*)"columns", T_OBJECT_EX, offsetof(Row, columns), 0, (char*)"tuple of column names" },
    { 0 }
};

static PyObject* Row_concat(PyObject* lhs, PyObject* rhs)
{
    PyErr_SetString(Error, "Row objects cannot be concatenated");
    return 0;
}
static PyObject* Row_repeat(PyObject *o, Py_ssize_t count)
{
    PyErr_SetString(Error, "Row objects cannot be repeated");
    return 0;
}
static PySequenceMethods row_as_sequence =
{
    Row_length,                 // sq_length
    Row_concat,                 // sq_concat
    Row_repeat,                 // sq_repeat
    Row_item,                   // sq_item
    0,                          // was_sq_slice
    Row_assign,                 // sq_ass_item
    0,                          // sq_ass_slice
    0, //Row_contains,               // sq_contains
};

/*
static PyMappingMethods row_as_mapping =
{
    Row_length,                 // mp_length
    Row_subscript,              // mp_subscript
    0,                          // mp_ass_subscript
};
*/

static PyMethodDef Row_methods[] =
{
    // { "__reduce__", (PyCFunction)Row_reduce, METH_NOARGS, 0 },
    { 0, 0, 0, 0 }
};


static char row_doc[] =
    "Row objects are sequence objects that hold query results.\n"
    "\n"
    "They are similar to tuples in that they cannot be resized and new attributes\n"
    "cannot be added, but individual elements can be replaced.  This allows data to\n"
    "be \"fixed up\" after being fetched.  (For example, datetimes may be replaced by\n"
    "those with time zones attached.)\n"
    "\n"
    "  row[0] = row[0].replace(tzinfo=timezone)\n"
    "  print row[0]\n"
    "\n"
    "Additionally, individual values can be optionally be accessed or replaced by\n"
    "name.  Non-alphanumeric characters are replaced with an underscore.\n"
    "\n"
    "  cursor.execute(\"select customer_id, [Name With Spaces] from tmp\")\n"
    "  row = cursor.fetchone()\n"
    "  print row.customer_id, row.Name_With_Spaces\n"
    "\n"
    "If using this non-standard feature, it is often convenient to specifiy the name\n"
    "using the SQL 'as' keyword:\n"
    "\n"
    "  cursor.execute(\"select count(*) as total from tmp\")\n"
    "  row = cursor.fetchone()\n"
    "  print row.total";

PyTypeObject RowType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "pglib.Row",                                            // tp_name
    sizeof(Row),                                            // tp_basicsize
    0,                                                      // tp_itemsize
    Row_dealloc,                                            // tp_dealloc
    0,                                                      // tp_print
    0,                                                      // tp_getattr
    0,                                                      // tp_setattr
    0,                                                      // tp_compare
    Row_repr,                                               // tp_repr
    0,                                                      // tp_as_number
    &row_as_sequence,                                       // tp_as_sequence
    0, // &row_as_mapping,                                        // tp_as_mapping
    0,                                                      // tp_hash
    0,                                                      // tp_call
    0,                                                      // tp_str
    Row_getattro,                                           // tp_getattro
    Row_setattro,                                           // tp_setattro
    0,                                                      // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                                     // tp_flags
    row_doc,                                                // tp_doc
    0,                                                      // tp_traverse
    0,                                                      // tp_clear
    0, //Row_richcompare,                                        // tp_richcompare
    0,                                                      // tp_weaklistoffset
    0,                                                      // tp_iter
    0,                                                      // tp_iternext
    Row_methods,                                            // tp_methods
    Row_members,                                            // tp_members
    0, // Row_getsetters,                                         // tp_getset
    0,                                                      // tp_base
    0,                                                      // tp_dict
    0,                                                      // tp_descr_get
    0,                                                      // tp_descr_set
    0,                                                      // tp_dictoffset
    0,                                                      // tp_init
    0,                                                      // tp_alloc
    0,                                                      // tp_new
    0,                                                      // tp_free
    0,                                                      // tp_is_gc
    0,                                                      // tp_bases
    0,                                                      // tp_mro
    0,                                                      // tp_cache
    0,                                                      // tp_subclasses
    0,                                                      // tp_weaklist
};
