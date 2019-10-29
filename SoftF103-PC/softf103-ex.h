#define SOFTIO_USE_FUNCTION
#include "softf103.h"
#include "assert.h"
#include "serial/serial.h"
#include <chrono>
#include <thread>
#include <string>
#include <mutex>
#include<algorithm>
#include <vector>
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
#define DUMP_TIMER 0x20
	int dump(int elements = 0);
// GPIO control
	void GPIO_write(uint8_t output);
	uint8_t GPIO_read();
	void GPIO_streaming(float frequency, vector<uint8_t> samples);
	void LED_set(bool opened);
// Timer control: timer = 1 or 2
	pair<float, float> Timer_Start_PWM(int timer, float frequency, float duty);
	float Timer_Start_IT(int timer, float frequency);
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
	if (elements & DUMP_TIMER) {
		softio_blocking(read_between, sio, mem.tim1_PWM, mem.tim2_pulse);
		printf("[Timer information]\n");
		const float clock = 72e6;
		printf("  1. tim1:\n");
		printf("       prescaler: 0x%04X, period: 0x%04X, pulse: 0x%04X\n", mem.tim1_prescaler, mem.tim1_period, mem.tim1_pulse);
		float freq1 = clock / (mem.tim1_prescaler + 1.f) / (mem.tim1_period + 1.f);
		float duty1 = (mem.tim1_pulse + 1.f) / (mem.tim1_period + 1.f);
		printf("       frequency: %f , duty: %f %%\n", freq1/1e3, duty1*100);
		printf("  2. tim2:\n");
		printf("       prescaler: 0x%04X, period: 0x%04X, pulse: 0x%04X\n", mem.tim2_prescaler, mem.tim2_period, mem.tim2_pulse);
		float freq2 = clock / (mem.tim2_prescaler + 1.f) / (mem.tim2_period + 1.f);
		float duty2 = (mem.tim2_pulse + 1.f) / (mem.tim2_period + 1.f);
		printf("       frequency: %f , duty: %f %%\n", freq2/1e3, duty2*100);
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

pair<float, float> SoftF103Host_t::Timer_Start_PWM(int timer, float frequency, float duty) {
	assert((timer == 1 || timer == 2) && "invalid timer number");
	const float clock = 72e6;
	float period_target = clock / frequency - 0.5;
	assert(period_target >= 0 && period_target < INT32_MAX && "invalid frequency");
	uint16_t prescaler = 1;
	while (period_target > 65536) {
		prescaler *= 2;
		period_target /= 2;
	}
	prescaler -= 1;
	uint16_t period = period_target;
	float frequency_real = clock / (prescaler + 1.f) / (period + 1.f);
	float pulse_target = (period + 1.f) * duty - 0.5;
	uint16_t pulse = pulse_target;
	float duty_real = (pulse + 1.f) / (period + 1.f);
	lock.lock();
	if (timer == 1) {
		mem.tim1_prescaler = prescaler;
		mem.tim1_period = period;
		mem.tim1_pulse = pulse;
		softio_blocking(write_between, sio, mem.tim1_prescaler, mem.tim1_pulse);
		mem.tim1_PWM = 1;
		softio_blocking(write, sio, mem.tim1_PWM);
	} else {
		mem.tim2_prescaler = prescaler;
		mem.tim2_period = period;
		mem.tim2_pulse = pulse;
		softio_blocking(write_between, sio, mem.tim2_prescaler, mem.tim2_pulse);
		mem.tim2_PWM = 1;
		softio_blocking(write, sio, mem.tim2_PWM);
	}
	lock.unlock();
	return make_pair(frequency_real, duty_real);
}

float SoftF103Host_t::Timer_Start_IT(int timer, float frequency) {
	assert((timer == 1 || timer == 2) && "invalid timer number");
	const float clock = 72e6;
	float period_target = clock / frequency - 0.5;
	assert(period_target >= 0 && period_target <= INT32_MAX && "invalid frequency");
	uint16_t prescaler = 1;
	while (period_target > 65536) {
		prescaler *= 2;
		period_target /= 2;
	}
	prescaler -= 1;
	uint32_t period = period_target;
	float frequency_real = clock / (prescaler + 1.f) / period;
	lock.lock();
	if (timer == 1) {
		mem.tim1_prescaler = prescaler;
		mem.tim1_period = period;
		softio_blocking(write_between, sio, mem.tim1_prescaler, mem.tim1_period);
		mem.tim1_IT = 1;
		softio_blocking(write, sio, mem.tim1_IT);
	} else {
		mem.tim2_prescaler = prescaler;
		mem.tim2_period = period;
		softio_blocking(write_between, sio, mem.tim2_prescaler, mem.tim2_period);
		mem.tim2_IT = 1;
		softio_blocking(write, sio, mem.tim2_IT);
	}
	lock.unlock();
	return frequency_real;
}

void SoftF103Host_t::GPIO_streaming(float frequency, vector<uint8_t> samples) {
	mem.gpio_count = 0;
	mem.gpio_underflow = 0;
	softio_blocking(write_between, sio, mem.gpio_count, mem.gpio_underflow);  // clear previous count and underflow count
	softio_blocking(reset_fifo, sio, mem.fifo0);  // reset fifo0
	fifo_clear(&mem.fifo0);
	float actual = Timer_Start_IT(1, frequency);  // start timer
	if (verbose) printf("GPIO streaming frequency: %f kHz\n", actual/1e3);
	// first fullfill the fifo
	uint32_t written_cnt = 0;  // the length of sent
	for (uint32_t i=0; i<samples.size() && !fifo_full(&mem.fifo0); ++i) {
		fifo_enque(&mem.fifo0, samples[written_cnt]);
		++written_cnt;\
	}
	while (!fifo_empty(&mem.fifo0)) {
		softio_delay_flush_try(write_fifo, sio, mem.fifo0);  // fullfill the remote fifo
	}
	mem.gpio_count = samples.size();
	softio_blocking(write, sio, mem.gpio_count);  // write count variable to start transmitting
	while (written_cnt < samples.size()) {
		// printf("mem.fifo0.length: %d, samples.size(): %d, mem.gpio_count: %d, written_cnt: %d\n", __FIFO_GET_LENGTH(&mem.fifo0), samples.size(), mem.gpio_count, written_cnt);
		uint32_t write_len = __FIFO_GET_LENGTH(&mem.fifo0) - 1 + samples.size() - mem.gpio_count - written_cnt;  // maximum write without overflow
		write_len = min(write_len, (uint32_t)(samples.size() - written_cnt));  // cannot exceed remained samples
		if (verbose) printf("[%d/%d] stream %d samples\n", (int)(samples.size() - mem.gpio_count), (int)samples.size(), (int)write_len);
		for (uint32_t i=0; i<write_len; ++i) {
			fifo_enque(&mem.fifo0, samples[written_cnt]);
			++written_cnt;\
		}
		while (!fifo_empty(&mem.fifo0)) {
			softio_delay(write_fifo, sio, mem.fifo0);  // fullfill the remote fifo
		}
		softio_delay_flush_try(read_between, sio, mem.gpio_count, mem.gpio_underflow);
		this_thread::sleep_for(chrono::milliseconds(1));
		assert(mem.gpio_underflow == 0 && "tx overflow occurs, may be system overloaded or frequency too high");
	}
	// waiting for stop
	while (1) {
		softio_blocking(read, sio, mem.gpio_count);
		if (mem.gpio_count == 0) break;
		if (verbose) printf("[%d/%d] waiting %d samples\n", (int)(samples.size() - mem.gpio_count), (int)samples.size(), mem.gpio_count);
		this_thread::sleep_for(chrono::milliseconds(100));
	}
}

#endif
