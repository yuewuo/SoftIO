#ifndef __fifo_H
#define __fifo_H

#include <stdint.h>
#include <math.h>
#include <string.h>

#if __SIZEOF_POINTER__ == 8
#include "assert.h"
#define FIFO_PTR(base) ((char*)((long long)(base)))
#define FIFO_BASE(ptr) ((uint32_t)((long long)(ptr)))
#else
#define FIFO_PTR(base) ((char*)(base))
#define FIFO_BASE(ptr) ((uint32_t)(ptr))
#endif

typedef struct {
    uint32_t base;  // base pointer
    uint32_t length;  // length of this fifo
    uint32_t read;
    uint32_t write;
} Fifo_t;

// if you use a parent struct that contains fifo "name" and also buffer named "name"_buf, this could be used to initialize it
#define FIFO_STD_INIT(parent, name) fifo_init(&(parent.name), parent.name##_buf, sizeof(parent.name##_buf))

inline void fifo_init(Fifo_t* fifo, char* base, uint32_t length) {
// there is risk to convert 64bit pointer to 32bit fifo_ptr, so sanity check is done on 64bit program
#if __SIZEOF_POINTER__ == 8
    assert(((size_t)base) + length < 0x100000000ULL);
#endif
    fifo->base = FIFO_BASE(base);
    fifo->length = length;
    fifo->read = 0;
    fifo->write = 0;
}

// not safe when fifo is full
inline void fifo_enque(Fifo_t* fifo, char c) {
    FIFO_PTR(fifo->base)[fifo->write] = c;
    fifo->write = (fifo->write + 1) % fifo->length;
}

// not safe when fifo is empty
inline char fifo_deque(Fifo_t* fifo) {
    char c = FIFO_PTR(fifo->base)[fifo->read];
    fifo->read = (fifo->read + 1) % fifo->length;
    return c;
}

inline char fifo_preread(Fifo_t* fifo, uint32_t index) {  // read at (read+index)%length
    return FIFO_PTR(fifo->base)[(fifo->read + index) % fifo->length];
}

inline char fifo_full(Fifo_t* fifo) {
    return (fifo->write + 1) % fifo->length == fifo->read;
}

inline char fifo_empty(Fifo_t* fifo) {
    return (fifo->read == fifo->write);
}

inline void fifo_clear(Fifo_t* fifo) {
    fifo->read = fifo->write;
}

// return the count of data in the buffer, however, not consistant because of no lock
inline uint32_t fifo_count(Fifo_t* fifo) {
    return (fifo->write - fifo->read + fifo->length) % fifo->length;
}
#define fifo_data_count(fifo) fifo_count(fifo)  // deprecated

inline uint32_t fifo_remain(Fifo_t* fifo) {
    return fifo->length - ((fifo->write - fifo->read + fifo->length) % fifo->length) - 1;
}

// move data from src to dest with maximum length of max_length, return the byte count moved
inline uint32_t fifo_move(Fifo_t* dest, Fifo_t* src, uint32_t max_length) {
    uint32_t i;
    for (i=0; i<max_length && !fifo_empty(src) && !fifo_full(dest); ++i) fifo_enque(dest, fifo_deque(src));
    return i;
}

inline char* __fifo_read_base(Fifo_t* fifo) {
    return FIFO_PTR(fifo->base) + fifo->read;
}

inline uint32_t __fifo_read_base_length(Fifo_t* fifo) {
    return fifo->length - fifo->read;
}

inline char* __fifo_write_base(Fifo_t* fifo) {
    return FIFO_PTR(fifo->base) + fifo->write;
}

inline uint32_t __fifo_write_base_length(Fifo_t* fifo) {
    return fifo->length - fifo->write;
}

inline void __fifo_fullfill(Fifo_t* fifo) {  // useful for testing speed
    fifo->write = (fifo->read + fifo->length - 1) % fifo->length;
}

// move data from src to dest with maximum length of max_length, return the byte count moved
inline uint32_t fifo_move_to_buffer(char* dest, Fifo_t* src, uint32_t max_length) {
    uint32_t copylen = fifo_count(src);
    if (max_length < copylen) copylen = max_length;
    uint32_t len = __fifo_read_base_length(src);
    if (len >= copylen) {  // copy once OK
        memcpy(dest, __fifo_read_base(src), copylen);
    } else {  // need slicing
        memcpy(dest, __fifo_read_base(src), len);
        memcpy(dest + len, FIFO_PTR(src->base), copylen - len);
    }
    src->read = (src->read + copylen) % src->length;  // set pointer directly
    return copylen;
}

// move data from src to dest with maximum length of max_length, return the byte count moved
inline uint32_t fifo_copy_from_buffer(Fifo_t* dest, char* src, uint32_t max_length) {
    uint32_t copylen = fifo_remain(dest);
    if (max_length < copylen) copylen = max_length;
    uint32_t len = __fifo_write_base_length(dest);
    if (len >= copylen) {  // copy once OK
        memcpy(__fifo_write_base(dest), src, copylen);
    } else {  // need slicing
        memcpy(__fifo_write_base(dest), src, len);
        memcpy(FIFO_PTR(dest->base), src + len, copylen - len);
    }
    dest->write = (dest->write + copylen) % dest->length;  // set pointer directly
    return copylen;
}

#define fifo_dump(fifo) do {\
    uint32_t count = fifo_count(&fifo); \
    printf("fifo \"%s\": base(0x%08X), length(%d), read(%d), write(%d), has data(%d)\n", #fifo, fifo.base, fifo.length, fifo.read, fifo.write, count); \
    for (uint32_t i=0; i<count; i+=16) { \
        printf("  "); \
        for (uint32_t j=0; j<16 && j+i<count; ++j) printf(" 0x%02X", (unsigned char)fifo_preread(&(fifo), i+j)); \
        printf("\n"); \
    } \
} while (0)

#endif
