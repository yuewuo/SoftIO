/*
 * 通过自动调整数字电位器获取最大功率的位置，并记录所有数据
 * 
 * 白天测一组走廊，晚上关灯测一组走廊
 * 第一个参数是设备端口号，第二个参数是文件名（输出文件为<文件名>.<ss>）
 * 
 */

#define X9C_CS_102 (1<<7)  // PB7，1KΩ电位器CS
#define X9C_CS_104 (1<<4)  // PB4，100KΩ电位器CS
#define X9C_U_D (1<<6)  // U/D
#define X9C_INC (1<<5)  // INC
#define SPI_SAMPLE_RATE 1e6
#define ZERO_RESISTANCE_OFFSET 67  // set resistance to 0 and set this to actual resistance

#include "stdio.h"
#define SOFTF103HOST_IMPLEMENTATION
#include "softf103-ex.h"
#include <ctime>
#include <string>
#include <signal.h>
#include <atomic>
#include <fstream>
#include <math.h>

SoftF103Host_t f103;

using namespace std;

atomic<bool> running(true);

void sigint_handler(int) {
    running = false;
}

/// index \in [0,99]
float compute_resistance(float maxval, int index) {
    return ZERO_RESISTANCE_OFFSET / 2.0 + maxval * index / 99;
}

/// return the index and the actual value of resistance
pair<pair<int, int>, float> find_resistance(int R) {
    assert(R >= 0 && R <= 100e3 && "value invalid");
    // 找到一个最接近的100k阻值
    float min_diff = 100e3;
    int min_index = 0;
    for (int i=0; i<100; ++i) {
        float val = fabs(compute_resistance(100e3, i) + compute_resistance(1e3, 0) - R);
        if (val < min_diff) {
            min_diff = val;
            min_index = i;
        }
    }
    // 在 [min_index-1, min_index+1] 内精细找一个最接近的
    min_diff = 100e3;
    int min_104 = 0;
    int min_102 = 0;
    if (min_index == 0) min_index = 1;
    if (min_index == 99) min_index = 98;
    for (int i=min_index-1; i<=min_index+1; ++i) {
        for (int j=0; j<100; ++j) {
            float val = fabs(compute_resistance(100e3, i) + compute_resistance(1e3, j) - R);
            if (val < min_diff) {
                min_diff = val;
                min_104 = i;
                min_102 = j;
            }
        }
    }
    return make_pair(make_pair(min_104, min_102), compute_resistance(100e3, min_104) + compute_resistance(1e3, min_102));
}

int last_104 = -1;
int last_102 = -1;
void update_resistance(int idx_104, int idx_102) {
    vector<uint8_t> samples;
    // first deselect 104 and 102 safely
    softio_blocking(read, f103.sio, f103.mem.gpio_out);
    samples.push_back(f103.mem.gpio_out);
    samples.push_back(samples[0] | X9C_INC);  // later will store wiper position
    samples.push_back(samples[1] | X9C_CS_102 | X9C_CS_104);  // deselect all
    if (last_104 < 0) {  // first move to highest value
        uint8_t base = X9C_CS_102;  // select 104
        samples.push_back(X9C_CS_102 | X9C_CS_104 | X9C_INC);
        for (int i=0; i<100; ++i) {
            samples.push_back(base | X9C_INC | X9C_U_D);
            samples.push_back(base | 0 | X9C_U_D);  // falling edge to wiper up
        }
        samples.push_back(X9C_U_D | X9C_CS_102 | X9C_CS_104);  // deselect all without store wiper position
        last_104 = 99;
    }
    if (last_102 < 0) {  // first move to highest value
        uint8_t base = X9C_CS_104;  // select 102
        samples.push_back(X9C_CS_102 | X9C_CS_104 | X9C_INC);
        for (int i=0; i<100; ++i) {
            samples.push_back(base | X9C_INC | X9C_U_D);
            samples.push_back(base | 0 | X9C_U_D);  // falling edge to wiper up
        }
        samples.push_back(X9C_U_D | X9C_CS_102 | X9C_CS_104);  // deselect all without store wiper position
        last_102 = 99;
    }
    if (last_104 != idx_104) {
        uint8_t base = X9C_CS_102;  // select 104
        samples.push_back(base | X9C_INC);
        bool is_up = idx_104 > last_104;
        while (last_104 != idx_104) {
            samples.push_back(base | X9C_INC | (is_up?X9C_U_D:0));
            samples.push_back(base | 0 | (is_up?X9C_U_D:0));  // falling edge to wiper up / down
            last_104 += is_up ? 1 : -1;
        }
        samples.push_back(X9C_INC | X9C_CS_102 | X9C_CS_104);  // deselect all
    }
    if (last_102 != idx_102) {
        uint8_t base = X9C_CS_104;  // select 102
        samples.push_back(base | X9C_INC);
        bool is_up = idx_102 > last_102;
        while (last_102 != idx_102) {
            samples.push_back(base | 0 | (is_up?X9C_U_D:0));  // falling edge to wiper up / down
            samples.push_back(base | X9C_INC | (is_up?X9C_U_D:0));
            last_102 += is_up ? 1 : -1;
        }
        samples.push_back(X9C_INC | X9C_CS_102 | X9C_CS_104);  // deselect all
    }
    samples.push_back(X9C_INC | X9C_CS_102 | X9C_CS_104);  // deselect all
    // for (size_t i=0; i<samples.size(); ++i) {
    //     auto x = samples[i];
    //     printf("%d %d %d %d\n", !!(x&X9C_INC), !!(x&X9C_U_D), !!(x&X9C_CS_102), !!(x&X9C_CS_104));
    // }
    f103.GPIO_streaming(SPI_SAMPLE_RATE, samples);
}

int main(int argc, char** argv) {
	if (argc != 7 && !(argc == 3 && argv[2][0] == '=')) {
		printf("usage: <portname> <R_min> <R_max> <point_cnt> <sample_cnt> <filename_prefix>\n");
		printf("   or: <portname> =<R>\n\n");
        printf("point_cnt: will search from R_min to R_max, given point_cnt internal points, exponential increasing\n");
        printf("sample_cnt: for each resistance, sample for sample_cnt times\n");
		return -1;
	}

    if (argc == 3) {  // only set resistance value for debugging
        f103.verbose = true;
        f103.open(argv[1]);
        f103.verbose = false;
        float R = atof(argv[2]+1);
        auto ret = find_resistance(R);
        int idx_104 = ret.first.first;
        int idx_102 = ret.first.second;
        float R_real = ret.second;
        update_resistance(idx_104, idx_102);
        printf("idx_104 = %d, idx_102 = %d\n", idx_104, idx_102);
        printf("set resistance to %f Ohm\n", R_real);
	    f103.close();
        return 0;
    }

    signal(SIGINT, sigint_handler);

    float R_min = atof(argv[2]);
    float R_max = atof(argv[3]);
    int point_cnt = atoi(argv[4]);
    int sample_cnt = atoi(argv[5]);
    const char* filename_prefix = argv[6];
    assert(strlen(filename_prefix) < 100);

    time_t currenttime = time(0);
    char strtime[64];
    char filename[256];
    strftime(strtime, sizeof(strtime), "%Y%m%d.%H%M%S", localtime(&currenttime));
    sprintf(filename, "%s.%s.dat", filename_prefix, strtime);
    printf("save to file: %s\n", filename);
    ofstream file;
    file.open(filename);
    if(!file.is_open()){
        printf("cannot open file %s to write\n", filename);
        return -2;
    }

	f103.verbose = true;
	f103.open(argv[1]);
    f103.verbose = false;


    for (int i=0; i<=point_cnt && running; ++i) {
        float R = R_max * pow(R_min / R_max, i / (float)point_cnt);
        // printf("R = %f\n", R);
        auto ret = find_resistance(R);
        int idx_104 = ret.first.first;
        int idx_102 = ret.first.second;
        float R_real = ret.second;
        // printf("R_real = %f\n", R_real);
        update_resistance(idx_104, idx_102);
        this_thread::sleep_for(chrono::milliseconds(10));  // waiting for RC to go steady
        printf("R_real = %f ... ", R_real);
        // system("pause");
        float sum_power = 0;
        float sum_U = 0;
        for (int j=0; j<sample_cnt; ++j) {
            softio_blocking(read, f103.sio, f103.mem.adc2);
            // printf("adc2: %d\n", f103.mem.adc2);
            float U = 3.3 * f103.mem.adc2 / 4096;
            sum_U += U;
            float P = U * U / R_real;
            sum_power += P;
            file << R_real << ' ' << P << endl;
        }
        printf("average_power = %f uW, U = %f V\n", sum_power / sample_cnt * 1e6, sum_U / sample_cnt);
    }

    file.close();
	f103.close();

    printf("exit gracefully\n");

	return 0;
}
