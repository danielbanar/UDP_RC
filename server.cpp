#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <ctime>
#include <locale>
#include <string>
#include <regex>
#include "controller.h"
#include "overlay.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "Dwrite.lib")

#define CHAR_TO_WCHAR(str) \
    ([](const char* input) { \
        int length = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0); \
        wchar_t* output = new wchar_t[length]; \
        MultiByteToWideChar(CP_UTF8, 0, input, -1, output, length); \
        return output; \
    })(str)

const char* TARGET_WINDOW_NAME = "Direct3D11 renderer";
char rxbuffer[128] = { 0 };
ID2D1Factory* pFactory = NULL;
ID2D1HwndRenderTarget* pRenderTarget = NULL;
IDWriteFactory* pDWriteFactory = NULL;
IDWriteTextFormat* pTextFormat = NULL;
ID2D1SolidColorBrush* pBrush = NULL;
ID2D1SolidColorBrush* pOutlineBrush = NULL;

void InitD2D(HWND hwnd);
void CleanupD2D();
void RenderText(HWND hwnd);
void Update(HWND hwnd, HWND targetWnd);

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

    const char* CLASS_NAME = "OverlayWindowClass";
    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH); // Transparent background

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

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY); // Make the window transparent

    ShowWindow(hwnd, SW_SHOW);

    InitD2D(hwnd);

    HWND targetWnd = FindWindow(NULL, TARGET_WINDOW_NAME);
    if (!targetWnd) {
        std::cerr << "Target window not found!" << std::endl;
    }

    while (true)
    {
        controller.Poll();
        controller.Deadzone();
        std::string messageToSend = controller.CreatePayload();
        memset(rxbuffer, 0, sizeof(rxbuffer));
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

    CleanupD2D();
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
void ResizeRenderTarget(HWND hwnd)
{
    if (pRenderTarget)
    {
        RECT rc;
        GetClientRect(hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(
            rc.right - rc.left,
            rc.bottom - rc.top
        );

        pRenderTarget->Resize(size);
    }
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        RenderText(hwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        ResizeRenderTarget(hwnd);
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void Update(HWND hwnd, HWND targetWnd)
{
    InvalidateRect(hwnd, 0, true);
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

    if (!pRenderTarget)
    {
        HRESULT hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(
                    DXGI_FORMAT_B8G8R8A8_UNORM,
                    D2D1_ALPHA_MODE_PREMULTIPLIED)
            ),
            D2D1::HwndRenderTargetProperties(
                hwnd,
                D2D1::SizeU(
                    ps.rcPaint.right - ps.rcPaint.left,
                    ps.rcPaint.bottom - ps.rcPaint.top)
            ),
            &pRenderTarget);

        if (SUCCEEDED(hr))
        {
            pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::White),
                &pBrush);

            pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Black),
                &pOutlineBrush);
        }
    }

    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)); // Fully transparent background

    D2D1_RECT_F layoutRect = D2D1::RectF(
        static_cast<FLOAT>(ps.rcPaint.left),
        static_cast<FLOAT>(ps.rcPaint.top),
        static_cast<FLOAT>(ps.rcPaint.right),
        static_cast<FLOAT>(ps.rcPaint.bottom)
    );

    time_t currentTime = time(0);
    struct tm localTime;
    if (localtime_s(&localTime, &currentTime) == 0)
    {
        wchar_t timeString[128];
        if (std::wcsftime(timeString, 128, L"%c", &localTime))
        {
            pRenderTarget->DrawText(
                timeString,
                wcslen(timeString),
                pTextFormat,
                layoutRect,
                pBrush
            );
        }
    }
    static wchar_t telemetryBuffer[128];
    std::regex pattern("Temp: (\\d+) C, R: (\\d+) KB/s, T: (\\d+) KB/s, RSSI: (-?\\d+), SNR: (-?\\d+)");
    std::smatch matches;
    std::string telemetry(rxbuffer);
    if (std::regex_search(telemetry, matches, pattern))
    {
        int temp = std::stoi(matches[1]);
        int read_speed = std::stoi(matches[2]);
        int write_speed = std::stoi(matches[3]);
        int rssi = std::stoi(matches[4]);
        int snr = std::stoi(matches[5]);

        swprintf(telemetryBuffer, 128, L"CPU %4d °C\n↓ %4d KB/s\n↑ %4d KB/s\nRSSI: %4d\nSNR: %5d", temp, read_speed, write_speed, rssi, snr);
    }
    layoutRect.top += 30.0f; // Move the position down for the next text
    pRenderTarget->DrawText(
        telemetryBuffer,
        wcslen(telemetryBuffer),
        pTextFormat,
        layoutRect,
        pBrush
    );
    pRenderTarget->EndDraw();

    EndPaint(hwnd, &ps);
}

void InitD2D(HWND hwnd)
{
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));
    pDWriteFactory->CreateTextFormat(
        L"Consolas",
        NULL,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        20.0f,
        L"",
        &pTextFormat);
}

void CleanupD2D()
{
    if (pBrush) pBrush->Release();
    if (pTextFormat) pTextFormat->Release();
    if (pDWriteFactory) pDWriteFactory->Release();
    if (pRenderTarget) pRenderTarget->Release();
    if (pFactory) pFactory->Release();
}

void UpdateOverlayPosition(HWND hwnd, HWND targetWnd) {
    RECT rect;
    if (GetWindowRect(targetWnd, &rect)) {
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        SetWindowPos(hwnd, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}
