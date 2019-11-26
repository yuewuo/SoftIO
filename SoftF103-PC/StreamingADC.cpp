#include "stdio.h"
#define SOFTF103HOST_IMPLEMENTATION
#include "softf103-ex.h"

SoftF103Host_t f103;

int main(int argc, char** argv) {
	if (argc != 4) {
		printf("usage: <portname> <frequency> <length>\n");
		return -1;
	}

	f103.verbose = true;
	f103.open(argv[1]);
    float frequency = atof(argv[2]);
    int length = atoi(argv[3]);

    vector<pair<float, float>> samples = f103.ADC_streaming(frequency, length);
    for (int i=0; i<length && i<100; ++i) {
        printf("(%f, %f)\n", samples[i].first, samples[i].second);
    }
    if (length > 100) printf("... only 100 data printed");

	f103.close();

	return 0;
}
