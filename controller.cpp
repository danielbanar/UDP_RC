#include "controller.h"

Controller::Controller(int i) : controllerIndex(i), nPacketNumber(0), Buttons(0), LT(0), RT(0), LX(0), LY(0), RX(0), RY(0)
{
#ifdef _WIN32
	ZeroMemory(&state, sizeof(XINPUT_STATE));
#else
	SDL_Init(SDL_INIT_GAMECONTROLLER);
	sdlController = SDL_GameControllerOpen(controllerIndex);
#endif
}
Controller::~Controller()
{
#ifdef _WIN32

#else
	if (sdlController != nullptr)
		SDL_GameControllerClose(sdlController);

#endif
}
void Controller::Poll()
{
#ifdef _WIN32
	DWORD dwResult = XInputGetState(controllerIndex, &state); // Get controller state
	if (dwResult == ERROR_SUCCESS)
	{
		nPacketNumber++;
		Buttons = state.Gamepad.wButtons;
		LT = state.Gamepad.bLeftTrigger;
		RT = state.Gamepad.bRightTrigger;
		LX = state.Gamepad.sThumbLX;
		LY = state.Gamepad.sThumbLY;
		RX = state.Gamepad.sThumbRX;
		RY = state.Gamepad.sThumbRY;
	}
	else
	{
		std::cerr << "Controller not connected." << std::endl;
	}
#else
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
	}
	if (sdlController != nullptr)
	{
		Buttons = SDL_GameControllerGetButton(sdlController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) * 256;
		LT = SDL_GameControllerGetAxis(sdlController, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 128;
		RT = SDL_GameControllerGetAxis(sdlController, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 128;
		LX = SDL_GameControllerGetAxis(sdlController, SDL_CONTROLLER_AXIS_LEFTX);
		LY = SDL_GameControllerGetAxis(sdlController, SDL_CONTROLLER_AXIS_LEFTY);
		RX = SDL_GameControllerGetAxis(sdlController, SDL_CONTROLLER_AXIS_RIGHTX);
		RY = SDL_GameControllerGetAxis(sdlController, SDL_CONTROLLER_AXIS_RIGHTY);
	}
	else
	{
		std::cerr << "Controller not connected." << std::endl;
		sdlController = SDL_GameControllerOpen(controllerIndex);
	}
#endif
}
std::string Controller::CreatePayload()
{
	std::string result = "N" + std::to_string(nPacketNumber) + "LX" + std::to_string(LX) + "LT" + std::to_string(LT) + "RT" + std::to_string(RT) + "B" + std::to_string(Buttons) + '\n';
	return result;
}

void Controller::Deadzone()
{
	if (LT < 10)
		LT = 0;
	if (RT < 10)
		RT = 0;
	if (-512 < LX && LX < 512)
		LX = 0;
}
void Controller::Print()
{
	printf("%8ld LX: %8d LT: %8d RT: %8d\n", nPacketNumber, LX, LT, RT);
}