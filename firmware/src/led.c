// RGB светодиод

#include "led.h"
#include "main.h"

// *** Настройка ***

// Куда подключен светодид
static GPIO_TypeDef* const ledPort = GPIOC;
static const unsigned ledPortR = GPIO_PIN_9;
static const unsigned ledPortG = GPIO_PIN_10;
static const unsigned ledPortB = GPIO_PIN_13;

// ***

// Установка цвета свечения светодиода.

void ledSet(unsigned color)
{
    ledPort->BSRR = ((color & 1) ? (ledPortR << 16) : ledPortR)
                  | ((color & 2) ? (ledPortG << 16) : ledPortG)
                  | ((color & 4) ? (ledPortB << 16) : ledPortB);
}
