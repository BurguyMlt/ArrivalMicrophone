#include "main.h"
#include <limits.h>
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "stm32f4xx_hal_gpio.h"
#include "keyboard.h"

// *** Настройки ***

// Куда подключена клавиша ВКЛ/ВЫКЛ
static GPIO_TypeDef* const keyboardPort = GPIOA;
static const unsigned keyboardPin = GPIO_PIN_0;

// Через сколько миллисекунд нажатия будет срабатывать обработчик выключения.
static const unsigned keyboardOffPressMs = 1000;

// Период сканирования клавиатуры
static const unsigned keyboardScanIntervalMs = 50;

// ***

void keyboard_createAndPickup(Keyboard* this)
{
    unsigned pressTime = 0;

    const unsigned keyboardOffPressN = keyboardOffPressMs / keyboardScanIntervalMs;

    for (;;)
    {
        if ((keyboardPort->IDR & keyboardPin) == 0) // Клавиша нажата
        {
            if (pressTime < UINT_MAX)
            {
                pressTime++;
                if (pressTime >= keyboardOffPressN)
                {
                    pressTime = UINT_MAX;
                    this->interface.keyPress(this->interface.keyPressArg, keyOff);
                }
            }
        }
        else // Клавиаша отпущена
        {
            if (pressTime != 0 && pressTime != UINT_MAX)
            {
                this->interface.keyPress(this->interface.keyPressArg, keyOn);
            }
            pressTime = 0;
        }
        osDelay(pdMS_TO_TICKS(keyboardScanIntervalMs));
    }
}
