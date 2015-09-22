
#ifndef CONNECTION_H
#define CONNECTION_H

enum AsyncStatus {
    ASYNC_STATUS_SYNC       = 0, // not an async connection
    ASYNC_STATUS_CONNECTING = 1,
    ASYNC_STATUS_IDLE       = 2,
    ASYNC_STATUS_READING    = 3,
    ASYNC_STATUS_WRITE      = 4
};

enum AsyncFunc {
    ASYNC_FUNC_IDLE    = 0,
    ASYNC_FUNC_CONNECT = 1,
    ASYNC_FUNC_QUERY   = 2,
    ASYNC_FUNC_FETCH   = 3
};


extern PyTypeObject ConnectionType;

struct Connection
{
    PyObject_HEAD

    PGconn* pgconn;
    // This will be set to zero when closed.  Always check!

    bool integer_datetimes;

    FILE* tracefile;

    AsyncStatus async_status;
    AsyncFunc async_func;
};

PyObject* Connection_New(PGconn* pgconn, bool async);

#endif // CONNECTION_H
