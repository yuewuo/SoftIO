#include "unistd.h"
#include "assert.h"
#define MQTT_IMPLEMENTATION
#include "mqtt.h"
#include <chrono>
using namespace std;
using namespace std::chrono_literals;
#include "stdio.h"
#include "limits.h"
#include <mutex>
#include <vector>
#define SOFTF103HOST_IMPLEMENTATION
#include "softf103-ex.h"

#define VERSION_STR "SteppingMotor Host compiled at " __TIME__ ", " __DATE__ 
const char* MQTT_HOST = "localhost";
int MQTT_PORT = 1883;
const char* ServerID = "default";
MQTT hostmqtt;
atomic<bool> running;
void hostOnMessage(MQTT& mqtt, const struct mosquitto_message *message);

const char* device_port = NULL;
SoftF103Host_t f103;
// using stepping motor with driver, 3 line: ENA(enable), PUL(pulse), DIR(direction)
#define MOTOR_ENA (1<<3) // PB3
#define MOTOR_PUL (1<<4) // PB4
#define MOTOR_DIR (1<<5) // PB5
// 160 pulse / mm
#define MAX_FREQUENCY 16e3  // 8kHz -> 50mm/s
#define MIN_FREQUENCY 160  // 80Hz -> 0.5mm/s

int motor_min = INT32_MIN;
int motor_max = INT32_MAX;
int motor_now = 0;
mutex motor_lock;

void report_min_now_max() {
	char buf[128];
	sprintf(buf, "%d %d %d", motor_min, motor_now, motor_max);
	hostmqtt.publish((string("steppingmotor/") + ServerID + "/now_min_max").c_str(), buf);
}

void motor_go(float frequency, int count) {
	if (frequency < MIN_FREQUENCY || frequency > MAX_FREQUENCY) return;
	motor_lock.lock();
	float actual = f103.Timer_Start_IT(1, frequency);
	if (motor_now + count > motor_max) count = (motor_max - motor_now);
	if (motor_now + count < motor_min) count = (motor_min - motor_now);
	printf("go %d with frequency: %f kHz\n", count, actual/1e3);
	if (count) {  // really go
		vector<uint8_t> samples;
		int count_abs = count > 0 ? count : -count;
		uint8_t tmp = count > 0 ? MOTOR_DIR : 0;
		for (int i=0; i<count_abs; ++i) {
			samples.push_back(tmp | MOTOR_PUL);
			samples.push_back(tmp);  // a pulse
		}
		f103.GPIO_streaming(frequency, samples);
	}
	motor_now += count;
	report_min_now_max();
	motor_lock.unlock();
}

int main(int argc, char** argv) {
	MQTT::LibInit();
	char c;
	const char* optformat = "H:P:C:p:";
	while ((c = getopt(argc, argv, optformat)) != EOF) {
		switch(c) {
			case 'H': MQTT_HOST = optarg; break;
			case 'P': MQTT_PORT = atoi(optarg); break;
			case 'C': ServerID = optarg; break;
			case 'p': device_port = optarg; break;
			default:
				printf("usage: %s\n", optformat);
				printf("  H: MQTT_HOST\n");
				printf("  P: MQTT_PORT\n");
				printf("  C: MQTT_CLIENTID\n");
				printf("  p: device port name\n");
				return -1;
		}
	}
	assert(device_port && "device port required");
	printf("SteppingMotor Host information:\n");
	printf("  ServerID: %s\n", ServerID);
	printf("  MQTT_HOST: %s\n", MQTT_HOST);
	printf("  MQTT_PORT: %d\n", MQTT_PORT);
	printf("  device port name: %s\n", device_port);
	printf("\n");

	hostmqtt.onLog = [&](MQTT& mqtt, int level, const char *str) {
		(void)mqtt; (void)level; (void)str;
		// printf("LOG: %s\n", str);  // log sometimes too much
	};
	hostmqtt.onConnect = [&](MQTT& mqtt, int result) {
		printf("hostmqtt connected: %d\n", result);
		mqtt.subscribe("steppingmotor/query");  // query retrohosts
		mqtt.subscribe((string("steppingmotor/") + ServerID + "/#").c_str());
		mqtt.subscribe((string("steppingmotor/") + ServerID + "/shutdown").c_str());
	};
	hostmqtt.onMessage = hostOnMessage;
	hostmqtt.start(ServerID, MQTT_HOST, MQTT_PORT);

	f103.verbose = true;
	f103.open(device_port);
	f103.verbose = false;

	running.store(true);
	while (running) {
		this_thread::sleep_for(100ms);
	}

	hostmqtt.stop();
	printf("stopped gracefully");
	MQTT::LibDeinit();
}

void hostOnMessage(MQTT& mqtt, const struct mosquitto_message *message) {
	string topic = message->topic;
	int length = message->payloadlen;
	vector<char> payload;
	payload.reserve(length+1);
	for (int i=0; i<length; ++i) {
		payload[i] = ((char*)message->payload)[i];
	} payload[length] = '\0';  // add \0 at the end
	char* str = payload.data();
	printf("topic: %s\n", topic.c_str());
	string prefix = string("steppingmotor/") + ServerID + "/";
	if (topic == "steppingmotor/query") {
		mqtt.publish((string("steppingmotor/") + ServerID + "/reply").c_str(), VERSION_STR);
		report_min_now_max();
	} else if (topic.compare(0, prefix.length(), prefix) == 0) {
		string subtopic = topic.substr(prefix.length());
		if (subtopic == "shutdown") {
			running.store(false);
		} else if (subtopic == "move") {
			float frequency;
			int count;
			sscanf(str, "%f %d", &frequency, &count);
			printf("frequency: %f, count: %d\n", frequency, count);
			motor_go(frequency, count);
		} else if (subtopic == "set_now") {
			motor_now = atoi(str);
			report_min_now_max();
		} else if (subtopic == "set_min") {
			motor_min = atoi(str);
			report_min_now_max();
		} else if (subtopic == "set_max") {
			motor_max = atoi(str);
			report_min_now_max();
		}
	}
}
