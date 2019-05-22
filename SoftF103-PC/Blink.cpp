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

	for (int i=0; i<10; ++i) {
		this_thread::sleep_for(chrono::milliseconds(100));
		f103.LED_set(0);
		this_thread::sleep_for(chrono::milliseconds(100));
		f103.LED_set(1);
	}

	f103.close();

	return 0;
}
