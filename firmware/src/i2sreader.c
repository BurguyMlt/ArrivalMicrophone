// Приём из I2S (DMA I2S на чтение в циклическом режиме).

#include "i2sreader.h"
#include "led.h"

// Костыль для этого примера. Обычно я так не пишу. Я добавляю пользовательский параметр к
// callback функци I2S_HandleTypeDef::RxCpltCallback. Сейчас же я не буду модифицировать
// бибиотеку и я не буду писать тут функцию поиска по массиву.
// Для примера напишу по простому.

static I2sReader* single = NULL;

static I2sReader* i2sReader_getByPort(I2S_HandleTypeDef* port)
{
    if (port == single->port) return single;
    return NULL;
}

// Эта функция вызывается обработчиками прерывания DMA I2S, когда заполнена половина приёмного буфера.

static void i2sReader_ready(I2sReader* this, int32_t* buffer)
{
    if (this->port->Init.DataFormat == I2S_DATAFORMAT_24B || this->port->Init.DataFormat == I2S_DATAFORMAT_32B)
    {
        // Преобразовать из формата 32 бита в формат 16 бит
        int32_t* s = buffer;
        int32_t* i = buffer;
        int32_t* ie = i + I2S_READER_BUFFER_BYTES / 8;
        do
        {
            int32_t l = *s++; // Левый канал
            int32_t r = *s++; // Правый канал
            *i++ = ((l >> 16) & 0xFF) | ((l & 0xFF) << 8) | (r & 0xFF0000) | ((r & 0xFF) << 24);
        } while (i != ie);

        // Данных стало в 2 раза меньше
        this->interface.dataReady(this->interface.dataReadyArg, buffer, I2S_READER_BUFFER_BYTES / 4);
        return;
    }

    this->interface.dataReady(this->interface.dataReadyArg, buffer, I2S_READER_BUFFER_BYTES / 2);
}

// Обработчик прерывания DMA I2S, когда заполнена первая половина приёмного буфера.

static void i2sReader_halfCpltIrq(I2S_HandleTypeDef* port)
{
    I2sReader* this = i2sReader_getByPort(port);
    if (this == NULL) return;

    i2sReader_ready(this, this->buffer32);
}

// Обработчик прерывания DMA I2S, когда заполнена вторая половина приёмного буфера.

static void i2sReader_cpltIrq(I2S_HandleTypeDef* port)
{
    I2sReader* this = i2sReader_getByPort(port);
    if (this == NULL) return;

    i2sReader_ready(this, this->buffer32 + I2S_READER_BUFFER_BYTES / 8);
}

// Запуск объекта. Сразу начинается приём данных в кольцевой буфер по DMA.

void i2sReader_create(I2sReader* this, I2S_HandleTypeDef* port)
{
    single = this;

    this->port = port;

    if (HAL_I2S_RegisterCallback(port, HAL_I2S_RX_COMPLETE_CB_ID, i2sReader_cpltIrq))
        Error_Handler(); // Функция без выхода

    if (HAL_I2S_RegisterCallback(port, HAL_I2S_RX_HALF_COMPLETE_CB_ID, i2sReader_halfCpltIrq))
        Error_Handler(); // Функция без выхода

    // Обмен 16 битными значениями, поэтому делим на 1. И еще над на 2 поделить из-за особенностей бибиотеки.
    if (HAL_I2S_Receive_DMA(port, (uint16_t*)this->buffer32, sizeof(this->buffer32) / 4))
        Error_Handler(); // Функция без выхода
}

// Разрушение объекта. Приём оканчивается.

void i2sReader_destroy(I2sReader* this)
{
    HAL_I2S_DMAStop(&hi2s2);

    if (HAL_I2S_UnRegisterCallback(this->port, HAL_I2S_RX_COMPLETE_CB_ID))
        Error_Handler(); // Функция без выхода

    if (HAL_I2S_UnRegisterCallback(this->port, HAL_I2S_RX_HALF_COMPLETE_CB_ID))
        Error_Handler(); // Функция без выхода

    single = NULL;
}
