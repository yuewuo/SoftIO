#ifndef __softio_H
#define __softio_H

/* SoftIO header-only library
 * with bidirectional memory-sync, softio provides flexible configuration and data transfer between PC and MCU
 */

// if you define this option, it will not store softio transaction headers, and will skip all responses
// however, if you enable this, read/read_fifo will not work properly, since all the responses are discarded
//#define NOT_HANDLE_RESPOND

#include "fifo.h"
#include "assert.h"
#ifdef SOFTIO_USE_FUNCTION  // for c++ lambda support
#include <functional>
#endif

// you can use softio_blocking(write, softio, xxx) for convience
#define softio_blocking(name, softio, args...) do { softio_delay_##name(softio, ##args); softio_flush(softio); softio_wait_all(softio); } while (0)
#define softio_delay(name, softio, args...) softio_delay_##name(softio, ##args)
#define softio_wait_delayed(softio) do { softio_flush(softio); softio_wait_all(softio); } while (0)
#define softio_delay_flush(name, softio, args...) do { softio_delay_##name(softio, ##args); softio_flush(softio); } while (0)
#define softio_delay_flush_try(name, softio, args...) do { softio_delay_##name(softio, ##args); softio_flush(softio); \
	assert(!!(softio).available); \
	size_t available = (softio).available(); \
	__softio_gets_fifo_blocking(&(softio), (softio).rx, available); /* this is not actually blocking, just get the current available ones */\
	softio_try_handle_all(softio); \
} while (0)
#define softio_buffered_count(softio) (((softio).write - (softio).read + (softio).length) % (softio).length)
// call below when waiting for packets
#define softio_flush_try_handle_all(softio) do { \
	softio_flush(softio); \
	softio_try_handle_all(softio); \
} while(0)

// utility functions
#define softio_is_variable_included(softio, head, var) ((head).addr<=((char*)&(var)-softio.base) && (head).addr+(head).length>=((char*)&(var)+sizeof(var)-softio.base))

typedef struct {
#define SOFTIO_HEAD_TYPE_READ 0x0
#define SOFTIO_HEAD_TYPE_WRITE 0x2
#define SOFTIO_HEAD_TYPE_READ_FIFO 0x4
#define SOFTIO_HEAD_TYPE_WRITE_FIFO 0x6
#define SOFTIO_HEAD_TYPE_CLEAR_FIFO 0x8
#define SOFTIO_HEAD_TYPE_RESET_FIFO 0xA
#define SOFTIO_HEAD_TYPE_MCU_RESET 0xC
#define SOFTIO_HEAD_TYPE_IS_REQUEST(type) (!((type)&0x01))
#define SOFTIO_HEAD_TYPE_IS_RET(type) (!!((type)&0x01))
#define SOFTIO_HEAD_TYPE_RAW(type) ((type)&0x0E)
#define SOFTIO_HEAD_TYPE_STR(type) (\
	SOFTIO_HEAD_TYPE_RAW(type) == SOFTIO_HEAD_TYPE_READ ? "read" : (\
	SOFTIO_HEAD_TYPE_RAW(type) == SOFTIO_HEAD_TYPE_WRITE ? "write" : (\
	SOFTIO_HEAD_TYPE_RAW(type) == SOFTIO_HEAD_TYPE_READ_FIFO ? "readfifo" : (\
	SOFTIO_HEAD_TYPE_RAW(type) == SOFTIO_HEAD_TYPE_WRITE_FIFO ? "writefifo" : (\
	SOFTIO_HEAD_TYPE_RAW(type) == SOFTIO_HEAD_TYPE_CLEAR_FIFO ? "clearfifo" : (\
	SOFTIO_HEAD_TYPE_RAW(type) == SOFTIO_HEAD_TYPE_RESET_FIFO ? "resetfifo" : (\
	SOFTIO_HEAD_TYPE_RAW(type) == SOFTIO_HEAD_TYPE_MCU_RESET ? "mcu_reset" : (\
"unknown" ))))))))
#define SOFTIO_HEAD_TYPE_REQRET_STR(type) (SOFTIO_HEAD_TYPE_IS_REQUEST(type) ? "request" : "ret")
	uint32_t type   : 4;
	uint32_t addr   : 20;
	uint32_t length : 8;
} SoftIO_Head_t;

static inline void __softio_head_enque(Fifo_t* tx, SoftIO_Head_t* head) {
	assert((size_t)fifo_remain(tx) >= sizeof(SoftIO_Head_t) && "cannot push head inside fifo");
	char* buf = (char*)(void*)head;
	for (size_t i=0; i < sizeof(SoftIO_Head_t); ++i) fifo_enque(tx, buf[i]);  // little endian
}

static inline void __softio_head_preread(Fifo_t* rx, SoftIO_Head_t* head) {
	assert((size_t)fifo_count(rx) >= sizeof(SoftIO_Head_t) && "is not a head");
	char* buf = (char*)(void*)head;
	for (size_t i=0; i < sizeof(SoftIO_Head_t); ++i) buf[i] = fifo_preread(rx, i);
}
static inline void __softio_head_deque(Fifo_t* rx, SoftIO_Head_t* head) {
	assert((size_t)fifo_count(rx) >= sizeof(SoftIO_Head_t) && "is not a head");
	char* buf = (char*)(void*)head;
	for (size_t i=0; i < sizeof(SoftIO_Head_t); ++i) buf[i] = fifo_deque(rx);
}

// you can define the maximum pending transactions here. You should be careful to match the initiater call and defination must match the length.
// by the way, the default 32 is enough in most cases
#ifndef SOFTIO_HEAD_LENGTH
#define SOFTIO_HEAD_LENGTH 32
#endif

typedef struct {
	// uint8_t status;  // TODO
	uint16_t length;  // a simple fifo here
	uint16_t write;
	uint16_t read;
	SoftIO_Head_t transactions[SOFTIO_HEAD_LENGTH];
	char* base;  // the base pointer of memory
	uint32_t size;  // the size of memory
	Fifo_t* rx;
	Fifo_t* tx;
// the fifo structure should be aligned one-by-one, otherwise set fifo_begin=0 and fifo_end=-1 to disable fifo_region check (NOT RECOMMENDED!!)
// a simple setting is to put all Fifo_t at the end of shared memory space, and rx at the begining with tx behind it.
//   in this condition you could call `softio_init` to auto set the fifo region check parameters.
	uint32_t fifo_begin;
	uint32_t fifo_end;
#ifndef SOFTIO_USE_FUNCTION
// check function: you can define the restricted area of operation or anything else you like
// you can even redirect read/write by set the addr in head!
	void (*before) (void* softio, SoftIO_Head_t* head);
// after function: you can process something after host read/write some memory from/to you
	void (*after) (void* softio, SoftIO_Head_t* head);
// callback function: read/write request fininsh callback
	void (*callback) (void* softio, SoftIO_Head_t* head);

// the following needs to be implemented
	size_t (*available) ();  // this will return current available bytes to gets(), useful to try handle
// gets function: get several bytes, return the actual bytes been read, if not provided,` while(1) yield()`; will be called for waiting rx fifo received
// softio will retry it to get enough data, so if you wanna exit the program if read timeout, do it yourself in `gets()` function!
	size_t (*gets) (char *buffer, size_t size);
// puts function: put several bytes, return the actual bytes written, if not provided, `while(1) yield();` will be called for waiting tx fifo sent
	size_t (*puts) (char *buffer, size_t size);
// yield function: for system with OS, if gets and puts is not provided, `while (1) yield;` will be called. for MCU, a NULL is OK for `while(1);`
	void (*yield) ();
#else
	std::function<void(void*, SoftIO_Head_t*)> before;
	std::function<void(void*, SoftIO_Head_t*)> after;
	std::function<void(void*, SoftIO_Head_t*)> callback;
	std::function<size_t()> available;
	std::function<size_t(char*, size_t)> gets;
	std::function<size_t(char*, size_t)> puts;
	std::function<void()> yield;
#endif
} SoftIO_t;

static inline void softio_init(SoftIO_t* softio, void* base, uint32_t size, Fifo_t* rx, Fifo_t* tx) {
	assert(sizeof(SoftIO_Head_t) == 4 && "head must be 4 byte");
	assert(tx == rx + 1 && "see comments above");  // tx is behind rx
	assert((char*)base <= (char*)rx && (char*)base + size >= (char*)rx + sizeof(Fifo_t) && "rx not in shared memory");
	assert((char*)base <= (char*)tx && (char*)base + size >= (char*)tx + sizeof(Fifo_t) && "tx not in shared memory");
	assert(((char*)base + size - (char*)rx) % sizeof(Fifo_t) == 0 && "see comments above");  // check whether memory behind it is fifo. not full check.
	softio->length = SOFTIO_HEAD_LENGTH;
	softio->read = 0;
	softio->write = 0;
	softio->base = (char*)base;
	softio->size = size;
	softio->rx = rx;
	softio->tx = tx;
	softio->fifo_begin = (char*)rx - (char*)base;
	softio->fifo_end = size;
	softio->before = NULL;
	softio->after = NULL;
	softio->callback = NULL;
	softio->gets = NULL;
	softio->puts = NULL;
	softio->yield = NULL;
}
// for special structure, basically: 1. siorx as the first fifo with siotx behind 2. defined a fifo initialization function somewhere
// can call this simplified initialization step, for both side
#define SOFTIO_QUICK_INIT(sio, mem, initfunc) do { \
	initfunc(mem); \
	softio_init(&(sio), &(mem), sizeof(mem), &((mem).siorx), &((mem).siotx)); \
} while (0)

// successfully handle one returns 0, otherwise return the byte needed (including existed) to read (>0), or the byte need to write (total) (<0)
#define SOFTIO_HANDLE_NEED_READ(need) if ( fifo_count(softio->rx) < (need) ) return (need)
#define SOFTIO_HANDLE_NEED_WRITE(need) if ( fifo_remain(softio->tx) < (need) ) return - (need)
static inline int __softio_try_handle_one(SoftIO_t* softio) {
	if (fifo_empty(softio->rx)) return 1; // no message in, just need 1 byte
	uint32_t type = 0x0F & fifo_preread(softio->rx, 0);
	uint32_t length;
	SoftIO_Head_t head;
	Fifo_t* fptr;
	char sum;
	// printf("type: %u\n", type);
	if (SOFTIO_HEAD_TYPE_IS_RET(type)) {  // respond
#ifdef NOT_HANDLE_RESPOND
		switch (type & 0x0E) {
		case SOFTIO_HEAD_TYPE_READ:
			SOFTIO_HANDLE_NEED_READ(2);  // length not ready
			length = (unsigned char)fifo_preread(softio->rx, 1);
			SOFTIO_HANDLE_NEED_READ(length + 3);  // data not ready
			for (uint32_t i=0; i<length+3; ++i) fifo_deque(softio->rx);
			break;
		case SOFTIO_HEAD_TYPE_WRITE:
			SOFTIO_HANDLE_NEED_READ(2);  // length not ready
			fifo_deque(softio->rx);  // get type out
			fifo_deque(softio->rx);
			break;
		case SOFTIO_HEAD_TYPE_READ_FIFO:
			SOFTIO_HANDLE_NEED_READ(2);  // length not ready
			length = (unsigned char)fifo_preread(softio->rx, 1);
			SOFTIO_HANDLE_NEED_READ(length + 3);  // data not ready
			for (uint32_t i=0; i<length+3; ++i) fifo_deque(softio->rx);
			break;
		case SOFTIO_HEAD_TYPE_WRITE_FIFO:
			SOFTIO_HANDLE_NEED_READ(2);  // length not ready
			fifo_deque(softio->rx);  // get type out
			fifo_deque(softio->rx);
			break;
		case SOFTIO_HEAD_TYPE_CLEAR_FIFO:
		case SOFTIO_HEAD_TYPE_RESET_FIFO:
			SOFTIO_HANDLE_NEED_READ(1);  // length not ready
			fifo_deque(softio->rx);  // get type out
			break;
		default:
			assert(0 && "invalid respond");
		}
#else
		assert(softio->read != softio->write && "transaction empty but receive respond");
		SoftIO_Head_t* rptr = softio->transactions + softio->read;
		assert(SOFTIO_HEAD_TYPE_RAW(type) == rptr->type && "ret and request must be the same type");
		switch (rptr->type) {
		case SOFTIO_HEAD_TYPE_READ:
			SOFTIO_HANDLE_NEED_READ(2);  // length not ready
			length = (unsigned char)fifo_preread(softio->rx, 1);
			// printf("length = %u, rptr->length = %u\n", length, rptr->length);
			assert(length == rptr->length && "read transaction length not equal");
			SOFTIO_HANDLE_NEED_READ(length + 3);  // data not ready
			// then data is ready! compute the checksum and write data into local memory
			sum = fifo_preread(softio->rx, 2+length);
			for (uint32_t i=0; i<length; ++i) sum += fifo_preread(softio->rx, 2+i);
			assert(sum == 0 && "checksum is non-zero");
			fifo_deque(softio->rx); fifo_deque(softio->rx);  // get type and length
			for (uint32_t i=0; i<length; ++i) softio->base[rptr->addr + i] = fifo_deque(softio->rx);
			fifo_deque(softio->rx); // get checksum out of fifo
			break;
		case SOFTIO_HEAD_TYPE_WRITE:
			SOFTIO_HANDLE_NEED_READ(2);  // length not ready
			fifo_deque(softio->rx);  // get type out
			length = (unsigned char)fifo_deque(softio->rx);
			// printf("length = %u, rptr->length = %u\n", length, rptr->length);
			assert(length == rptr->length && "write transaction length not equal");
			break;
		case SOFTIO_HEAD_TYPE_READ_FIFO:
			SOFTIO_HANDLE_NEED_READ(2);  // length not ready
			length = (unsigned char)fifo_preread(softio->rx, 1);
			assert(length <= rptr->length && "read fifo transaction length greater");
			SOFTIO_HANDLE_NEED_READ(length + 3);  // data not ready
			sum = fifo_preread(softio->rx, 2+length);
			for (uint32_t i=0; i<length; ++i) sum += fifo_preread(softio->rx, 2+i);
			assert(sum == 0 && "checksum is non-zero");
			fifo_deque(softio->rx); fifo_deque(softio->rx);  // get type and length
			for (uint32_t i=0; i<length; ++i) fifo_enque((Fifo_t*)(softio->base + rptr->addr), fifo_deque(softio->rx));
			fifo_deque(softio->rx); // get checksum out of fifo
			break;
		case SOFTIO_HEAD_TYPE_WRITE_FIFO:
			SOFTIO_HANDLE_NEED_READ(2);  // length not ready
			fifo_deque(softio->rx);  // get type out
			length = (unsigned char)fifo_deque(softio->rx);
			assert(length == rptr->length && "write transaction length not equal");
			break;
		case SOFTIO_HEAD_TYPE_CLEAR_FIFO:
		case SOFTIO_HEAD_TYPE_RESET_FIFO:
			SOFTIO_HANDLE_NEED_READ(1);  // length not ready
			fifo_deque(softio->rx);  // get type out
			break;
		default:
			assert(0 && "invalid respond");
		}
		if (softio->callback) softio->callback(softio, rptr);
		softio->read = (softio->read + 1) % softio->length;  // delete this transaction
#endif
	} else {  // request
		switch (type) {
		case SOFTIO_HEAD_TYPE_READ:
			SOFTIO_HANDLE_NEED_READ(4);  // length not ready
			__softio_head_preread(softio->rx, &head);  // do not get the head, simply because space may not available now
			assert(head.length != 0 && head.length != 255 && "invalid length");
			assert((uint32_t)(head.addr + head.length) <= softio->size && "read outside shared space");
			SOFTIO_HANDLE_NEED_WRITE((uint32_t)(3 + head.length));  // fifo is not ready for reply
			__softio_head_deque(softio->rx, &head);  // really get head
			if (softio->before) softio->before(softio, &head);
			fifo_enque(softio->tx, (SOFTIO_HEAD_TYPE_READ | 0x01));
			fifo_enque(softio->tx, head.length);
			sum=0; for (uint32_t i=0; i<head.length; ++i) {
				sum += softio->base[head.addr + i];
				fifo_enque(softio->tx, softio->base[head.addr + i]);
			}
			fifo_enque(softio->tx, -sum);
			break;
		case SOFTIO_HEAD_TYPE_WRITE:
			SOFTIO_HANDLE_NEED_READ(4);  // length not ready
			__softio_head_preread(softio->rx, &head);  // do not get the head, simply because data may not available now
			assert(head.length != 0 && head.length != 255 && "invalid length");
			assert((uint32_t)(head.addr + head.length) <= softio->size && "write outside shared space");
			SOFTIO_HANDLE_NEED_READ((uint32_t)(4 + head.length + 1));  // data not ready
			SOFTIO_HANDLE_NEED_WRITE(2);  // fifo is not ready for reply
			__softio_head_deque(softio->rx, &head);  // really get head
			if (softio->before) softio->before(softio, &head);
			sum=0; for (uint32_t i=0; i<(uint32_t)(head.length + 1); ++i) {  // including checksum
				sum += fifo_preread(softio->rx, i);
			} assert(sum == 0 && "check sum failed for write");
			for (uint32_t i=0; i<head.length; ++i) {  // actually write into local memory
				softio->base[head.addr + i] = fifo_deque(softio->rx);
			} fifo_deque(softio->rx);  // get checksum outside
			fifo_enque(softio->tx, (SOFTIO_HEAD_TYPE_WRITE | 0x01));
			fifo_enque(softio->tx, head.length);
			break;
		case SOFTIO_HEAD_TYPE_READ_FIFO:
			SOFTIO_HANDLE_NEED_READ(4);  // length not ready
			__softio_head_preread(softio->rx, &head);  // do not get the head, simply because space may not available now
			assert(head.length != 0 && head.length != 255 && "invalid length");
			assert((uint32_t)head.addr >= softio->fifo_begin && (uint32_t)(head.addr + sizeof(Fifo_t)) <= softio->fifo_end && "read fifo outside valid space");
			assert((head.addr - softio->fifo_begin) % sizeof(Fifo_t) == 0 && "read fifo alignment error");
			fptr = (Fifo_t*)(softio->base + head.addr);
			length = head.length;
			if (fifo_count(fptr) < length) length = fifo_count(fptr);  // only have these
			SOFTIO_HANDLE_NEED_WRITE((uint32_t)(3 + length));  // fifo is not ready for reply
			__softio_head_deque(softio->rx, &head);  // really get head
			if (softio->before) softio->before(softio, &head);
			fifo_enque(softio->tx, (SOFTIO_HEAD_TYPE_READ_FIFO | 0x01));
			fifo_enque(softio->tx, length);
			sum=0; for (uint32_t i=0; i<length; ++i) {
				char a = fifo_deque(fptr); sum += a;
				fifo_enque(softio->tx, a);
			}
			fifo_enque(softio->tx, -sum);
			break;
		case SOFTIO_HEAD_TYPE_WRITE_FIFO:
			SOFTIO_HANDLE_NEED_READ(4);  // length not ready
			__softio_head_preread(softio->rx, &head);  // do not get the head, simply because space may not available now
			assert(head.length != 0 && head.length != 255 && "invalid length");
			assert((uint32_t)head.addr >= softio->fifo_begin && (uint32_t)(head.addr + sizeof(Fifo_t)) <= softio->fifo_end && "write fifo outside valid space");
			assert((head.addr - softio->fifo_begin) % sizeof(Fifo_t) == 0 && "write fifo alignment error");
			SOFTIO_HANDLE_NEED_READ((uint32_t)(4 + head.length + 1));  // data not ready
			SOFTIO_HANDLE_NEED_WRITE(2);  // fifo is not ready for reply
			__softio_head_deque(softio->rx, &head);  // really get head
			if (softio->before) softio->before(softio, &head);
			sum=0; for (uint32_t i=0; i<(uint32_t)(head.length + 1); ++i) {  // including checksum
				sum += fifo_preread(softio->rx, i);
			} assert(sum == 0 && "check sum failed for write");
			fptr = (Fifo_t*)(softio->base + head.addr);
			length = head.length;
			assert(length <= fifo_remain(fptr) && "fifo is not enough to write");
			for (uint32_t i=0; i<length; ++i) {  // actually write into local memory
				fifo_enque(fptr, fifo_deque(softio->rx));
			} for (uint32_t i=0; i<head.length - length; ++i) fifo_deque(softio->rx);  // get overflowed ones
			fifo_deque(softio->rx);  // get checksum outside
			fifo_enque(softio->tx, (SOFTIO_HEAD_TYPE_WRITE_FIFO | 0x01));
			fifo_enque(softio->tx, length);
			break;
		case SOFTIO_HEAD_TYPE_CLEAR_FIFO:
		case SOFTIO_HEAD_TYPE_RESET_FIFO:
			SOFTIO_HANDLE_NEED_READ(4);  // length not ready
			SOFTIO_HANDLE_NEED_WRITE(1);  // fifo is not ready for reply
			__softio_head_deque(softio->rx, &head);  // really get head
			if (softio->before) softio->before(softio, &head);
			assert(head.length == 0 && "invalid length, must be 0");
			assert((uint32_t)head.addr >= softio->fifo_begin && (uint32_t)(head.addr + sizeof(Fifo_t)) <= softio->fifo_end && "write fifo outside valid space");
			assert((head.addr - softio->fifo_begin) % sizeof(Fifo_t) == 0 && "write fifo alignment error");
			if (type == SOFTIO_HEAD_TYPE_CLEAR_FIFO) fifo_clear((Fifo_t*)(softio->base + head.addr));
			else {
				fptr = (Fifo_t*)(softio->base + head.addr);
				fptr->write = 0; fptr->read = 0;
			}
			fifo_enque(softio->tx, (type | 0x01));
			break;
		default:
			assert(0 && "invalid request");
		}
		if (softio->after) softio->after(softio, &head);
	}
	return 0;
}
#define softio_try_handle_one(softio) __softio_try_handle_one(&(softio))

static inline void __softio_try_handle_all(SoftIO_t* softio) {
	while (__softio_try_handle_one(softio) == 0);
}
#define softio_try_handle_all(softio) __softio_try_handle_all(&(softio))

static inline void __softio_gets_fifo_blocking(SoftIO_t* softio, Fifo_t* fifo, size_t size) {  // wait for fifo_count > size
	assert(__FIFO_GET_LENGTH(fifo) > size);
	if (!softio->gets) { while (fifo_count(fifo) < size) if (softio->yield) softio->yield(); }
	else {  // use gets function to get bytes, note that fifo may not be continuous so just do it
		while (fifo_count(fifo) < size) {  // always try
			size_t length = size - fifo_count(fifo);
			if (length > __fifo_write_base_length(fifo)) length = __fifo_write_base_length(fifo);
			size_t ret = softio->gets(__fifo_write_base(fifo), length);
			fifo->write = (fifo->write + ret) % __FIFO_GET_LENGTH(fifo);
		}
	}
}
#define softio_flush_fifo(softio, fifo) __softio_puts_fifo_blocking(&(softio), &(fifo), __FIFO_GET_LENGTH(&(fifo)) - 1)
#define softio_flush(softio) do { \
	softio_flush_fifo(softio, (*(softio).tx)); \
	if ((softio).available) { \
		unsigned int wait_cnt = (softio).available() + fifo_count((softio).rx); \
		if (wait_cnt >= __FIFO_GET_LENGTH(((softio).rx))) wait_cnt = __FIFO_GET_LENGTH(((softio).rx)) - 2; \
		__softio_gets_fifo_blocking(&(softio), (softio).rx, wait_cnt); \
	} \
} while(0)
static inline void __softio_puts_fifo_blocking(SoftIO_t* softio, Fifo_t* fifo, size_t size) {  // wait for fifo_remain > size
	assert(__FIFO_GET_LENGTH(fifo) > size);
	if (!softio->puts) { while (fifo_count(fifo) < size) if (softio->yield) softio->yield(); }
	else {  // use puts function to put bytes
		while (fifo_remain(fifo) < size) {  // always try
			size_t length = size - fifo_remain(fifo);
			if (length > __fifo_read_base_length(fifo)) length = __fifo_read_base_length(fifo);
			size_t ret = softio->puts(__fifo_read_base(fifo), length);
			fifo->read = (fifo->read + ret) % __FIFO_GET_LENGTH(fifo);
		}
	}
}
static inline void __softio_wait_one(SoftIO_t* softio) {
	// first flush it
	softio_flush(*softio);
	if (fifo_empty(softio->rx) && softio->read != softio->write) {  // need respond
		__softio_gets_fifo_blocking(softio, softio->rx, 1);  // get 1 byte to process
	}
	int need = __softio_try_handle_one(softio);
	while (need != 0) {
		if (need > 0) {  // need to read
			__softio_gets_fifo_blocking(softio, softio->rx, need);
		} else {  // need to write
			__softio_puts_fifo_blocking(softio, softio->tx, -need);
		}
		need = __softio_try_handle_one(softio);  // retry it
	}
}
#define softio_wait_one(softio) __softio_wait_one(&(softio))
#define softio_wait_all(softio) do { \
	while ((softio).read != (softio).write) softio_wait_one(softio); \
} while (0)

static inline void __softio_delay_read_no_check(SoftIO_t* softio, uint32_t addr, uint32_t length) {
#ifndef NOT_HANDLE_RESPOND
	if ((softio->write + 1) % softio->length == softio->read) __softio_wait_one(softio);  // queue is full, wait one
	while (fifo_remain(softio->tx) < 4) __softio_wait_one(softio);  // sending queue is full, wait
#endif
	SoftIO_Head_t* tptr = softio->transactions + softio->write;
	tptr->type = SOFTIO_HEAD_TYPE_READ;
	tptr->addr = addr;
	tptr->length = length;
	// printf("length %u\n", tptr->length);
#ifndef NOT_HANDLE_RESPOND
	softio->write = (softio->write + 1) % softio->length;
#endif
	__softio_head_enque(softio->tx, tptr);
}
static inline void __softio_delay_read(SoftIO_t* softio, void* addr, uint32_t length) {
	assert(softio->base <= (char*)addr && softio->base + softio->size >= (char*)addr + length && "read range exceeded");
	uint32_t bias = 0;
	uint32_t startaddr = (char*)addr - softio->base;
	while (bias < length) {
		int len = (length - bias) > 254 ? 254 : (length - bias);  // maximum length is 254
		__softio_delay_read_no_check(softio, startaddr + bias, len);
		bias += len;
	}
}
#define softio_delay_read(softio, var) __softio_delay_read(&(softio), &(var), sizeof(var))
#define softio_delay_read_between(softio, var1, var2) __softio_delay_read(&(softio), &(var1), (char*)(void*)(&(var2)) - (char*)(void*)(&(var1)) + sizeof(var2))

static inline void __softio_delay_write_no_check(SoftIO_t* softio, uint32_t addr, uint32_t length) {
#ifndef NOT_HANDLE_RESPOND
	if ((softio->write + 1) % softio->length == softio->read) __softio_wait_one(softio);  // queue is full, wait one
	while (fifo_remain(softio->tx) < 5 + length) __softio_wait_one(softio);  // sending queue is full, wait
#endif
	SoftIO_Head_t* tptr = softio->transactions + softio->write;
	tptr->type = SOFTIO_HEAD_TYPE_WRITE;
	tptr->addr = addr;
	tptr->length = length;
#ifndef NOT_HANDLE_RESPOND
	softio->write = (softio->write + 1) % softio->length;
#endif
	__softio_head_enque(softio->tx, tptr);
	char sum = 0;
	for (uint32_t i=0; i<length; ++i) {
		sum += softio->base[addr + i];
		fifo_enque(softio->tx, softio->base[addr + i]);
	}
	fifo_enque(softio->tx, -sum);
}
static inline void __softio_delay_write(SoftIO_t* softio, void* addr, uint32_t length) {
	assert(softio->base <= (char*)addr && softio->base + softio->size >= (char*)addr + length && "write range exceeded");
	uint32_t bias = 0;
	uint32_t startaddr = (char*)addr - softio->base;
	while (bias < length) {
		int len = (length - bias) > 254 ? 254 : (length - bias);  // maximum length is 254
		__softio_delay_write_no_check(softio, startaddr + bias, len);
		bias += len;
	}
}
#define softio_delay_write(softio, var) __softio_delay_write(&(softio), &(var), sizeof(var))
#define softio_delay_write_between(softio, var1, var2) __softio_delay_write(&(softio), &(var1), (char*)(void*)(&(var2)) - (char*)(void*)(&(var1)) + sizeof(var2))

static inline void __softio_delay_read_fifo_no_check(SoftIO_t* softio, uint32_t addr, uint32_t length) {
#ifndef NOT_HANDLE_RESPOND
	if ((softio->write + 1) % softio->length == softio->read) __softio_wait_one(softio);  // queue is full, wait one
	while (fifo_remain(softio->tx) < 4) __softio_wait_one(softio);  // sending queue is full, wait
#endif
	SoftIO_Head_t* tptr = softio->transactions + softio->write;
	tptr->type = SOFTIO_HEAD_TYPE_READ_FIFO;
	tptr->addr = addr;
	tptr->length = length;
#ifndef NOT_HANDLE_RESPOND
	softio->write = (softio->write + 1) % softio->length;
#endif
	__softio_head_enque(softio->tx, tptr);
}
static inline void __softio_delay_read_fifo(SoftIO_t* softio, Fifo_t* addr, uint32_t length) {
	assert(softio->base <= (char*)addr && softio->base + softio->size >= (char*)addr + sizeof(Fifo_t) && "read fifo range exceeded");
	assert(length >= 1 && length < 255 && "fifo read length invalid");
	__softio_delay_read_fifo_no_check(softio, (char*)addr - softio->base, length);
}
// if you want more than 254 byte for one time, you need to do more things to make sure it is correct.
// 	   it must be a blocking function since fifo_read may read less than what you've requested
#define softio_delay_read_fifo_part(softio, var, length) __softio_delay_read_fifo(&(softio), &(var), length)
#define softio_delay_read_fifo(softio, var) softio_delay_read_fifo_part(softio, var, 254)

static inline void __softio_delay_write_fifo_no_check(SoftIO_t* softio, uint32_t addr, uint32_t length, Fifo_t* fifo) {
#ifndef NOT_HANDLE_RESPOND
	if ((softio->write + 1) % softio->length == softio->read) __softio_wait_one(softio);  // queue is full, wait one
	while (fifo_remain(softio->tx) < 5 + length) __softio_wait_one(softio);  // sending queue is full, wait
#endif
	SoftIO_Head_t* tptr = softio->transactions + softio->write;
	tptr->type = SOFTIO_HEAD_TYPE_WRITE_FIFO;
	tptr->addr = addr;
	tptr->length = length;
#ifndef NOT_HANDLE_RESPOND
	softio->write = (softio->write + 1) % softio->length;
#endif
	__softio_head_enque(softio->tx, tptr);
	char sum = 0;
	for (uint32_t i=0; i<length; ++i) {
		char a = fifo_deque(fifo);
		sum += a;
		fifo_enque(softio->tx, a);
	}
	fifo_enque(softio->tx, -sum);
}
static inline void __softio_delay_write_fifo(SoftIO_t* softio, Fifo_t* addr, uint32_t length) {
	assert(softio->base <= (char*)addr && softio->base + softio->size >= (char*)addr + sizeof(Fifo_t) && "write fifo range exceeded");
	assert(length >= 1 && length < 255 && "fifo write length invalid");
	if (length > fifo_count(addr)) length = fifo_count(addr);
	if (length != 0) __softio_delay_write_fifo_no_check(softio, (char*)addr - softio->base, length, addr);
}
#define softio_delay_write_fifo_part(softio, var, length) __softio_delay_write_fifo(&(softio), &(var), length)
#define softio_delay_write_fifo(softio, var) softio_delay_write_fifo_part(softio, var, 254)

static inline void __softio_delay_clear_reset_fifo(SoftIO_t* softio, Fifo_t* addr, uint32_t type) {
	assert(softio->base <= (char*)addr && softio->base + softio->size >= (char*)addr + sizeof(Fifo_t) && "fifo range exceeded");
#ifndef NOT_HANDLE_RESPOND
	if ((softio->write + 1) % softio->length == softio->read) __softio_wait_one(softio);  // queue is full, wait one
	while (fifo_remain(softio->tx) < 4) __softio_wait_one(softio);  // sending queue is full, wait
#endif
	SoftIO_Head_t* tptr = softio->transactions + softio->write;
	tptr->type = type;
	tptr->addr = (char*)addr - softio->base;
	tptr->length = 0;
#ifndef NOT_HANDLE_RESPOND
	softio->write = (softio->write + 1) % softio->length;
#endif
	__softio_head_enque(softio->tx, tptr);
}
#define softio_delay_clear_fifo(softio, var) __softio_delay_clear_reset_fifo(&(softio), &(var), SOFTIO_HEAD_TYPE_CLEAR_FIFO)
#define softio_delay_reset_fifo(softio, var) __softio_delay_clear_reset_fifo(&(softio), &(var), SOFTIO_HEAD_TYPE_RESET_FIFO)

#define softio_dump(softio) do {\
	int count = ((softio).write - (softio).read + (softio).length) % (softio).length;\
	printf("softio \"%s\": base(0x%p), size(%u), transaction: length(%d), read(%d), write(%d), has (%d)\n", \
		#softio, (softio).base, (softio).size, (softio).length, (softio).read, (softio).write, count);\
	for (int i=0; i<count; ++i) {\
		printf("   %2d: %s-%s addr(0x%05X) length(%u) raw(0x%08X)\n", i, SOFTIO_HEAD_TYPE_STR((softio).transactions[((softio).read + i) % (softio).length].type), \
			SOFTIO_HEAD_TYPE_REQRET_STR((softio).transactions[((softio).read + i) % (softio).length].type), \
			(softio).transactions[((softio).read + i) % (softio).length].addr, (softio).transactions[((softio).read + i) % (softio).length].length, \
			*(uint32_t*)((void*)((softio).transactions + (((softio).read + i) % (softio).length)))); \
	}\
} while(0)

// WARNING: could only dump 32bit machine's fifo
#if __SIZEOF_POINTER__ == 8
#define softio_protected_dump_remote_fifo(prefix, softio, var) do {\
	assert(&(var) != (softio).rx && &(var) != (softio).tx && "dump remote rx and tx not supported");\
	softio_wait_all(softio);  /* finish all the transactions buffered */\
	Fifo_t tmp = var;  /* save current state of local fifo */\
	softio_blocking(read, softio, var);\
	printf(prefix #var ": usage(%d/%d), read(%d), write(%d)\n", ((var).write - (var).read + (var).__H) % (var).__H, (var).__H, (var).read, (var).write);\
	var = tmp;  /* restore state of local fifo */\
} while (0)
#else
#define softio_protected_dump_remote_fifo(prefix, softio, var) do {\
	assert(&(var) != (softio).rx && &(var) != (softio).tx && "dump remote rx and tx not supported");\
	softio_wait_all(softio);  /* finish all the transactions buffered */\
	Fifo_t tmp = var;  /* save current state of local fifo */\
	softio_blocking(read, softio, var);\
	printf(prefix #var ": usage(%d/%d), read(%d), write(%d)\n", ((var).write - (var).read + (var).length) % (var).length, (var).length, (var).read, (var).write);\
	var = tmp;  /* restore state of local fifo */\
} while (0)
#endif

#endif
