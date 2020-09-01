// Циклический буфер для UART

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdatomic.h>

// *** Настройки ***

// Размер буфера передачи. Если задача будет передавть больше, то она будет тормозиться.

#define UART_CYCLE_BUFFER_TX_BUFFER_SIZE 256

// Размер буфера приёма. Если задача не будет успевать обрабатывать данные, то данные будут теряться.
// При этом будет установлен флаг rxOverflow

#define UART_CYCLE_BUFFER_RX_BUFFER_SIZE 16

// ***


typedef struct
{
    UART_HandleTypeDef*     port;
    xSemaphoreHandle        txSemaphore;
    xSemaphoreHandle        rxSemaphore;
    volatile atomic_size_t  rxWr;
    volatile atomic_size_t  rxRd;
    volatile bool           rxOverflow;
    volatile atomic_size_t  txWr;
    volatile atomic_size_t  txRd;
    volatile atomic_size_t  txRd2;
    volatile atomic_uchar   rxBuffer[UART_CYCLE_BUFFER_RX_BUFFER_SIZE];
    uint8_t                 safeArea;
    volatile atomic_uchar   txBuffer[UART_CYCLE_BUFFER_TX_BUFFER_SIZE];
} UartCycleBuffer;

// Функции

void    uartCycleBuffer_create(UartCycleBuffer* this, UART_HandleTypeDef* port);
void    uartCycleBuffer_irqHandler(UART_HandleTypeDef* port);
void    uartCycleBuffer_destroy(UartCycleBuffer* this);
uint8_t uartCycleBuffer_recv(UartCycleBuffer* this);
void    uartCycleBuffer_sendStr(UartCycleBuffer* this, const char* str);
void    uartCycleBuffer_send(UartCycleBuffer* this, const void* data, size_t dataSize);
