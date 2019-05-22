#include "fifo.h"
#include "softio.h"

/*
 * This header library provides basic functions for MCU operation
 * take whatever you like and ignore others, by pre-define the function you need
 */

// MCU_VERSION: uint32_t number, like 0x19052200, be sure to update this number when memory is different from before
#define MCU_VERSION 0x19052200
// MCU_PID: uint16_t number, the pid to distinguish different devices, you should modify it, for example:
#define MCU_PID 0x1234

// usage: you should instantiate 'mem' object in MCU using "SoftF103_Mem_t mem;"
// also instantiate 'sio' object using "SoftIO_t sio;"

/*
 * Shared Memory Structure
 */

typedef struct {

#define STATUS_INIT 0x00
#define STATUS_IDLE 0x01
#define STATUS_TEST 0xFF
#define STATUS_RUNNING 0x66
#define STATUS_STR(status) ((status)==STATUS_INIT?"init":\
							((status)==STATUS_IDLE?"idle":\
							((status)==STATUS_TEST?"test":\
							((status)==STATUS_RUNNING?"running":"ERROR"))))
    uint8_t status;

#define VERBOSE_NONE 0x00
#define VERBOSE_ERROR 0x20
#define VERBOSE_WARN 0x40
#define VERBOSE_INFO 0x60
#define VERBOSE_DEBUG 0x80
#define VERBOSE_REACH_LEVEL(level, threshold) ((level) >= (threshold))  // 使用方法：如果想要debug才打印的信息，设为 VERBOSE_DEBUG 即可
	uint8_t verbose_level;

    uint16_t pid;  // equals to MCU_PID when initialized

    uint32_t version;  // 系统版本，见compile_conf.h

	uint32_t mem_size;  // sizeof(ReaderH7_Mem_t)
	
	uint16_t siorx_overflow;

// GPIO functions
	uint8_t gpio_out;  // write to this variable will immediately update GPIO value of PB0 ~ PB7
	uint8_t gpio_in;  // read PB8 ~ PB15
	uint32_t gpio_count_add;  // write to atomically add to gpio_count
	uint32_t gpio_count;  // for data streaming, provide the count of samples. only timer 1 interrupt is valid, and using fifo0
	uint32_t gpio_underflow;  // record underflow count for sanity check

// read adc value immediately
	uint16_t adc1;
	uint16_t adc2;

// LED functions
	uint8_t led;  // write 1 to open the LED and write 0 to close. only the LSB is used
	uint8_t recv_led_1;
	uint16_t recv_led_2;

// Timers
	uint8_t tim1_PWM;  // write 1 to enable, 0 to disable
	uint8_t tim1_IT;  // write 1 to enable, 0 to disable
	uint16_t tim1_prescaler;
	uint16_t tim1_period;
	uint16_t tim1_pulse;

	uint8_t tim2_PWM;  // write 1 to enable, 0 to disable
	uint8_t tim2_IT;  // write 1 to enable, 0 to disable
	uint16_t tim2_prescaler;
	uint16_t tim2_period;
	uint16_t tim2_pulse;

	char siorx_buf[1024];
	char siotx_buf[1024];
	char logging_buf[512];  // debug informations here
	char fifo0_buf[1024];
	char fifo1_buf[1024];
#define Mem_FifoInit(mem) do {\
	FIFO_STD_INIT(mem, siorx);\
	FIFO_STD_INIT(mem, siotx);\
	FIFO_STD_INIT(mem, logging);\
	FIFO_STD_INIT(mem, fifo0);\
	FIFO_STD_INIT(mem, fifo1);\
} while(0)
	Fifo_t siorx;  // must be the first 
	Fifo_t siotx;
	Fifo_t logging;
	Fifo_t fifo0;
	Fifo_t fifo1;

} SoftF103_Mem_t;

#define print_debug(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_DEBUG))    printf("D: " format "\r\n",##__VA_ARGS__); } while(0)
#define print_info(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_INFO))      printf("I: " format "\r\n",##__VA_ARGS__); } while(0)
#define print_warn(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_WARN))      printf("W: " format "\r\n",##__VA_ARGS__); } while(0)
#define print_error(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_ERROR))    printf("E: " format "\r\n",##__VA_ARGS__); } while(0)

#ifdef EXTERN_MEM
extern SoftF103_Mem_t mem;
extern SoftIO_t sio;
#endif

/*
 * Memory Structure Initiator
 */
#ifdef MEM_INITIATOR
#undef MEM_INITIATOR
void memory_init_user_code_begin_sys_init(void) {
	mem.status = STATUS_INIT;  // host could read this status word, change to STATUS_IDLE when init successs
    mem.version = MCU_VERSION;
    mem.pid = MCU_PID;
	mem.verbose_level = VERBOSE_DEBUG;  // set verbose level
	mem.mem_size = sizeof(SoftF103_Mem_t);
	SOFTIO_QUICK_INIT(sio, mem, Mem_FifoInit);
}
#endif
