// Приём из I2S (DMA I2S на чтение в циклическом режиме).

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "main.h"

// *** Настройка ***

// Размер буфера DMA. Надо увеличить эту константу, если будут происходить пропуски прерываний.
#define I2S_READER_BUFFER_BYTES 2048

// ***

typedef struct I2sReaderInterface
{
    void* dataReadyArg;
    void (*dataReady)(void* arg, void* data, size_t dataSize);
} I2sReaderInterface;

typedef struct I2sReader
{
    I2sReaderInterface interface;
    I2S_HandleTypeDef* port;
    int32_t            buffer32[I2S_READER_BUFFER_BYTES / 4];
} I2sReader;

void i2sReader_create(I2sReader* this, I2S_HandleTypeDef* port);
void i2sReader_destroy(I2sReader* this);

//! Вынести буфер в запускающий объект
