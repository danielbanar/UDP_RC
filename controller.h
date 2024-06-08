#pragma once
#include <iostream>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#include <XInput.h>
#else
//Lunix
#endif
class Controller
{
public:
    Controller(int i);
    void Poll();
    void Print();
    void Deadzone();
    std::string CreatePayload();
    ~Controller();
  
private:
    XINPUT_STATE state;
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