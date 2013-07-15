
#ifndef PARAMS_H
#define PARAMS_H

void Params_Init();

struct Pool;

struct Params
{
    Oid*   types;
    const char** values;
    int*   lengths;
    int*   formats;

    int count; // How many are we going to bind?
    int bound; // How many have we bound?

    Pool* pool;

    Params(int count);
    ~Params();

    bool valid() const
    {
        return types && values && lengths && formats;
    }

    char* Allocate(size_t cbNeeded);

    bool Bind(Oid type, const void* value, int length, int format);
};

bool BindParams(Connection* cnxn, Params& params, PyObject* args);

#endif // PARAMS_H
