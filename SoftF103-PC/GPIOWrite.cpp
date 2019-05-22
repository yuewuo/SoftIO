#include "stdio.h"
#define SOFTF103HOST_IMPLEMENTATION
#include "softf103-ex.h"

SoftF103Host_t f103;

int main(int argc, char** argv) {
	if (argc != 3) {
		printf("usage: <portname> <hex uint8_t>\n");
		return -1;
	}

	f103.verbose = true;
	f103.open(argv[1]);

	uint32_t hex = 0;
	sscanf(argv[2], "%X", &hex);
	uint8_t output = hex;
	f103.GPIO_write(output);
	printf("write 0x%02X to PB7-PB0\n", output);

	f103.close();

	return 0;
}
