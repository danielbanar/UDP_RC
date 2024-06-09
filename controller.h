#pragma once
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <XInput.h>
#pragma comment(lib, "XInput.lib")
#else
#include <SDL2/SDL.h>
#endif
class Controller
{
public:
    Controller(int i);
    ~Controller();
    void Poll();
    void Print();
    void Deadzone();
    std::string CreatePayload();
  
private:
#ifdef _WIN32
    XINPUT_STATE state;
#else
    SDL_GameController* sdlController;
#endif
    int controllerIndex;
    unsigned long nPacketNumber;
    unsigned short Buttons;
    short LT;
    short RT;
    short LX;
    short LY;
    short RX;
    short RY;
};