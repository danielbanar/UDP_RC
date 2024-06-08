#include <iostream>
#include "controller.h"
#include <string>

#pragma comment(lib, "XInput.lib")


Controller::Controller(int i) : controllerIndex(i), nPacketNumber(0), Buttons(0)
{
	ZeroMemory(&state, sizeof(XINPUT_STATE));
}

Controller::~Controller()
{
}
void Controller::Poll()
{
	DWORD dwResult = XInputGetState(controllerIndex, &state); // Get controller state

	if (dwResult == ERROR_SUCCESS) {
		nPacketNumber++;
		Buttons = state.Gamepad.wButtons;
		LT = state.Gamepad.bLeftTrigger;
		RT = state.Gamepad.bRightTrigger;
		LX = state.Gamepad.sThumbLX;
		LY = state.Gamepad.sThumbLY;
		RX = state.Gamepad.sThumbRX;
		RY = state.Gamepad.sThumbRY;
	}
	else {
		std::cerr << "Controller not connected." << std::endl;
	}
}
std::string Controller::CreatePayload()
{
	std::string result = "N" + std::to_string(nPacketNumber) + "LX" + std::to_string(LX) + "LT" + std::to_string(LT) + "RT" + std::to_string(RT) + "B" + std::to_string(Buttons) +'\n';
	return result;
}

void Controller::Deadzone()
{
	if (LT < 10)
		LT = 0;
	if (RT < 10)
		RT = 0;
	if ( -512 < LX && LX < 512)
		LX = 0;
}
void Controller::Print()
{ 
	printf("%8ld LX: %8d LT: %8d RT: %8d\n", nPacketNumber, LX, LT, RT);
}