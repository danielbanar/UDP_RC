#pragma once
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <XInput.h>
#pragma comment(lib, "XInput.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#endif
class Controller
{
public:
    Controller(int i);
    void Poll();
    void Print();
    void Deadzone();
    std::string CreatePayload();
  
private:
    int controllerIndex;
    unsigned long nPacketNumber;
    unsigned short Buttons;
    unsigned char LT;
    unsigned char RT;
    short LX;
    short LY;
    short RX;
    short RY;
};