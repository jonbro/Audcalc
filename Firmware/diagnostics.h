#pragma once
#include "hardware.h"
#include "ws2812.h"
#include "m6x118pt7b.h"
#include <stdio.h>


class Diagnostics
{
public:
    void run(); 
    void flashQuickClear();
private:
    void buttonTest();
    uint32_t color[25];
    uint32_t keyState = 0;
    uint32_t lastKeyState = 0;
    char str[64];
};