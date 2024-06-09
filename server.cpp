#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include "controller.h"
#include "overlay.h"
#pragma comment(lib, "Ws2_32.lib")
int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return EXIT_FAILURE;
    }

    int serverSocket;
    struct sockaddr_in serverAddr;
    int serverAddrLen = sizeof(serverAddr);
    const int serverPort = 2223;

    if ((serverSocket = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
    {
        std::cerr << "Socket creation error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return EXIT_FAILURE;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(serverPort);

    const int enable = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(enable)) < 0)
    {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Binding failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    std::cout << "Server listening on port " << serverPort << std::endl;

    struct sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    u_long mode = 1;
    if (ioctlsocket(serverSocket, FIONBIO, &mode) != NO_ERROR)
    {
        std::cerr << "ioctlsocket failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    uint8_t rxbuffer[128] = {0};
    Controller controller(0);
    Overlay* overlay = Overlay::getInstance();
    overlay->Init("Direct3D11 renderer");
    while (true)
    {
        controller.Poll();
        controller.Deadzone();
        std::string messageToSend = controller.CreatePayload();
        int bytesRead = recvfrom(serverSocket, (char*)rxbuffer, sizeof(rxbuffer), 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (bytesRead == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK)
            {
                LPSTR errorMessage = nullptr;
                FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorMessage, 0, NULL);
                std::cerr << "Error receiving data from UDP socket: " << errorMessage;
            }
        }
        else
        {
            std::cout << "Received message from client: " << rxbuffer;
        }
        if (sendto(serverSocket, messageToSend.c_str(), messageToSend.length(), 0, (struct sockaddr*)&clientAddr, clientAddrLen) == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            LPSTR errorMessage = nullptr;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorMessage, 0, NULL);
            std::cerr << "Error sending data to client: " << errorMessage;
        }
        overlay->Update();
        overlay->telemetry=std::string((char*)rxbuffer);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
