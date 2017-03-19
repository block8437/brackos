#include <stdint.h>

unsigned long strlen(const char* str) {
    unsigned long i = 0;

    while ( *str ) {
        i++;
        str++;
    }

    return i;
}

void* memcpy(void* dest, const void* src, unsigned long count) {
    for ( int i = 0; i < count; i++ ) {
        *(unsigned char*) dest = *(unsigned char*) src;

        dest++;
        src++;
    }

    return dest;
}

void* memset(void* dest, unsigned char c, unsigned int count) {
    unsigned char* _dest = (unsigned char*) dest;

    for ( int i = 0; i < count; i++ ) {
        _dest[i] = c;
    }

    return dest;
}
