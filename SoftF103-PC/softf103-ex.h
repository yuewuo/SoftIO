#define SOFTIO_USE_FUNCTION
#include "softf103.h"
#include "assert.h"
#include "serial/serial.h"
#include <chrono>
#include <thread>
#include <string>
#include <mutex>
#include "assert.h"
using namespace std;

struct SoftF103Host_t {
	serial::Serial *com;
	string port;
	bool verbose;
	uint16_t pid;  // written after device is opened
	SoftF103_Mem_t mem;
	SoftIO_t sio;
	mutex lock;
	SoftF103Host_t();
	int open(const char* port);
	int close();
#define DUMP_BASIC 0x01
#define DUMP_GPIO 0x02
#define DUMP_ADC 0x04
#define DUMP_UART 0x08
#define DUMP_SPI 0x10
	int dump(int elements = 0);
// GPIO control
	void GPIO_write(uint8_t output);
	uint8_t GPIO_read();
	void LED_set(bool opened);
};

#ifdef SOFTF103HOST_IMPLEMENTATION
#undef SOFTF103HOST_IMPLEMENTATION

SoftF103Host_t::SoftF103Host_t() {
	com = NULL;
	verbose = false;
}

int SoftF103Host_t::open(const char* _port) {
	assert(com == NULL && "device has been opened");
	// open com port
	lock.lock();
	port = _port;
	if (verbose) printf("opening device \"%s\"...\n", port.c_str());
	com = new serial::Serial(port.c_str(), 115200, serial::Timeout::simpleTimeout(1000));
	assert(com->isOpen() && "port is not opened");
	// setup softio controller
	SOFTIO_QUICK_INIT(sio, mem, Mem_FifoInit);
	sio.gets = [&](char *buffer, size_t size)->size_t {
		size_t s = com->read((uint8_t*)buffer, size);
		com->flush();
		return s;
	};
	sio.puts = [&](char *buffer, size_t size)->size_t {
		size_t s = com->write((uint8_t*)buffer, size);
		com->flush();
		return s;
	};
	sio.available = [&]()->size_t {
		return com->available();
	};
	sio.callback = [&](void* softio, SoftIO_Head_t* head)->void {
		assert(softio);
		assert(head);
	};
	// initialize device and verify it
	softio_blocking(read, sio, mem.version);
	assert(mem.version == MCU_VERSION && "version not match");
	softio_blocking(read, sio, mem.mem_size);
	assert(mem.mem_size == sizeof(mem) && "memory size not equal, this should NOT occur");
	softio_blocking(read, sio, mem.pid);
	pid = mem.pid;
	if (verbose) printf("device \"%s\" opened, version = 0x%08X, pid = 0x%04X, shared memory size = %d bytes\n", port.c_str(), mem.version, mem.pid, mem.mem_size);
	lock.unlock();
	return 0;
}

int SoftF103Host_t::close() {
	assert(com && "device not opened");
	lock.lock();
	softio_wait_all(sio);
	com->close();
	delete com;
	com = NULL;
	lock.unlock();
	return 0;
}

int SoftF103Host_t::dump(int elements) {
	assert(com && "device not opened");
	lock.lock();
	if (elements & DUMP_BASIC) {
		softio_blocking(read_between, sio, mem.status, mem.siorx_overflow);
		printf("[basic information]\n");
		printf("  1. status: 0x%02X [%s]\n", mem.status, STATUS_STR(mem.status));
		printf("  2. verbose_level: 0x%02X\n", mem.verbose_level);
		printf("  3. pid: 0x%04X\n", mem.pid);
		printf("  4. version: 0x%08X\n", mem.version);
		printf("  5. mem_size: %d\n", (int)mem.mem_size);
		printf("  6. siorx_overflow: %d\n", (int)mem.siorx_overflow);
	}
	if (elements & DUMP_GPIO) {
		softio_blocking(read_between, sio, mem.gpio_out, mem.gpio_in);
		printf("[GPIO information]\n");
		printf("  1. out (PB7-PB0): ");
		for (int i=7; i>=0; --i) printf("%d", 0x01&(mem.gpio_out>>i));
		printf("\n");
		printf("  2. in (PB15-PB8): ");
		for (int i=7; i>=0; --i) printf("%d", 0x01&(mem.gpio_in>>i));
		printf("\n");
	}
	lock.unlock();
	return 0;
}

void SoftF103Host_t::GPIO_write(uint8_t output) {
	lock.lock();
	mem.gpio_out = output;
	softio_blocking(write, sio, mem.gpio_out);
	lock.unlock();
}

uint8_t SoftF103Host_t::GPIO_read() {
	lock.lock();
	softio_blocking(read, sio, mem.gpio_in);
	lock.unlock();
	return mem.gpio_in;
}

void SoftF103Host_t::LED_set(bool opened) {
	lock.lock();
	mem.led = opened;
	softio_blocking(write, sio, mem.led);
	lock.unlock();
}

#endif
