#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include "cmsis_os.h"

enum KeyCode
{
    keyOn = 1,
    keyOff = 2,
};

typedef struct
{
    void* keyPressArg;
    void (*keyPress)(void* arg, enum KeyCode code);
} KeyboardInterface;

typedef struct Keyboard
{
    KeyboardInterface    interface;
} Keyboard;

void keyboard_createAndPickup(Keyboard* this);

