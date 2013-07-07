
#ifndef PARAMS_H
#define PARAMS_H

struct Params
{
    Oid*   types;
    char** values;
    int*   lengths;
    int*   formats;

    int count; // How many are we going to bind?
    int bound; // How many have we bound?

    void* pool;

    Params(int count);
    ~Params();

    bool valid() const
    {
        return types && values && lengths && formats;
    }

    bool Bind(Oid type, char* value, int length, int format);
};

bool BindParams(Connection* cnxn, Params& params, PyObject* args);

#endif // PARAMS_H
