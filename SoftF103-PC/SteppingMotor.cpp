#include "stdio.h"
#define SOFTF103HOST_IMPLEMENTATION
#include "softf103-ex.h"

SoftF103Host_t f103;

// using stepping motor with driver, 3 line: ENA(enable), PUL(pulse), DIR(direction)

#define MOTOR_MAXCOUNT 10000

#define MOTOR_ENA (1<<3) // PB3
#define MOTOR_PUL (1<<4) // PB4
#define MOTOR_DIR (1<<5) // PB5

int main(int argc, char** argv) {
	if (argc != 4) {
		printf("usage: <portname> <frequency> <pulse count>\n");
		return -1;
	}

	float frequency;
	int pulse_count;
	sscanf(argv[2], "%f", &frequency);
	sscanf(argv[3], "%d", &pulse_count);
	assert(frequency >= 0.1 && "invalid frequency");
	assert(pulse_count >= -MOTOR_MAXCOUNT && pulse_count <= MOTOR_MAXCOUNT && pulse_count != 0 && "invalid pulse count");
	printf("target frequency: %f kHz\n", frequency/1e3);

	f103.verbose = true;
	f103.open(argv[1]);
	
	vector<uint8_t> samples;
	int pulse_count_abs = pulse_count > 0 ? pulse_count : -pulse_count;
	for (int i=0; i<pulse_count_abs; ++i) {
		uint8_t tmp = pulse_count > 0 ? MOTOR_DIR : 0;
		samples.push_back(tmp | MOTOR_PUL);
		samples.push_back(tmp);  // a pulse
	}
	f103.GPIO_streaming(frequency, samples);

	f103.close();

	return 0;
}
