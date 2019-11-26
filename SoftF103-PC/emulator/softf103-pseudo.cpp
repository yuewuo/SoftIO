#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <mutex>
#include <termios.h>
using namespace std;

#define SOFTF103HOST_IMPLEMENTATION
#define SOFTIO_USE_FUNCTION
#define EXTERN_MEM
#define MEM_INITIATOR
#include "softf103.h"

static int fdm;
static int connect_cnt = 0;
static const char *pts_name = "UNKNOWN";
SoftF103_Mem_t mem;
SoftIO_t sio;
static mutex memmtx;

int ptym_open() {
    int fdm = posix_openpt(O_RDWR);
    if (fdm < 0) {
        perror("cannot open pseudo terminal"); exit(-1);
    }
    if (grantpt(fdm) < 0) {
        perror("grant access to slave failed"); exit(-2);
    }
    if (unlockpt(fdm) < 0) {
        perror("clear slave's lock flag failed"); exit(-3);
    }
    if ((pts_name = ptsname(fdm)) == NULL) {
        perror("get slave's name failed"); exit(-4);
    }
    // printf("pseudo device opened at: %s\n", pts_name);
    return fdm;
}

int refresh_screen() {
    lock_guard<std::mutex> lck(memmtx);  // always lock when handling memory

    erase();
    box(stdscr, ACS_VLINE, ACS_HLINE); /*draw a box*/
    mvprintw(0, 2, "  MCU Version 0x%08X ", MCU_VERSION);
    mvprintw(0, 30, "  pseudo device \"%s\" ", pts_name);
    mvprintw(0, 60, "  [%d] ", connect_cnt);
    int lineidx = 2;
    int X = 2;
    mvprintw(lineidx++, X, "status: 0x%02X [%s]", mem.status, STATUS_STR(mem.status));
    mvprintw(lineidx++, X, "verbose_level: 0x%02X", mem.verbose_level);
    mvprintw(lineidx++, X, "siorx_overflow: %d", mem.siorx_overflow);

    lineidx++;  // GPIO
#define S(i) ((mem.gpio_out>>i)&1?'H':'L')
    mvprintw(lineidx++, X, "gpio_out: 0x%02X [ %c %c %c %c %c %c %c %c ]", mem.gpio_out, S(7), S(6), S(5), S(4), S(3), S(2), S(1), S(0));
#undef S
#define S(i) ((mem.gpio_in>>i)&1?'H':'L')
    mvprintw(lineidx++, X, " gpio_in: 0x%02X [ %c %c %c %c %c %c %c %c ]", mem.gpio_in, S(7), S(6), S(5), S(4), S(3), S(2), S(1), S(0));
#undef S
    mvprintw(lineidx++, X, "gpio_count_add: %d, gpio_count: %d, gpio_underflow: %d", mem.gpio_count_add);

    lineidx++;  // ADC
    mvprintw(lineidx++, X, "adc1: %d [%f * Vref]", mem.adc1, mem.adc1 / 4096.);
    mvprintw(lineidx++, X, "adc2: %d [%f * Vref]", mem.adc2, mem.adc2 / 4096.);

    lineidx++;  // LED
    mvprintw(lineidx++, X, "led: (XX) [ %s ]", mem.led&1 ? "ON" : "OFF");
    attron(COLOR_PAIR(mem.led ? 3 : 2));
    mvprintw(lineidx-1, X+6, "  ");
    attroff(COLOR_PAIR(mem.led ? 3 : 2));

    const float clock = 72e6;

    lineidx++;  // Timer
    mvprintw(lineidx++, X, "Timer 1:");
    mvprintw(lineidx++, X, "PWM: ");
    attron(COLOR_PAIR(mem.tim1_PWM ? 4 : 5)); printw("%s", mem.tim1_PWM?"Enabled":"Disabled"); attroff(COLOR_PAIR(mem.tim1_PWM ? 4 : 5));
    printw(", PWM: ");
    attron(COLOR_PAIR(mem.tim1_IT ? 4 : 5)); printw("%s", mem.tim1_IT?"Enabled":"Disabled"); attroff(COLOR_PAIR(mem.tim1_PWM ? 4 : 5));
    mvprintw(lineidx++, X, "prescaler: %d, period: %d, pulse: %d",  mem.tim1_prescaler, mem.tim1_period, mem.tim1_pulse);
    mvprintw(lineidx++, X, "( frequency: %f Hz, duty: %f %% )"
        , clock / (mem.tim1_prescaler + 1.f) / (mem.tim1_period + 1.f), (mem.tim1_pulse + 1.f) / (mem.tim1_period + 1.f) * 100);
    mvprintw(lineidx++, X, "Timer 2:");
    mvprintw(lineidx++, X, "PWM: ");
    attron(COLOR_PAIR(mem.tim2_PWM ? 4 : 5)); printw("%s", mem.tim2_PWM?"Enabled":"Disabled"); attroff(COLOR_PAIR(mem.tim2_PWM ? 4 : 5));
    printw(", PWM: ");
    attron(COLOR_PAIR(mem.tim2_IT ? 4 : 5)); printw("%s", mem.tim2_IT?"Enabled":"Disabled"); attroff(COLOR_PAIR(mem.tim2_PWM ? 4 : 5));
    mvprintw(lineidx++, X, "prescaler: %d, period: %d, pulse: %d",  mem.tim2_prescaler, mem.tim2_period, mem.tim2_pulse);
    mvprintw(lineidx++, X, "( frequency: %f Hz, duty: %f %% )"
        , clock / (mem.tim2_prescaler + 1.f) / (mem.tim2_period + 1.f), (mem.tim2_pulse + 1.f) / (mem.tim2_period + 1.f) * 100);

    refresh();
}

int main() {
    initscr();
    noecho();  // no echo display
    curs_set(FALSE);  // no curser display
    if (has_colors()) start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_BLACK, COLOR_BLACK);
    init_pair(3, COLOR_BLACK, COLOR_GREEN);
    init_pair(4, COLOR_GREEN, COLOR_WHITE);
    init_pair(5, COLOR_RED, COLOR_WHITE);
    bkgd(COLOR_PAIR(1));

    // Initialize device
    memory_init_user_code_begin_sys_init();
    sio.gets = [&](char *buffer, size_t size)->size_t {
		size_t s = read(fdm, (uint8_t*)buffer, size);
        tcdrain(fdm);
		return s;
	};
	sio.puts = [&](char *buffer, size_t size)->size_t {
		size_t s = write(fdm, (uint8_t*)buffer, size);
        tcdrain(fdm);
		return s;
	};
	sio.available = [&]()->size_t {
        int count = 0;
        assert(ioctl(fdm, TIOCINQ, &count) != -1);
        return count;
	};
	sio.callback = [&](void* softio, SoftIO_Head_t* head)->void {
		assert(softio);
		assert(head);
	};
    sio.before = [&](void* softio, SoftIO_Head_t* head)->void {

    };
    sio.after = [&](void* softio, SoftIO_Head_t* head)->void {

    };
    fdm = ptym_open();
    ++connect_cnt;
    refresh_screen();
    char buf[1024];
    while (1) {
        softio_flush_try_handle_all(sio);
        if (sio.write == sio.read) {  // do not refresh screen until all transaction is done
            refresh_screen();
            usleep (1000);  // sleep 1ms
        }
    }
    close(fdm);

    endwin();
    return 0;
}
