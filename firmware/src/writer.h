#pragma once

#include <stdbool.h>
#include "i2sreader.h"
#include "fatfs.h"
#include "cyclebuffer.h"

typedef struct Writer
{
    volatile bool          enabled;
    volatile atomic_uint   fileNumber;
    xSemaphoreHandle       semaphore;
    volatile atomic_uint   fsErrors;
    volatile atomic_uint   lastFsError;
    FIL                    file;
    unsigned               blink;
    volatile atomic_size_t payloadBytesInFile;
    volatile bool          fileOpened;
    volatile bool          realFileOpened;
    volatile bool          sdEmuEnabled;
    unsigned               micEmuCounter;
    volatile bool          micEmuEnabled;
    volatile bool          maxAllocEnabled;
    bool                   fatfsMounted;
    I2sReader              i2sReader;
    FATFS                  fatfs;
    CycleBuffer            cycleBuffer;
    volatile atomic_uint   sdEmuMaxDelayMs;
    volatile atomic_uint   sdEmuMinDelayMs;
    volatile atomic_uint   maxFileSize;

    // Отдельный поток
    osThreadAttr_t         taskAttributes;
    osThreadId_t           taskHandle;
    uint32_t               taskBuffer[1024];
    StaticTask_t           taskControlBlock;

    uint8_t                cycleBufferSpace[65536];
} Writer;

typedef struct WriterInfo
{
    bool              enabled;
    unsigned          fileNumber;
    unsigned          lostBytes;
    unsigned          lastFsError;
    unsigned          fsErrors;
    size_t            payloadBytesInFile;
    bool              fileOpened;
    bool              realFileOpened;
    unsigned          sdEmuEnabled;
    unsigned          micEmuEnabled;
    unsigned          maxAllocEnabled;
    unsigned          sdEmuMaxDelayMs;
    unsigned          sdEmuMinDelayMs;
    unsigned          maxFileSize;
} WriterInfo;

void writer_create(Writer* writer);
void writer_setEnabledMt(void* this_void, bool enabled);
void writer_setSdEmuEnabledMt(void* this_void, bool enabled);
void writer_setMicEmuEnabledMt(void* this_void, bool enabled);
void writer_setMaxAllocEnabledMt(void* this_void, bool enabled);
void writer_getInfoMt(void* this_void, WriterInfo* out);
void writer_setSdEmuMaxDelayMs(void* this_void, unsigned value);
void writer_setSdEmuMinDelayMs(void* this_void, unsigned value);
void writer_setMaxFileSize(void* this_void, unsigned value);
