/*
 * 通过调整电位器获取最大功率的位置，认为是当前位置的最大功率
 * 
 * 白天测一组走廊，晚上关灯测一组走廊
 * 第一个参数是设备端口号，第二个参数是文件名（输出文件为<文件名>.<ss>）
 */

#include "stdio.h"
#define SOFTF103HOST_IMPLEMENTATION
#include "softf103-ex.h"
#include <ctime>
#include <string>
#include <signal.h>
#include <atomic>
#include <fstream>

SoftF103Host_t f103;

using namespace std;

atomic<bool> running(true);

void sigint_handler(int) {
    running = false;
}

int main(int argc, char** argv) {
	if (argc != 3) {
		printf("usage: <portname> <filename_prefix>\n");
		return -1;
	}
    
    signal(SIGINT, sigint_handler);

    const char* filename_prefix = argv[2];
    assert(strlen(filename_prefix) < 100);

    time_t currenttime = time(0);
    char strtime[64];
    char filename[256];
    strftime(strtime, sizeof(strtime), "%Y%m%d.%H%M%S", localtime(&currenttime));
    sprintf(filename, "%s.%s.txt", filename_prefix, strtime);
    printf("save to file: %s\n", filename);
    ofstream file;
    file.open(filename);
    if(!file.is_open()){
        printf("cannot open file %s to write\n", filename);
        return -2;
    }

	f103.verbose = true;
	f103.open(argv[1]);

    double max_power = 0;
    
    while (running) {
        softio_blocking(read_between, f103.sio, f103.mem.adc1, f103.mem.adc2);
        // printf("adc1: %d\n", f103.mem.adc1);
        // printf("adc2: %d\n", f103.mem.adc2);

        /// 3V3 -- 33kΩ --ADC1-- R2 -- GND, 于是ADC1可以反推R2
        double R2 = 3.3e4 * f103.mem.adc1 / (4096 - f103.mem.adc1);
        // printf("R2 = %f Ohm\n", R2);
        // 虽然滑动变阻器标称100kΩ，但实际R2最大值大约91.xxKΩ，于是计算R1=92k-R2
        double R1 = 92e3 - R2;
        if (R1 < 1000) { printf("R1 is not accurate enough, continue\n"); continue; }
        // 当前太阳能板电压
        double U = f103.mem.adc2 * 3.3 / 4096;
        // 太阳能板输出功率
        double P = U * U / R1;
        if (P > max_power) max_power = P;
        printf("R1/(R1+R2) = %03d/100, P = %f uW, MaxPower = %f uW\n", (int)((R1 / (R1 + R2)) * 100), P * 1e6, max_power * 1e6);
        // printf("R1 = %f Ohm\n", R1);

        file << f103.mem.adc1 << ' ' << f103.mem.adc2 << ' ' << R2 << ' ' << R1 << ' ' << P << endl;

        this_thread::sleep_for(chrono::milliseconds(100));
    }

    file.close();
	f103.close();

    printf("exit gracefully\n");

	return 0;
}
