#include "stdio.h"
#define SOFTF103HOST_IMPLEMENTATION
#include "softf103-ex.h"

SoftF103Host_t f103;

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: <portname>\n");
		return -1;
	}

	f103.verbose = true;
	f103.open(argv[1]);

	softio_blocking(read_between, f103.sio, f103.mem.adc1, f103.mem.adc2);
    printf("adc1: %d\n", f103.mem.adc1);
    printf("adc2: %d\n", f103.mem.adc2);

	f103.close();

	return 0;
}
