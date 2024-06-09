#include "overlay.h"
#include <regex>
#define CHAR_TO_WCHAR(str) \
    ([](const char* input) { \
        int length = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0); \
        wchar_t* output = new wchar_t[length]; \
        MultiByteToWideChar(CP_UTF8, 0, input, -1, output, length); \
        return output; \
    })(str)
Overlay* Overlay::instance = nullptr;

Overlay::Overlay() {
	// Initialize GDI+
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);


}

Overlay* Overlay::getInstance() {
	if (!instance) {
		instance = new Overlay();
	}
	return instance;
}

void Overlay::Init(const char* windowName)
{
	TARGET_WINDOW_NAME = windowName;
	// Find the target window
	targetWnd = FindWindow(NULL, TARGET_WINDOW_NAME);
	if (targetWnd == NULL) {
		std::cerr << "Target window not found!" << std::endl;
	}
	const char* CLASS_NAME = "OverlayWindowClass";
	HINSTANCE hInstance = GetModuleHandle(NULL);

	WNDCLASS wc = { };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = CreateSolidBrush(RGB(10, 10, 10)); // Transparent background but not black

	RegisterClass(&wc);

	hwnd = CreateWindowEx(
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

	if (hwnd == NULL) {
		std::cerr << "Failed to create overlay window" << std::endl;
		return;
	}

	SetLayeredWindowAttributes(hwnd, RGB(10, 10, 10), 0, LWA_COLORKEY); // Make the window transparent

	ShowWindow(hwnd, SW_SHOW);
}

void Overlay::Update() {
	InvalidateRect(hwnd,0,false);
	MSG msg = { };
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			return;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}


	if (targetWnd == NULL) {
		std::cerr << "Target window not found!" << std::endl;
		targetWnd = FindWindow(NULL, TARGET_WINDOW_NAME);
	}

	UpdateOverlayPosition(hwnd);
}

LRESULT CALLBACK Overlay::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

void Overlay::RenderText(HWND hwnd) {
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
			graphics.DrawString(timeString, -1, &font, { rect.right - rect.left - textRect.Width - 15+1, 0.0f+1 }, &outlinePen);
			graphics.DrawString(timeString, -1, &font, { rect.right - rect.left - textRect.Width - 15, 0.0f }, &solidBrush);
		}
	}
	std::regex pattern("Temp: (\\d+) C, R: (\\d+) KB/s, T: (\\d+) KB/s");
	std::smatch matches;

	if (std::regex_search(Overlay::getInstance()->telemetry, matches, pattern))
	{
		int temp = std::stoi(matches[1]);
		int read_speed = std::stoi(matches[2]);
		int write_speed = std::stoi(matches[3]);
		wchar_t buffer[100];
		swprintf(buffer, 100, L"T %4d °C\n↓ %4d KB/s\n↑ %4d KB/s", temp, read_speed, write_speed);
		graphics.DrawString(buffer, -1, &font, {1,51}, &outlinePen);
		graphics.DrawString(buffer, -1, &font, {0,50}, &solidBrush);
	}
	EndPaint(hwnd, &ps);
}

void Overlay::UpdateOverlayPosition(HWND overlayWnd) {
	RECT rect;
	if (GetWindowRect(targetWnd, &rect)) {
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;
		SetWindowPos(overlayWnd, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
	}

}
