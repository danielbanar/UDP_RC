#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>

#define PORT 2223
#define BUFFER_SIZE 1024

void receiveMessages(int sockfd, struct sockaddr_in &serverAddr) {
    char buffer[BUFFER_SIZE];
    socklen_t addrLen = sizeof(serverAddr);

    while (true) {
        int len = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &serverAddr, &addrLen);
        buffer[len] = '\0';
        std::cout << "Server: " << buffer;
    }
}

int main() {
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in serverAddr;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));

    // Filling server information
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("192.168.1.88");

    // Start a thread to receive messages from the server
    std::thread receiveThread(receiveMessages, sockfd, std::ref(serverAddr));

    while (true) {
        const char *message = "Hello from client";
        sendto(sockfd, message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &serverAddr, sizeof(serverAddr));
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    receiveThread.join();
    close(sockfd);
    return 0;
}
