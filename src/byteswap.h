
#ifndef BYTESWAP_H
#define BYTESWAP_H

// Remember, htons and htonl worked with *unsigned* integers.
//
// Not tested on 32-bit or big endian machines yet.

#ifdef __BIG_ENDIAN__
inline long signed_ntohs(int16_t value)
{
    return value;
}
#else

// Watch out for sign extension.  If shifting the top bits to the right, if the sign bit is on it will be propogated.
// The easy solution is to shift first, and then &.

inline long signed_ntohs(int16_t value)
{
    // Note: Don't remove the cast to int16_t
    return (int16_t)(((value >> 8) & 0x00ff) | ((value & 0x00ff) << 8));
}

inline long signed_ntohl(int32_t value)
{
    return (int32_t)(
    ((value & 0x000000FF) << 24) |
    ((value & 0x0000FF00) <<  8) |
    ((value & 0x00FF0000) >>  8) |
    ((value >> 24) & 0x000000FF)
    );
}

inline int64_t signed_ntohll(int64_t value)
{
    return (int64_t)(
        ((value & 0x00000000000000FFULL) << 56) | 
        ((value & 0x000000000000FF00ULL) << 40) | 
        ((value & 0x0000000000FF0000ULL) << 24) | 
        ((value & 0x00000000FF000000ULL) <<  8) | 
        ((value & 0x000000FF00000000ULL) >>  8) | 
        ((value & 0x0000FF0000000000ULL) >> 24) | 
        ((value & 0x00FF000000000000ULL) >> 40) | 
        ((value >> 56) & 0x00000000000000FFULL));
}
#endif

#endif //  BYTESWAP_H
