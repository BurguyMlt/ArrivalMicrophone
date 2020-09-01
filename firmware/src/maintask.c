#include "maintask.h"
#include "main.h"
#include "led.h"
#include "cmsis_os.h"
#include "fatfs.h"
#include <stdio.h>
#include <stdlib.h>
#include "uartconsole.h"
#include "wav.h"

MainTask mainTask = { 0 };

static void mainTask_onKeyboardMt(void *argument, enum KeyCode keyCode)
{
    MainTask* this = (MainTask*)argument;

    switch (keyCode)
    {
        case keyOn:
            writer_setEnabledMt(&this->writer, true);
            break;
        case keyOff:
            writer_setEnabledMt(&this->writer, false);
            break;
    }
}

void mainTask_main(void *argument)
{
    MainTask* this = (MainTask*)argument;

    // Запуск объекта записи на SD
    writer_create(&this->writer);

    // Запуск объекта UART-консоли
    this->uartConsole.interface.setMicEmuEnabledArg = &this->writer;
    this->uartConsole.interface.setMicEmuEnabled = writer_setMicEmuEnabledMt;
    this->uartConsole.interface.setSdEmuEnabledArg = &this->writer;
    this->uartConsole.interface.setSdEmuEnabled = writer_setSdEmuEnabledMt;
    this->uartConsole.interface.setEnabledArg = &this->writer;
    this->uartConsole.interface.setEnabled = writer_setEnabledMt;
    this->uartConsole.interface.getInfoArg = &this->writer;
    this->uartConsole.interface.getInfo = writer_getInfoMt;
    this->uartConsole.interface.setMaxAllocArg = &this->writer;
    this->uartConsole.interface.setMaxAlloc = writer_setMaxAllocEnabledMt;
    this->uartConsole.interface.setSdEmuMaxDelayMsArg = &this->writer;
    this->uartConsole.interface.setSdEmuMaxDelayMs = writer_setSdEmuMaxDelayMs;
    this->uartConsole.interface.setSdEmuMinDelayMsArg = &this->writer;
    this->uartConsole.interface.setSdEmuMinDelayMs = writer_setSdEmuMinDelayMs;
    this->uartConsole.interface.setMaxFileSizeArg = &this->writer;
    this->uartConsole.interface.setMaxFileSize = writer_setMaxFileSize;

    uartConsole_create(&this->uartConsole);

    // Запуск объекта клавиатуры
    this->keyboard.interface.keyPressArg = this;
    this->keyboard.interface.keyPress = mainTask_onKeyboardMt;
    keyboard_createAndPickup(&this->keyboard);
}

//! Не отработано вытаскивание карты
