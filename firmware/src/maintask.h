#pragma once

#include <stdbool.h>
#include "keyboard.h"
#include "writer.h"
#include "uartconsole.h"

typedef struct MainTask
{
    Keyboard    keyboard;
    Writer      writer;
    UartConsole uartConsole;
} MainTask;

extern MainTask mainTask;

void mainTask_main(void *argument);
