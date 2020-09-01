// Циклиеский буфер для передачи из прерывания в процесс

#include "cyclebuffer.h"
#include <string.h>
#include "main.h"

void cycleBuffer_create(CycleBuffer* this, void* buffer, size_t bufferSize)
{
    this->buffer = buffer;
    this->bufferSize = bufferSize;
    atomic_store(&this->wr, 0);
    atomic_store(&this->rd, 0);
    atomic_store(&this->lostBytes, 0);

    vSemaphoreCreateBinary(this->semaphore);
    if (this->semaphore == NULL)
        Error_Handler(); // Функция без выхода
}

void cycleBuffer_destroy(CycleBuffer* this)
{
    vSemaphoreDelete(this->semaphore);
}

// Получить указатель на данные в буфере и их длину

void* cycleBuffer_get(CycleBuffer* this, size_t* outSize)
{
    // Ждем, пока в буфере не появятся данные
    size_t rd = atomic_load(&this->rd);
    size_t wr;
    for(;;)
    {
        wr = atomic_load(&this->wr);
        if (rd != wr) break;
        xSemaphoreTake(this->semaphore, portMAX_DELAY);
    }

    *outSize = rd <= wr ? wr - rd : this->bufferSize - rd;
    return this->buffer + rd;
}

// Удалить данные из кольцевого буфера

void* cycleBuffer_pop(CycleBuffer* this, size_t outSize)
{
    size_t rd = atomic_load(&this->rd) + outSize;
    while (rd >= this->bufferSize) rd -= this->bufferSize;
    atomic_store(&this->rd, rd);
}

// Поместить данные в кольцевой буфер

void cycleBuffer_putFromInterrupt(CycleBuffer* this, const void* data, size_t dataSize)
{
    // Сколько байт можно дописать в буфер
    size_t rd = atomic_load(&this->rd), wr = atomic_load(&this->wr);
    size_t freeSize = rd <= wr ? this->bufferSize - wr + rd - 1 : rd - wr - 1;

    // Записываем сколько влезет
    if (dataSize > freeSize)
    {
        atomic_fetch_add(&this->lostBytes, dataSize);
        dataSize = freeSize;
    }

    // Нечего записывать
    if (dataSize == 0) return;

    // Сохраняем в кольцевой буфер
    size_t maxTxRightSize = this->bufferSize - wr;
    size_t txRightSize = dataSize < maxTxRightSize ? dataSize : maxTxRightSize;
    memcpy(this->buffer + wr, data, txRightSize);
    memcpy(this->buffer, (uint8_t*)data + txRightSize, dataSize - txRightSize);
    wr += dataSize;
    while (wr >= this->bufferSize) wr -= this->bufferSize;
    atomic_store(&this->wr, wr);

    // Если задача остановлена в функции cycleBuffer_get, то будим её.
    portBASE_TYPE xTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(this->semaphore, &xTaskWoken);
    portYIELD_FROM_ISR(xTaskWoken);
}

// Получить кол-во потерянных байт

size_t cycleBuffer_getLostBytesMt(CycleBuffer* this)
{
    return atomic_load(&this->lostBytes);
}
