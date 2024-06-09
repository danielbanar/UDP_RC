#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <iostream>
#include <ctime>
#include <locale>
#include <string>
#pragma comment (lib,"Gdiplus.lib")

using namespace Gdiplus;

class Overlay {
public:
    static Overlay* getInstance();
    void Init(const char* windowName);
    void Update();
    std::string telemetry;
private:
    static Overlay* instance;
    ULONG_PTR gdiplusToken;
    HWND targetWnd;
    HWND hwnd;
    const char* TARGET_WINDOW_NAME;
    Overlay();
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static void RenderText(HWND hwnd);
    void UpdateOverlayPosition(HWND overlayWnd);
};

