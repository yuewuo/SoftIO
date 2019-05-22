#include "stdio.h"
#define SOFTF103HOST_IMPLEMENTATION
#include "softf103-ex.h"

SoftF103Host_t f103;

int main(int argc, char** argv) {
	if (argc != 5) {
		printf("usage: <portname> <timer=1/2> <frequency> <duty>\n");
		return -1;
	}

	int timer;
	float frequency, duty;
	sscanf(argv[2], "%d", &timer);
	sscanf(argv[3], "%f", &frequency);
	sscanf(argv[4], "%f", &duty);
	assert((timer == 1 || timer == 2) && "invalid timer");
	assert(frequency >= 0.1 && "invalid frequency");
	assert(duty > 0 && duty < 1 && "invalid duty");
	printf("target frequency: %f kHz, duty: %f %%\n", frequency/1e3, duty*100);

	f103.verbose = true;
	f103.open(argv[1]);

	pair<float, float> actual = f103.Timer_Start_PWM(timer, frequency, duty);
	printf("actual frequency: %f kHz, duty: %f %%\n", actual.first/1e3, actual.second*100);

	f103.close();

	return 0;
}
