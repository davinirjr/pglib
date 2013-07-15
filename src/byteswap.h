
#ifndef BYTESWAP_H
#define BYTESWAP_H

#ifdef __BIG_ENDIAN__

#define swaps2
#define swaps4
#define swaps8
#define swapu2
#define swapu4
#define swapu8

#define swapfloat
#define swapdouble

#else

inline uint16_t swapu2(uint16_t value)
{
    return (
        ((value & 0x00FF) << 8) |
        ((value & 0xFF00) >> 8)
    );
}

inline uint32_t swapu4(uint32_t value)
{
    return (
        ((value & 0x000000FF) << 24) |
        ((value & 0x0000FF00) <<  8) |
        ((value & 0x00FF0000) >>  8) |
        ((value & 0xFF000000) >> 24)
    );
}

inline uint64_t swapu8(uint64_t value)
{
    return (
        ((value & 0x00000000000000FFULL) << 56) |
        ((value & 0x000000000000FF00ULL) << 40) |
        ((value & 0x0000000000FF0000ULL) << 24) |
        ((value & 0x00000000FF000000ULL) <<  8) |
        ((value & 0x000000FF00000000ULL) >>  8) |
        ((value & 0x0000FF0000000000ULL) >> 24) |
        ((value & 0x00FF000000000000ULL) >> 40) |
        ((value & 0xFF00000000000000ULL) >> 56)
    );
}

inline int16_t swaps2(int16_t value) { return (int16_t)swapu2((uint16_t)value); }
inline int32_t swaps4(int32_t value) { return (int32_t)swapu4((uint32_t)value); }
inline int64_t swaps8(int64_t value) { return (int64_t)swapu8((uint64_t)value); }

inline float swapfloat(float value)
{
    union
    {
        float f;
        uint32_t i;
    } tmp;
    tmp.f = value;
    tmp.i = swapu4(tmp.i);
    return tmp.f;
}

inline double swapdouble(double value)
{
    union
    {
        double f;
        uint64_t i;
    } tmp;
    tmp.f = value;
    tmp.i = swapu8(tmp.i);
    return tmp.f;
}

#endif

#endif //  BYTESWAP_H
