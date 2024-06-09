#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <windows.h>
#include <gdiplus.h>
#include <ctime>
#include <locale>
#include <string>
#include <regex>
#include "controller.h"
#include "overlay.h"
#pragma comment (lib,"Gdiplus.lib")
#pragma comment(lib, "Ws2_32.lib")

using namespace Gdiplus;
#define CHAR_TO_WCHAR(str) \
    ([](const char* input) { \
        int length = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0); \
        wchar_t* output = new wchar_t[length]; \
        MultiByteToWideChar(CP_UTF8, 0, input, -1, output, length); \
        return output; \
    })(str)



const char* TARGET_WINDOW_NAME = "Direct3D11 renderer";
char rxbuffer[128] = { 0 };
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

    Controller controller(0);

    ULONG_PTR gdiplusToken;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    const char* CLASS_NAME = "OverlayWindowClass";
    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(10, 10, 10)); // Transparent background but not black

    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT, // Window style
        CLASS_NAME,
        "Overlay",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) {
        std::cerr << "Failed to create overlay window" << std::endl;
        return 1;
    }

    SetLayeredWindowAttributes(hwnd, RGB(10, 10, 10), 0, LWA_COLORKEY); // Make the window transparent

    ShowWindow(hwnd, SW_SHOW);

    HWND targetWnd = FindWindow(NULL, TARGET_WINDOW_NAME);
    if (!targetWnd) {
        std::cerr << "Target window not found!" << std::endl;
    }

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
        Update(hwnd, targetWnd);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // Shutdown GDI+
    GdiplusShutdown(gdiplusToken);
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        RenderText(hwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void Update(HWND hwnd, HWND targetWnd)
{
    InvalidateRect(hwnd, 0, false);
    MSG msg = { };
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (!targetWnd)
    {
        std::cerr << "Target window not found!" << std::endl;
        targetWnd = FindWindow(NULL, TARGET_WINDOW_NAME);
        ShowWindow(hwnd, SW_HIDE);
    }

    UpdateOverlayPosition(hwnd, targetWnd);
}



void RenderText(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // Create GDI+ graphics object
    Graphics graphics(hdc);

    // Clear the entire window with transparency
    graphics.Clear(Color(10, 10, 10)); //Not needed cuse in InvalidateRect()

    // Set text color and font
    FontFamily fontFamily(L"Consolas");
    Font font(&fontFamily, 20, FontStyleRegular, UnitPixel);
    SolidBrush solidBrush(Color(255, 255, 255, 255)); // White text
    SolidBrush outlinePen(Color(255, 0, 0, 0));
    // Text to render
    time_t currentTime = time(0);
    struct tm localTime;
    if (localtime_s(&localTime, &currentTime) == 0)
    {
        wchar_t timeString[128];
        if (std::wcsftime(timeString, 128, L"%c", &localTime))
        {
            RectF textRect;
            graphics.MeasureString(timeString, -1, &font, PointF(0, 0), &textRect);
            RECT rect;
            GetWindowRect(hwnd, &rect);
            graphics.DrawString(timeString, -1, &font, { rect.right - rect.left - textRect.Width - 15 + 1, 0.0f + 1 }, &outlinePen);
            graphics.DrawString(timeString, -1, &font, { rect.right - rect.left - textRect.Width - 15, 0.0f }, &solidBrush);
        }
    }
    std::regex pattern("Temp: (\\d+) C, R: (\\d+) KB/s, T: (\\d+) KB/s");
    std::smatch matches;
    std::string telemetry(rxbuffer);
    if (std::regex_search(telemetry, matches, pattern))
    {
        int temp = std::stoi(matches[1]);
        int read_speed = std::stoi(matches[2]);
        int write_speed = std::stoi(matches[3]);
        wchar_t buffer[100];
        swprintf(buffer, 100, L"CPU %4d °C\n↓ %4d KB/s\n↑ %4d KB/s", temp, read_speed, write_speed);
        graphics.DrawString(buffer, -1, &font, { 1,51 }, &outlinePen);
        graphics.DrawString(buffer, -1, &font, { 0,50 }, &solidBrush);
    }
    EndPaint(hwnd, &ps);
}

void UpdateOverlayPosition(HWND hwnd, HWND targetWnd) {
    RECT rect;
    if (GetWindowRect(targetWnd, &rect)) {
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        SetWindowPos(hwnd, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}
