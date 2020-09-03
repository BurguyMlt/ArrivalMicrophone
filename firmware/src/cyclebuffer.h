// Циклиеский буфер для передачи из прерывания в процесс

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct CycleBuffer
{
    xSemaphoreHandle       semaphore;
    volatile atomic_size_t wr;
    volatile atomic_size_t rd;
    volatile bool          overflow;
    volatile atomic_size_t lostBytes;
    size_t                 bufferSize;
    uint8_t*               buffer;
} CycleBuffer;

void   cycleBuffer_create(CycleBuffer*, void* buffer, size_t bufferSize);
void   cycleBuffer_destroy(CycleBuffer*);
void*  cycleBuffer_get(CycleBuffer* this, size_t* outSize);
void*  cycleBuffer_pop(CycleBuffer* this, size_t size);
void   cycleBuffer_putFromInterrupt(CycleBuffer* this, const void* data, size_t dataSize);
size_t cycleBuffer_getLostBytesMt(CycleBuffer* this);
