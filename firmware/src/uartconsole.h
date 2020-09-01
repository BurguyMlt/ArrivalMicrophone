#pragma once

#include "uartcyclebuffer.h"
#include "cmsis_os.h"
#include "writer.h"

typedef struct
{
    void* setEnabledArg;
    void  (*setEnabled)(void*, bool);

    void* setMicEmuEnabledArg;
    void  (*setMicEmuEnabled)(void*, bool);

    void* setSdEmuEnabledArg;
    void  (*setSdEmuEnabled)(void*, bool);

    void* setMaxAllocArg;
    void  (*setMaxAlloc)(void*, bool);

    void* getInfoArg;
    void  (*getInfo)(void*, WriterInfo* out);

    void* setSdEmuMaxDelayMsArg;
    void  (*setSdEmuMaxDelayMs)(void*, unsigned);

    void* setSdEmuMinDelayMsArg;
    void  (*setSdEmuMinDelayMs)(void*, unsigned);

    void* setMaxFileSizeArg;
    void  (*setMaxFileSize)(void*, unsigned);
} UartConsoleInterface;

typedef struct
{
    UartConsoleInterface interface;
    char                 cmdLine[256];
    UartCycleBuffer      uart;

    // Отдельный поток
    osThreadAttr_t       taskAttributes;
    osThreadId_t         taskHandle;
    uint32_t             taskBuffer[1024];
    StaticTask_t         taskControlBlock;
} UartConsole;

void uartConsole_create(UartConsole* writer);
