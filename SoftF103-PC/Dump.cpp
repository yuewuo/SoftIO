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
	f103.dump(~0);
	f103.close();

	return 0;
}
