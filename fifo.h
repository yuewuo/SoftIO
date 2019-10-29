#ifndef __fifo_H
#define __fifo_H

#include <stdint.h>
#include <math.h>
#include <string.h>

#if __SIZEOF_POINTER__ == 8
typedef struct {
    char* base;
    uint32_t length;
} fifo_base_length_t;
#include "assert.h"
#include "stdio.h"
// #define FIFO_PTR(base) ((char*)((long long)(base)))
// #define FIFO_BASE(ptr) ((uint32_t)((long long)(ptr)))
#define __FIFO_GET_PTR8(fifo) ((fifo_base_length_t*)((fifo)->__L + (((long long)(fifo)->__H) << 32)))
#define __FIFO_INIT(fifo) do { fifo_base_length_t* ptr = new fifo_base_length_t; (fifo)->__L = ((long long)ptr); (fifo)->__H = ((long long)ptr) >> 32; } while(0)
#define __FIFO_SET_LENGTH(fifo, length) __FIFO_GET_PTR8(fifo)->length = (length)
#define __FIFO_SET_BASE(fifo, base) __FIFO_GET_PTR8(fifo)->base = (base)
#define __FIFO_GET_LENGTH(fifo) (__FIFO_GET_PTR8(fifo)->length)
#define __FIFO_GET_BASE(fifo) (__FIFO_GET_PTR8(fifo)->base)
#else
// #define FIFO_PTR(base) ((char*)(base))
// #define FIFO_BASE(ptr) ((uint32_t)(ptr))
#define __FIFO_INIT(fifo)  // do nothing
#define __FIFO_SET_LENGTH(fifo, length) (fifo)->length = (length)
#define __FIFO_SET_BASE(fifo, base) (fifo)->base = (uint32_t)(base)
#define __FIFO_GET_LENGTH(fifo) ((fifo)->length)
#define __FIFO_GET_BASE(fifo) ((char*)((fifo)->base))
#endif

typedef struct {
#if __SIZEOF_POINTER__ == 8
    uint32_t __L;
    uint32_t __H;
#else
    uint32_t base;  // base pointer
    uint32_t length;  // length of this fifo
#endif
    uint32_t read;
    uint32_t write;
#if __SIZEOF_POINTER__ == 8 && defined(__cplusplus)
    uint32_t Length() { return __FIFO_GET_LENGTH(this); }
    char* Base() { return __FIFO_GET_BASE(this); }
#endif
} Fifo_t;

#if __SIZEOF_POINTER__ == 8
static inline void fifo_destroy(Fifo_t* fifo) { delete __FIFO_GET_PTR8(fifo); }  // do not call this will cause memory leak
#else
static inline void fifo_destroy(Fifo_t* fifo) {}  // do nothing
#endif

// if you use a parent struct that contains fifo "name" and also buffer named "name"_buf, this could be used to initialize it
#define FIFO_STD_INIT(parent, name) fifo_init(&(parent.name), parent.name##_buf, sizeof(parent.name##_buf))

static inline void fifo_init(Fifo_t* fifo, char* base, uint32_t length) {
// there is risk to convert 64bit pointer to 32bit fifo_ptr, so sanity check is done on 64bit program
    __FIFO_INIT(fifo);
    __FIFO_SET_BASE(fifo, base);
    __FIFO_SET_LENGTH(fifo, length);
    fifo->read = 0;
    fifo->write = 0;
}

// not safe when fifo is full
static inline void fifo_enque(Fifo_t* fifo, char c) {
    __FIFO_GET_BASE(fifo)[fifo->write] = c;
    fifo->write = (fifo->write + 1) % __FIFO_GET_LENGTH(fifo);
}

// not safe when fifo is empty
static inline char fifo_deque(Fifo_t* fifo) {
    char c = __FIFO_GET_BASE(fifo)[fifo->read];
    fifo->read = (fifo->read + 1) % __FIFO_GET_LENGTH(fifo);
    return c;
}

static inline char fifo_preread(Fifo_t* fifo, uint32_t index) {  // read at (read+index)%length
    return __FIFO_GET_BASE(fifo)[(fifo->read + index) % __FIFO_GET_LENGTH(fifo)];
}

static inline char fifo_full(Fifo_t* fifo) {
    return (fifo->write + 1) % __FIFO_GET_LENGTH(fifo) == fifo->read;
}

static inline char fifo_empty(Fifo_t* fifo) {
    return (fifo->read == fifo->write);
}

static inline void fifo_clear(Fifo_t* fifo) {
    fifo->read = fifo->write;
}

// return the count of data in the buffer, however, not consistant because of no lock
static inline uint32_t fifo_count(Fifo_t* fifo) {
    return (fifo->write - fifo->read + __FIFO_GET_LENGTH(fifo)) % __FIFO_GET_LENGTH(fifo);
}
#define fifo_data_count(fifo) fifo_count(fifo)  // deprecated

static inline uint32_t fifo_remain(Fifo_t* fifo) {
    return __FIFO_GET_LENGTH(fifo) - ((fifo->write - fifo->read + __FIFO_GET_LENGTH(fifo)) % __FIFO_GET_LENGTH(fifo)) - 1;
}

// move data from src to dest with maximum length of max_length, return the byte count moved
static inline uint32_t fifo_move(Fifo_t* dest, Fifo_t* src, uint32_t max_length) {
    uint32_t i;
    for (i=0; i<max_length && !fifo_empty(src) && !fifo_full(dest); ++i) fifo_enque(dest, fifo_deque(src));
    return i;
}

static inline char* __fifo_read_base(Fifo_t* fifo) {
    return __FIFO_GET_BASE(fifo) + fifo->read;
}

static inline uint32_t __fifo_read_base_length(Fifo_t* fifo) {
    return __FIFO_GET_LENGTH(fifo) - fifo->read;
}

static inline char* __fifo_write_base(Fifo_t* fifo) {
    return __FIFO_GET_BASE(fifo) + fifo->write;
}

static inline uint32_t __fifo_write_base_length(Fifo_t* fifo) {
    return __FIFO_GET_LENGTH(fifo) - fifo->write;
}

static inline void __fifo_fullfill(Fifo_t* fifo) {  // useful for testing speed
    fifo->write = (fifo->read + __FIFO_GET_LENGTH(fifo) - 1) % __FIFO_GET_LENGTH(fifo);
}

// move data from src to dest with maximum length of max_length, return the byte count moved
static inline uint32_t fifo_move_to_buffer(char* dest, Fifo_t* src, uint32_t max_length) {
    uint32_t copylen = fifo_count(src);
    if (max_length < copylen) copylen = max_length;
    uint32_t len = __fifo_read_base_length(src);
    if (len >= copylen) {  // copy once OK
        memcpy(dest, __fifo_read_base(src), copylen);
    } else {  // need slicing
        memcpy(dest, __fifo_read_base(src), len);
        memcpy(dest + len, __FIFO_GET_BASE(src), copylen - len);
    }
    src->read = (src->read + copylen) % __FIFO_GET_LENGTH(src);  // set pointer directly
    return copylen;
}

// move data from src to dest with maximum length of max_length, return the byte count moved
static inline uint32_t fifo_copy_from_buffer(Fifo_t* dest, const char* src, uint32_t max_length) {
    uint32_t copylen = fifo_remain(dest);
    if (max_length < copylen) copylen = max_length;
    uint32_t len = __fifo_write_base_length(dest);
    if (len >= copylen) {  // copy once OK
        memcpy(__fifo_write_base(dest), src, copylen);
    } else {  // need slicing
        memcpy(__fifo_write_base(dest), src, len);
        memcpy(__FIFO_GET_BASE(dest), src + len, copylen - len);
    }
    dest->write = (dest->write + copylen) % __FIFO_GET_LENGTH(dest);  // set pointer directly
    return copylen;
}

#define fifo_dump(fifo) do {\
    uint32_t count = fifo_count(&fifo); \
    printf("fifo \"%s\": base(%p), length(%d), read(%d), write(%d), has data(%d)\n", #fifo, __FIFO_GET_BASE(&fifo), __FIFO_GET_LENGTH(&fifo), fifo.read, fifo.write, count); \
    for (uint32_t i=0; i<count; i+=16) { \
        printf("  "); \
        for (uint32_t j=0; j<16 && j+i<count; ++j) printf(" 0x%02X", (unsigned char)fifo_preread(&(fifo), i+j)); \
        printf("\n"); \
    } \
} while (0)

#endif
