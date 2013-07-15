
#include "pglib.h"
#include "debug.h"

void DumpBytes(const void* v, int len)
{
    const char* p = (const char*)v;

    printf("len=%d\n", len);
    for (int i = 0; i < len; i++)
    {
        if (i > 0 && (i % 4) == 0)
            printf(" ");

        if (i > 0 && (i % 10) == 0)
            printf("\n");
        printf("%02x", *(unsigned char*)&p[i]);
    }
    printf("\n");
}

