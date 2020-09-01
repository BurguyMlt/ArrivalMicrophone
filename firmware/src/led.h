// RGB светодиод

#pragma once

enum { ledOff = 0, ledRed = 1, ledGreen = 2, ledBlue = 4 };

void ledSet(unsigned color);
