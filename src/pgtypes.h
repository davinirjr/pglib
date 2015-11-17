
#ifndef PGTYPES_H
#define PGTYPES_H

// Unfortunately these aren't exposed in libpq-fe.h.  Watch your packing.

struct Interval
{
    int64_t time;
    int32_t day;
    int32_t month;
};

#endif
