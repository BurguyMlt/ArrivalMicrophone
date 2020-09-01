#include "uartconsole.h"
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "fatfs.h"
#include "stm32f4xx_hal_spi.h"
#include "stm32f4xx_hal_gpio.h"
#include <string.h>
#include "uartcyclebuffer.h"
#include <stdio.h>
#include "writer.h"
#include <stdlib.h>

// Куда подключен UART

static UART_HandleTypeDef* consoleUart = &huart2;

// Получение строки от пользователя

static void uartConsole_readLine(UartConsole* this)
{
    size_t cmdLinePos = 0;
    for(;;)
    {
        char c = (char)uartCycleBuffer_recv(&this->uart);
        if (c == 8)
        {
            if (cmdLinePos == 0)
            {
                uartCycleBuffer_send(&this->uart, "\x07", 1);
                continue;
            }
            cmdLinePos--;
            uartCycleBuffer_send(&this->uart, "\x08 \x08", 3);
            continue;
        }
        if (c == 13)
        {
            this->cmdLine[cmdLinePos] = 0;
            uartCycleBuffer_send(&this->uart, "\r\n", 2);
            break;
        }
        if (cmdLinePos + 1 >= sizeof(this->cmdLine))
        {
            uartCycleBuffer_send(&this->uart, "\x07", 1);
            continue;
        }
        this->cmdLine[cmdLinePos] = c;
        cmdLinePos++;
        uartCycleBuffer_send(&this->uart, &c, 1);
    }
}

static bool uartConsole_cmdHelp(UartConsole* this, char* args);

static bool uartConsole_cmdOn(UartConsole* this, char* args)
{
    if (args[0] != 0) return false;
    this->interface.setEnabled(this->interface.setEnabledArg, true);
    return true;
}

static bool uartConsole_cmdOff(UartConsole* this, char* args)
{
    if (args[0] != 0) return false;
    this->interface.setEnabled(this->interface.setEnabledArg, false);
    return true;
}

static bool uartConsole_cmdInfo(UartConsole* this, char* args)
{
    if (args[0] != 0) return false;

    WriterInfo info;
    this->interface.getInfo(this->interface.getInfoArg, &info);

    int r = snprintf(this->cmdLine, sizeof(this->cmdLine),
                     "enabled            = %u\r\n"
                     "lostBytes          = %u\r\n"
                     "lastFsError        = %u\r\n"
                     "fileNumber         = %u\r\n"
                     "fsErrors           = %u\r\n",
                     (unsigned)info.enabled,
                     (unsigned)info.lostBytes,
                     (unsigned)info.lastFsError,
                     (unsigned)info.fileNumber,
                     (unsigned)info.fsErrors
             );

    if (r < 0 || r >= sizeof(this->cmdLine))
        Error_Handler(); // Функция не возращается

    uartCycleBuffer_sendStr(&this->uart, this->cmdLine);

    r = snprintf(this->cmdLine, sizeof(this->cmdLine),
                     "payloadBytesInFile = %u\r\n"
                     "fileOpened         = %u\r\n"
                     "realFileOpened     = %u\r\n"
                     "sdemu              = %u\r\n"
                     "micemu             = %u\r\n"
                     "maxalloc           = %u\r\n",
                     (unsigned)info.payloadBytesInFile,
                     (unsigned)info.fileOpened,
                     (unsigned)info.realFileOpened,
                     (unsigned)info.sdEmuEnabled,
                     (unsigned)info.micEmuEnabled,
                     (unsigned)info.maxAllocEnabled
             );

    if (r < 0 || r >= sizeof(this->cmdLine))
        Error_Handler(); // Функция не возращается

    uartCycleBuffer_sendStr(&this->uart, this->cmdLine);

    r = snprintf(this->cmdLine, sizeof(this->cmdLine),
                     "maxfilesize        = %u\r\n"
                     "mindelay           = %u\r\n"
                     "maxdelay           = %u\r\n",
                     (unsigned)info.maxFileSize,
                     (unsigned)info.sdEmuMinDelayMs,
                     (unsigned)info.sdEmuMaxDelayMs
             );

    if (r < 0 || r >= sizeof(this->cmdLine))
        Error_Handler(); // Функция не возращается

    uartCycleBuffer_sendStr(&this->uart, this->cmdLine);

    return true;
}

static unsigned parseOnOffArg(const char* arg)
{
    if (arg[0] == '0' && arg[1] == 0) return 0;
    if (arg[0] == '1' && arg[1] == 0) return 1;
    return 2;
}

static bool uartConsole_cmdMicEmu(UartConsole* this, char* arg)
{
    unsigned onOff = parseOnOffArg(arg);
    if (onOff > 1) return false;
    this->interface.setMicEmuEnabled(this->interface.setMicEmuEnabledArg, onOff != 0);
    return true;
}

static bool uartConsole_cmdSdEmu(UartConsole* this, char* arg)
{
    unsigned onOff = parseOnOffArg(arg);
    if (onOff > 1) return false;
    this->interface.setSdEmuEnabled(this->interface.setSdEmuEnabledArg, onOff != 0);
    return true;
}

static bool uartConsole_cmdMaxAlloc(UartConsole* this, char* arg)
{
    unsigned onOff = parseOnOffArg(arg);
    if (onOff > 1) return false;
    this->interface.setMaxAlloc(this->interface.setMaxAllocArg, onOff != 0);
    return true;
}

static bool uartConsole_cmdMinDelay(UartConsole* this, char* arg)
{
    char* end = "";
    unsigned long value = strtoul(arg, &end, 10);
    if (end[0] != 0 || value > 10000) return false;
    this->interface.setSdEmuMinDelayMs(this->interface.setSdEmuMinDelayMsArg, (unsigned)value);
    return true;
}

static bool uartConsole_cmdMaxDelay(UartConsole* this, char* arg)
{
    char* end = "";
    unsigned long value = strtoul(arg, &end, 10);
    if (end[0] != 0 || value > 10000) return false;
    this->interface.setSdEmuMaxDelayMs(this->interface.setSdEmuMaxDelayMsArg, (unsigned)value);
    return true;
}

static bool uartConsole_cmdMaxFileSize(UartConsole* this, char* arg)
{
    char* end = "";
    unsigned long value = strtoul(arg, &end, 10);
    if (end[0] != 0 || value > UINT32_MAX) return false;
    this->interface.setMaxFileSize(this->interface.setMaxFileSizeArg, (unsigned)value);
    return true;
}

typedef struct Command
{
    const char* cmd;
    const char* descr;
    bool (*handler)(UartConsole* this, char* args);
} Command;

static Command commands[] =
{
    { "help",        "\tShow usage",                                         uartConsole_cmdHelp        },
    { "info",        "\tShow info",                                          uartConsole_cmdInfo        },
    { "on",          "\tEnable microphone recoring to SD card",              uartConsole_cmdOn          },
    { "off",         "\tDisable microphone recoring to SD card",             uartConsole_cmdOff         },
    { "micemu",      "\tEnable/disable microphone emulation (0, 1)",         uartConsole_cmdMicEmu      },
    { "sdemu",       "\tEnable/disable SD card emulation (0, 1)",            uartConsole_cmdSdEmu       },
    { "maxalloc",    "Enable/disable preallocate maximum space (on, off)",   uartConsole_cmdMaxAlloc    },
    { "mindelay",    "Set minimal delay for SD Card emulation (ms)",         uartConsole_cmdMinDelay    },
    { "maxdelay",    "Set maximal delay for SD Card emulation (ms)",         uartConsole_cmdMaxDelay    },
    { "maxfilesize", "Set maximal file size (bytes)",                        uartConsole_cmdMaxFileSize },
    { NULL }
};

static bool uartConsole_cmdHelp(UartConsole* this, char* args)
{
    if (args[0] != 0) return false;
    Command* i;
    for (i = commands; i->cmd; i++)
    {
        uartCycleBuffer_sendStr(&this->uart, i->cmd);
        uartCycleBuffer_sendStr(&this->uart, "\t");
        uartCycleBuffer_sendStr(&this->uart, i->descr);
        uartCycleBuffer_sendStr(&this->uart, "\r\n");
    }
    return true;
}

static Command* findCommand(const char* name)
{
    Command* i;
    for (i = commands; i->cmd;  i++)
        if (0 == strcmp(i->cmd, name))
            return i;
    return NULL;
}

void uartConsole_main(void *argument)
{
    UartConsole* this = (UartConsole*)argument;

    // Вывод отладочной информации в UART
    //! Не совсем правильно использовать __DATE__
    uartCycleBuffer_sendStr(&this->uart, "Arrival Microphone. Version 0.1. Build " __DATE__ " "__TIME__ ". Alemorf.\r\n");

    for (;;)
    {
        // Получаем строку
        uartConsole_readLine(this);

        // Разделяем строку на команду и параметр
        char* args = strchr(this->cmdLine, ' ');
        if (args == NULL) args = this->cmdLine + strlen(this->cmdLine);
                     else *args++ = 0;

        // Ищем команду
        Command* cmd = findCommand(this->cmdLine);
        if (!cmd)
        {
            uartCycleBuffer_sendStr(&this->uart, "Invalid command.\r\n");
        }
        else
        {
            bool result = cmd->handler(this, args);
            uartCycleBuffer_sendStr(&this->uart, result ? "Ok\r\n" : "Error\r\n");
        }
    }
}

// Запуск
void uartConsole_create(UartConsole* this)
{
    // Иницализация буфера
    uartCycleBuffer_create(&this->uart, consoleUart);

    this->taskAttributes.name       = "uartConsoleTask";
    this->taskAttributes.stack_mem  = &this->taskBuffer[0];
    this->taskAttributes.stack_size = sizeof(this->taskBuffer);
    this->taskAttributes.cb_mem     = &this->taskControlBlock;
    this->taskAttributes.cb_size    = sizeof(this->taskControlBlock);
    this->taskAttributes.priority   = (osPriority_t)osPriorityLow;
    this->taskHandle = osThreadNew(uartConsole_main, (void*)this, &this->taskAttributes);
}
