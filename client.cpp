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
#define THROTTLE_PIN 14
#define SERVO_PIN 15
#define PORT 2223
#define BUFFER_SIZE 1024
// Macro to map a value from one range to another
#define MAP_RANGE(value, fromLow, fromHigh, toLow, toHigh) ((value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow)

void receiveMessages(int sockfd, struct sockaddr_in& serverAddr) {
    char buffer[BUFFER_SIZE];
    socklen_t addrLen = sizeof(serverAddr);
    std::regex regexPattern("N(-?\\d+)LX(-?\\d+)LT(-?\\d+)RT(-?\\d+)\\n");

    while (true) {
        int len = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr*)&serverAddr, &addrLen);
        buffer[len] = '\0';

        std::cmatch matches;
        if (std::regex_search(buffer, matches, regexPattern)) 
        {
            int N = std::stoi(matches[1]);
            int LX = std::stoi(matches[2]);
            int LT = std::stoi(matches[3]);
            int RT = std::stoi(matches[4]);
            gpioServo(SERVO_PIN, MAP_RANGE(LX,-32768, 32767, 1000, 2000));
            gpioServo(SERVO_PIN, MAP_RANGE(RT-LT,-255, 255, 1000, 2000));
        }
        else 
        {
            std::cout << "Received: " << buffer;
        }
    }
}

int main() {
    int sockfd;
    char buffer[BUFFER_SIZE];
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

    while (true) {
        const char* message = "Hello from client";
        sendto(sockfd, message, strlen(message), MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    receiveThread.join();
    close(sockfd);
    // Stop servo pulses
    gpioServo(SERVO_PIN, 0);

    // Terminate pigpio library
    gpioTerminate();
    return 0;
}
