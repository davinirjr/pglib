
#include "pglib.h"
#include "row.h"
#include "getdata.h"
#include "resultset.h"

PyObject* Row_New(ResultSet* rset, int iRow)
{
    Row* row = PyObject_NEW(Row, &RowType);
    if (row == 0)
        return 0;

    Py_INCREF(rset);

    row->rset  = rset;
    row->iRow  = iRow;

    return (PyObject*)row;
}

static void Row_dealloc(PyObject* self)
{
    Row* row = reinterpret_cast<Row*>(self);
    Py_DECREF(row->rset);
    PyObject_Del(self);
}

static PyObject* Row_getattro(PyObject* self, PyObject* name)
{
    // Called to handle 'row.colname'.

    Row* row = (Row*)self;

    const char* szName = PyUnicode_AsUTF8(name);

    int iCol = PQfnumber(row->rset->result, szName);

    if (iCol == -1)
        return PyObject_GenericGetAttr((PyObject*)row, name);

    return ConvertValue(row->rset->result, row->iRow, iCol);
}

static Py_ssize_t Row_length(PyObject* self)
{
    Row* row = (Row*)self;
    return row->rset->cCols;
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

static PyObject* Row_item(PyObject* self, Py_ssize_t i)
{
    // Apparently, negative indexes are handled by magic ;) -- they never make it here.

    Row* row = (Row*)self;

    if (i < 0 || i >= row->rset->cCols)
    {
        PyErr_SetString(PyExc_IndexError, "tuple index out of range");
        return NULL;
    }

    return ConvertValue(row->rset->result, row->iRow, i);
}

/*
static int Row_ass_item(PyObject* o, Py_ssize_t i, PyObject* v)
{
    // Implements row[i] = value.

    Row* self = (Row*)o;

    if (i < 0 || i >= self->cValues)
    {
        PyErr_SetString(PyExc_IndexError, "Row assignment index out of range");
        return -1;
    }

    Py_XDECREF(self->apValues[i]);
    Py_INCREF(v);
    self->apValues[i] = v;

    return 0;
}


static int Row_setattro(PyObject* o, PyObject *name, PyObject* v)
{
    Row* self = (Row*)o;

    PyObject* index = PyDict_GetItem(self->map_name_to_index, name);

    if (index)
        return Row_ass_item(o, PyNumber_AsSsize_t(index, 0), v);

    return PyObject_GenericSetAttr(o, name, v);
}
*/

/*
static PyObject* Row_repr(PyObject* o)
{
    Row* self = (Row*)o;

    if (self->cValues == 0)
        return PyUnicode_FromString("()");

    List list(self->cValues * 2 + 1);
    list.AppendAndIncrement(strLeftParen);

    for (Py_ssize_t i = 0; i < self->cValues; i++)
    {
        if (i > 0)
            list.AppendAndIncrement(strComma);
        list.AppendAndBorrow(PyObject_Repr(self->apValues[i]));
    }

    if (self->cValues == 1)
    {
        // Need a trailing comma: (value,)
        list.AppendAndIncrement(strComma);
    }

    list.AppendAndIncrement(strRightParen);

    return list.Join(strEmpty);
}
*/
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
        if (!PyObject_RichCompareBool(lhs->apValues[i], rhs->apValues[i], Py_EQ))
            return PyObject_RichCompare(lhs->apValues[i], rhs->apValues[i], op);

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

        Py_INCREF(row->apValues[i]);
        return row->apValues[i];
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
            PyTuple_SET_ITEM(result.Get(), i, row->apValues[index]);
            Py_INCREF(row->apValues[index]);
        }
        return result.Detach();
    }

    return PyErr_Format(PyExc_TypeError, "row indices must be integers, not %.200s", Py_TYPE(key)->tp_name);
}

static PyObject* Row_repr(PyObject* o)
{
    Row* self = (Row*)o;

    if (self->cValues == 0)
        return PyUnicode_FromString("()");

    List list(self->cValues * 2 + 1);


    list.AppendAndIncrement(strLeftParen);

    for (Py_ssize_t i = 0; i < self->cValues; i++)
    {
        if (i > 0)
            list.AppendAndIncrement(strComma);
        list.AppendAndBorrow(PyObject_Repr(self->apValues[i]));
    }

    if (self->cValues == 1)
    {
        // Need a trailing comma: (value,)
        list.AppendAndIncrement(strComma);
    }

    list.AppendAndIncrement(strRightParen);

    return list.Join(strEmpty);
}

*/

static PySequenceMethods row_as_sequence =
{
    Row_length,                 // sq_length
    0,                          // sq_concat
    0,                          // sq_repeat
    Row_item,                   // sq_item
    0,                          // was_sq_slice
    0, //Row_ass_item,               // sq_ass_item
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

static char description_doc[] = "The Cursor.description sequence from the Cursor that created this row.";

static PyMemberDef Row_members[] =
{
    // { "cursor_description", T_OBJECT_EX, offsetof(Row, description), READONLY, description_doc },
    { 0 }
};


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
    0, //Row_repr,                                               // tp_repr
    0,                                                      // tp_as_number
    &row_as_sequence,                                       // tp_as_sequence
    0, // &row_as_mapping,                                        // tp_as_mapping
    0,                                                      // tp_hash
    0,                                                      // tp_call
    0,                                                      // tp_str
    Row_getattro,                                           // tp_getattro
    0, // Row_setattro,                                           // tp_setattro
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
    0,                                                      // tp_getset
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
