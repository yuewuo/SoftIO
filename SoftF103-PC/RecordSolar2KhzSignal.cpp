#include "stdio.h"
#define SOFTF103HOST_IMPLEMENTATION
#include "softf103-ex.h"
#include <ctime>
#include <string>
#include <signal.h>
#include <atomic>
#include <fstream>
#include <math.h>

#define FREQUENCY 8000

SoftF103Host_t f103;

int main(int argc, char** argv) {
	if (argc != 4) {
		printf("usage: <portname> <duration/s> <filename_prefix>\n");
		return -1;
	}

	f103.verbose = true;
	f103.open(argv[1]);
	f103.verbose = false;
    float duration = atof(argv[2]);
    int length = duration * FREQUENCY;
    const char* filename_prefix = argv[3];
    assert(strlen(filename_prefix) < 100);
    
    time_t currenttime = time(0);
    char strtime[64];
    char filename[256];
    strftime(strtime, sizeof(strtime), "%Y%m%d.%H%M%S", localtime(&currenttime));
    sprintf(filename, "%s.%s.bin", filename_prefix, strtime);
    printf("save to file: %s\n", filename);
    ofstream file;
    file.open(filename, ios::binary);
    if(!file.is_open()){
        printf("cannot open file %s to write\n", filename);
        return -2;
    }

    printf("start streaming...\n");
    vector<pair<float, float>> samples = f103.ADC_streaming(FREQUENCY, length);
    for (int i=0; i<(int)samples.size(); ++i) {
        file.write((char*)&samples[i].second, sizeof(float));
    }

    printf("done written to %s, size = %d bytes\n", filename, (int)(samples.size() * sizeof(float)));

	f103.close();

	return 0;
}
