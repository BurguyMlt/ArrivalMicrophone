// Циклический буфер для UART

#include "uartcyclebuffer.h"
#include <string.h>

// Костыль для этого примера. Обычно я так не пишу. Я добавляю пользовательский параметр к
// callback функци UART_HandleTypeDef::TxCpltCallback. Сейчас же я не буду модифицировать
// бибиотеку и я не буду писать тут функцию поиска по массиву, с блокировками.
// Для примера напишу по простому.

static UartCycleBuffer* single = NULL;

static UartCycleBuffer* uartCycleBuffer_getByPort(UART_HandleTypeDef* port)
{
    if (port == single->port) return single;
    return NULL;
}

// Это обработчик прерывания UART. После прерывания UART проверяем, был ли получен байт.

void uartCycleBuffer_irqHandler(UART_HandleTypeDef* port)
{
    UartCycleBuffer* this = uartCycleBuffer_getByPort(port);
    if (this == NULL) return;

    if (port->RxXferCount == 1) // Был получен байт
    {
        size_t rxRd = atomic_load(&this->rxRd), rxWr = atomic_load(&this->rxWr);

        // Следующее место записи.
        size_t nextWr = (rxWr + 1) % sizeof(this->rxBuffer);

        // Контроль переполнения буфера.
        if (nextWr == rxRd) this->rxOverflow = true;
                       else atomic_store(&this->rxWr, nextWr);

        // Быстрый перезапуск приёма.
        port->RxXferCount = 2;
        port->pRxBuffPtr = (uint8_t*)(this->rxBuffer + nextWr);

        // Если задача остановлена в функции uartCycleBuffer_recv, то будим её.
        portBASE_TYPE xTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(this->rxSemaphore, &xTaskWoken);
        portYIELD_FROM_ISR(xTaskWoken);
    }
}

// Внутренняя функция. Запуск передачи в UART. Вызывается из прерывания или функции uartCycleBuffer_sendStr.

static void uartCycleBuffer_sendNext(UartCycleBuffer* this)
{
    size_t txRd = atomic_load(&this->txRd), txWr = atomic_load(&this->txWr);
    size_t bytesInTxBuffer = txRd <= txWr ? txWr - txRd : sizeof(this->txBuffer) - txRd;

    if (bytesInTxBuffer == 0) return;

    atomic_store(&this->txRd2, (txRd + bytesInTxBuffer) % sizeof(this->txBuffer));
    HAL_UART_Transmit_IT(this->port, (uint8_t*)this->txBuffer + txRd, bytesInTxBuffer);
}

// Внутренняя функция. Обработчик прерывания об завершении отправки данных в UART.

static void uartCycleBuffer_txCplt(UART_HandleTypeDef* port)
{
    UartCycleBuffer* this = uartCycleBuffer_getByPort(port);
    if (this == NULL) return;

    // Освобождение буфера отправки.
    atomic_store(&this->txRd, atomic_load(&this->txRd2));

    // Попытка продолжения отправки
    uartCycleBuffer_sendNext(this);

    // Если задача остановлена в функции uartCycleBuffer_send, то будим её.
    portBASE_TYPE xTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(this->txSemaphore, &xTaskWoken);
    portYIELD_FROM_ISR(xTaskWoken);
}

// Запуск объекта. Сразу начинается приём данных в кольцевой буфер.

void uartCycleBuffer_create(UartCycleBuffer* this, UART_HandleTypeDef* port)
{
    single = this; // Потому что в стандартной бибилиотеке у callback функции нет первого пользовательского параметра.

    this->port = port;
    atomic_store(&this->rxWr, 0);
    atomic_store(&this->rxRd, 0);
    atomic_store(&this->txWr, 0);
    atomic_store(&this->txRd, 0);
    this->rxOverflow = false;

    if (HAL_UART_RegisterCallback(port, HAL_UART_TX_COMPLETE_CB_ID, uartCycleBuffer_txCplt))
        Error_Handler(); // Функция без выхода

    vSemaphoreCreateBinary(this->rxSemaphore);
    if (this->rxSemaphore == NULL)
        Error_Handler(); // Функция без выхода

    vSemaphoreCreateBinary(this->txSemaphore);
    if (this->txSemaphore == NULL)
        Error_Handler(); // Функция без выхода

    if (HAL_UART_Receive_IT(port, (uint8_t*)this->rxBuffer, 2))
        Error_Handler(); // Функция без выхода
}

// Остановка приёма и разрушение объекта.

void uartCycleBuffer_destroy(UartCycleBuffer* this)
{
    HAL_UART_AbortReceive_IT(this->port);
    vSemaphoreDelete(this->rxSemaphore);
    vSemaphoreDelete(this->txSemaphore);

    if (HAL_UART_UnRegisterCallback(this->port, HAL_UART_TX_COMPLETE_CB_ID))
    {
        Error_Handler(); // Функция без выхода
        return;
    }

    single = NULL;
}

// Получение принятого байта. Если в приёмном буфере нет данных, то задача тормозится.

uint8_t uartCycleBuffer_recv(UartCycleBuffer* this)
{
    // Ждем, пока в буфере не появятся данные
    size_t rxRd = atomic_load(&this->rxRd);
    while(rxRd == atomic_load(&this->rxWr))
        xSemaphoreTake(this->rxSemaphore, portMAX_DELAY);

    // Возвращаем байт из буфера. Это не очень оптимально, для нормальной программы я бы вычитывал по несколько байт.
    uint8_t d = this->rxBuffer[rxRd];
    atomic_store(&this->rxRd, (rxRd + 1) % sizeof(this->rxBuffer));
    return d;
}

// Отправка блока данных в UART.
// Если буфер передачи не заполнен, то задача не тормозится.

void uartCycleBuffer_send(UartCycleBuffer* this, const void* data, size_t dataSize)
{
    // Копируем данные в кольцевой буфер отправки
    while (dataSize != 0)
    {
        // Сколько байт можно дописать в буфер
        size_t txRd = atomic_load(&this->txRd), txWr = atomic_load(&this->txWr);
        size_t freeSize = txRd <= txWr ? sizeof(this->txBuffer) - txWr + txRd - 1 : txRd - txWr - 1;

        // В буфере нет места, тогда подождем освобождения буфера
        if (freeSize == 0)
        {
            xSemaphoreTake(this->txSemaphore, portMAX_DELAY);
            continue;
        }

        // Сохраняем в кольцевой буфер
        size_t txSize = dataSize < freeSize ? dataSize : freeSize;
        size_t maxTxRightSize = sizeof(this->txBuffer) - txWr;
        size_t txRightSize = txSize < maxTxRightSize ? txSize : maxTxRightSize;
        memcpy((uint8_t*)this->txBuffer + txWr, data, txRightSize);
        memcpy((uint8_t*)this->txBuffer, (uint8_t*)data + txRightSize, txSize - txRightSize);
        atomic_store(&this->txWr, (txWr + txSize) % sizeof(this->txBuffer));
        data = (const uint8_t*)data + txSize;
        dataSize -= txSize;

        // Запуск предачи, если не запущена.
        if (this->port->gState != HAL_UART_STATE_BUSY_TX)
            uartCycleBuffer_sendNext(this);
    }
}

// Отправка строки в UART. Конец строки определяется по \0.

void uartCycleBuffer_sendStr(UartCycleBuffer* this, const char* str)
{
    uartCycleBuffer_send(this, str, strlen(str));
}

//! Что делать с overflow?
