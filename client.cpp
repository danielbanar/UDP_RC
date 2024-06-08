#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <netdb.h>
#include <regex>
#include <pigpio.h>
#include <fstream>
#define THROTTLE_PIN 14
#define SERVO_PIN 15
#define LED_PIN 18
#define PORT 2223
#define BUFFER_SIZE 1024
// Macro to map a value from one range to another
#define MAP_RANGE(value, fromLow, fromHigh, toLow, toHigh) ((value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow)

int get_cpu_temperature();
struct NetworkUsage {
	unsigned long rx_bytes;
	unsigned long tx_bytes;
};
NetworkUsage getNetworkUsage();
void receiveMessages(int sockfd, struct sockaddr_in& serverAddr) {
	char buffer[BUFFER_SIZE];
	socklen_t addrLen = sizeof(serverAddr);
	std::regex regexPattern("N(-?\\d+)LX(-?\\d+)LT(-?\\d+)RT(-?\\d+)B(-?\\d+)\\n");

	while (true)
	{
		int len = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr*)&serverAddr, &addrLen);
		buffer[len] = '\0';

		std::cmatch matches;
		if (std::regex_search(buffer, matches, regexPattern))
		{
			static auto lastValidPayload = std::chrono::high_resolution_clock::now();
			static int lastN = 0;
			int N = std::stoi(matches[1]);
			if (lastN < N)
			{
				int LX = std::stoi(matches[2]);
				int LT = std::stoi(matches[3]);
				int RT = std::stoi(matches[4]);
				int Buttons = std::stoi(matches[5]);
				gpioServo(SERVO_PIN, MAP_RANGE(LX, -32768, 32767, 1000, 2000));
				gpioServo(THROTTLE_PIN, MAP_RANGE(RT - LT, -255, 255, 1000, 2000));
				gpioWrite(LED_PIN, Buttons & 0x100?1:0);
				//std::cout << "T" << MAP_RANGE(LX, -32768, 32767, 1000, 2000) << std::endl;
				//std::cout << "S" << MAP_RANGE(RT - LT, -255, 255, 1000, 2000) << std::endl;
				//std::cout << "L" << (Buttons & 0x100?1:0) << std::endl;
				lastN = N;
				lastValidPayload = std::chrono::high_resolution_clock::now();
			}
			else
			{
				auto currentTime = std::chrono::high_resolution_clock::now();
				auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastValidPayload).count();
				if (elapsedTime >= 2)
					lastN = 0;
			}
		}
		else
		{
			std::cout << "Received: " << buffer;
		}
	}
}

int main() {
	int sockfd;
	struct sockaddr_in serverAddr;
	struct addrinfo hints, * res;

	// Creating socket file descriptor
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&serverAddr, 0, sizeof(serverAddr));
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_DGRAM; // UDP

	// Resolve the hostname to an IP address
	const char* hostname = "tutos.ddns.net";
	if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
		perror("getaddrinfo failed");
		exit(EXIT_FAILURE);
	}

	// Copy the resolved address to serverAddr
	serverAddr = *(struct sockaddr_in*)(res->ai_addr);
	serverAddr.sin_port = htons(PORT);

	freeaddrinfo(res);

	if (gpioInitialise() < 0) {
		std::cerr << "Failed to initialize pigpio" << std::endl;
		return 1;
	}

	// Start a thread to receive messages from the server
	std::thread receiveThread(receiveMessages, sockfd, std::ref(serverAddr));

	while (true)
	{
		const double interval = 0.5; // interval in seconds
		const double bytes_to_kb = 1024.0;
		NetworkUsage usage1 = getNetworkUsage();
		std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval * 1000)));
		NetworkUsage usage2 = getNetworkUsage();
		unsigned long rx_diff = usage2.rx_bytes - usage1.rx_bytes;
		unsigned long tx_diff = usage2.tx_bytes - usage1.tx_bytes;
		unsigned long rx_kbps = static_cast<unsigned long>(rx_diff / bytes_to_kb / interval);
		unsigned long tx_kbps = static_cast<unsigned long>(tx_diff / bytes_to_kb / interval);

		std::string temp_str = "Temp " + std::to_string(get_cpu_temperature()) + "C, R: " + std::to_string(rx_kbps) + " KB/s, T: " + std::to_string(tx_kbps) + " KB/s\n";

		sendto(sockfd, temp_str.c_str(), temp_str.length(), MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));
	}

	receiveThread.join();
	close(sockfd);
	// Stop servo pulses
	gpioServo(SERVO_PIN, 0);

	// Terminate pigpio library
	gpioTerminate();
	return 0;
}
