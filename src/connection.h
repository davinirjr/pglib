
#ifndef CONNECTION_H
#define CONNECTION_H

extern PyTypeObject ConnectionType;

struct Connection
{
    PyObject_HEAD
    PGconn* pgconn;
    bool integer_datetimes;

    FILE* tracefile;
};

PyObject* Connection_New(const char* conninfo);

#endif // CONNECTION_H