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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void RenderText(HWND hwnd);
void Update(HWND hwnd, HWND targetWnd);
void UpdateOverlayPosition(HWND hwnd, HWND targetWnd);
