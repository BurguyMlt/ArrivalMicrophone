#include "maintask.h"
#include "main.h"
#include "led.h"
#include "cmsis_os.h"
#include "fatfs.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "uartconsole.h"
#include "wav.h"

// *** Настройки ***

// Используемый порт I2S
static I2S_HandleTypeDef* i2sPort = &hi2s2;

// Шаблон имени файла
static const char fileNameTemplate[] = "0:/%u.WAV";

// Эмуляция файловой системы

static void sdEmuDelay(Writer* this)
{
    unsigned min = atomic_load(&this->sdEmuMinDelayMs), max = atomic_load(&this->sdEmuMaxDelayMs);
    if (min > 0 && min <= max)
    {
        unsigned delayMs = min + rand() % (max - min + 1);
        osDelay(pdMS_TO_TICKS(delayMs));
    }
}

static bool writer_mount(Writer* this)
{
    if (!this->fatfsMounted)
    {
        FRESULT r = f_mount(&this->fatfs, SDPath, 1);
        if (r != FR_OK)
        {
            atomic_store(&this->lastFsError, r);
            return false;
        }
        this->fatfsMounted = true;
    }
    return true;
}

// Создание нового файла

static bool writer_createFile(Writer* this)
{
    if (this->sdEmuEnabled)
    {
        sdEmuDelay(this);
    }
    else
    {
        // Подключение к файловой системе
        if (!this->fatfsMounted)
        {
            if (!writer_mount(this))
            {
                atomic_fetch_add(&this->fsErrors, 1);
                return false;
            }
        }

        // Следующий номер файла
        unsigned fileNumber = atomic_load(&this->fileNumber) + 1;
        if (fileNumber >= 1000000) fileNumber = 0; // Без использования LFN, максимальный размер имени 8 символов.
        atomic_store(&this->fileNumber, fileNumber);

        // Имя файла
        char fileName[sizeof(fileNameTemplate) - 2 + 8]; // Минус %u, плюс 00000000
        int e = snprintf(fileName, sizeof(fileName), fileNameTemplate, fileNumber);
        if (e < 0 || e >= sizeof(fileName))
            Error_Handler(); // Функция не возвращается

        // Создаём файл
        FRESULT r = f_open(&this->file, fileName, FA_CREATE_ALWAYS | FA_WRITE);
        if (r != FR_OK)
        {
            atomic_store(&this->lastFsError, r);
            atomic_fetch_add(&this->fsErrors, 1);
            return false;
        }

        const uint32_t dataSize = this->maxAllocEnabled ? this->maxFileSize : 0;

        // Записываем заголовок
        WavPcmHeader h;
        makeWavPcmHeader(&h, /* freq*/ 1600, /* channnels, stereo */ 2, /* bits */ 16, dataSize);
        UINT wr = 0;
        r = f_write(&this->file, &h, sizeof(h), &wr);
        if (r != FR_OK || wr != sizeof(h))
        {
            atomic_store(&this->lastFsError, r);
            atomic_fetch_add(&this->fsErrors, 1);
            f_close(&this->file);
            return false;
        }

        // Сразу выделяем место, что бы процесс записи происходил быстрее
        if (dataSize > 0)
        {
            r = f_lseek(&this->file, h.fileSizeMinus8);
            if (r != FR_OK)
            {
                atomic_store(&this->lastFsError, r);
                atomic_fetch_add(&this->fsErrors, 1);
                f_close(&this->file);
                return false;
            }

            r = f_write(&this->file, &h, 8, &wr);
            if (r != FR_OK || wr != 8)
            {
                atomic_store(&this->lastFsError, r);
                atomic_fetch_add(&this->fsErrors, 1);
                f_close(&this->file);
                return false;
            }

            r = f_lseek(&this->file, sizeof(WavPcmHeader));
            if (r != FR_OK)
            {
                atomic_store(&this->lastFsError, r);
                atomic_fetch_add(&this->fsErrors, 1);
                f_close(&this->file);
                return false;
            }
        }

        this->realFileOpened = true;
    }

    atomic_store(&this->payloadBytesInFile, 0);
    this->fileOpened = true;
    return true;
}

static void writer_closeFile(Writer* this)
{
    if (this->realFileOpened)
    {
        // Тут будет конец файла
        FRESULT r = f_truncate(&this->file);
        if (r != FR_OK)
        {
            atomic_store(&this->lastFsError, r);
            atomic_fetch_add(&this->fsErrors, 1);
        }

        // Обновляем заголовок
        r = f_lseek(&this->file, 0);
        if (r != FR_OK)
        {
            atomic_store(&this->lastFsError, r);
            atomic_fetch_add(&this->fsErrors, 1);
        }
        else
        {
            WavPcmHeader h;
            makeWavPcmHeader(&h, /* freq*/ 1600, /* channnels, stereo */ 2, /* bits */ 16,
                             atomic_load(&this->payloadBytesInFile));

            UINT wr = 0;
            r = f_write(&this->file, &h, sizeof(h), &wr);
            if (r != FR_OK || wr != sizeof(h))
            {
                atomic_store(&this->lastFsError, r);
                atomic_fetch_add(&this->fsErrors, 1);
            }
        }

        r = f_close(&this->file);
        if (r != FR_OK)
        {
            atomic_store(&this->lastFsError, r);
            atomic_fetch_add(&this->fsErrors, 1);
        }

        this->realFileOpened = false;
    }
    else if (this->sdEmuEnabled)
    {
        sdEmuDelay(this);
    }

    this->fileOpened = false;
}

static void writer_writeFile(Writer* this, const void* data, size_t dataSize)
{
    atomic_fetch_add(&this->payloadBytesInFile, dataSize);
    if (this->realFileOpened)
    {
        unsigned wr = 0;
        FRESULT r = f_write(&this->file, data, dataSize, &wr);
        if (r != FR_OK || wr != dataSize)
        {
            atomic_store(&this->lastFsError, r);
            atomic_fetch_add(&this->fsErrors, 1);
        }
    }
    else if (this->sdEmuEnabled)
    {
        sdEmuDelay(this);
    }
}

static void writer__i2sSkip(Writer* this, size_t n)
{
    size_t t = 0;
    while (t < n)
    {
        size_t dataCount;
        cycleBuffer_get(&this->cycleBuffer, &dataCount);
        cycleBuffer_pop(&this->cycleBuffer, dataCount);
        t += dataCount;
    }
}

void writer_main(void *argument)
{
    Writer* this = (Writer*)argument;

    writer_mount(this);

    //! Первый номер файла

    for (;;)
    {
        // Задача остановлена
        if (!this->enabled)
        {
            ledSet(ledBlue);
            vTaskSuspend(NULL);
            continue;
        }

        // Создание нового файла
        if (!this->fileOpened)
        {
            if (!writer_createFile(this))
            {
                ledSet(ledRed | ledGreen);
                osDelay(pdMS_TO_TICKS(500));
                continue;
            }
        }

        // Запускаем буфер
        cycleBuffer_create(&this->cycleBuffer, this->cycleBufferSpace, sizeof(this->cycleBufferSpace));

        // Включаем получение данных с микрофона
        i2sReader_create(&this->i2sReader, i2sPort);        

        // Пропускаем первую секунду. Микрофон в это время приходит в себя и выдает "свип" сигнал.
        writer__i2sSkip(this, 16000 * 2 * 2);

        while (this->enabled)
        {
            // Создание нового файла
            if (!this->fileOpened)
            {
                if (!writer_createFile(this))
                {
                    ledSet(ledRed | ledGreen);
                    osDelay(pdMS_TO_TICKS(500));
                    continue;
                }
            }

            // Индикация
            this->blink++;
            ledSet((this->blink & 1) == 0 ? ledRed : ledGreen);

            // Получение данных
            size_t dataCount;
            void* data = cycleBuffer_get(&this->cycleBuffer, &dataCount);

            // Запись данных в файл
            writer_writeFile(this, data, dataCount);

            // Удаляем данные из буфера
            cycleBuffer_pop(&this->cycleBuffer, dataCount);

            // Закрываем файл, если он слишком большой
            if (atomic_load(&this->payloadBytesInFile) + sizeof(WavPcmHeader) >= this->maxFileSize)
            {
                writer_closeFile(this);
            }
        }

        // Выключаем получение данных
        i2sReader_destroy(&this->i2sReader);

        // Останавлиаем буфер
        cycleBuffer_destroy(&this->cycleBuffer);

        // Закрываем файл
        writer_closeFile(this);
    }
}

static void writer_i2sDataReadyMt(void* this_void, void* data, size_t dataSize)
{
    Writer* this = (Writer*)this_void;

    // Эмуляция микрофона. Мы не выключаем получение данных по i2s, что бы правильно эмулировать темп приёма.
    // Заполняем файл равномерно возрастающими значениями, так проще увидеть пропуски.
    if (this->micEmuEnabled)
    {
        int16_t *i, *ie;
        unsigned e = this->micEmuCounter;
        for (i = (int16_t*)data, ie = i + dataSize / 2; i != ie; i++)
            *i = (int16_t)++e;
        this->micEmuCounter = e;
    }

    cycleBuffer_putFromInterrupt(&this->cycleBuffer, data, dataSize);
}

void writer_setEnabledMt(void* this_void, bool enabled)
{
    Writer* this = (Writer*)this_void;
    this->enabled = enabled;
    if (enabled) vTaskResume(this->taskHandle);
}

void writer_setSdEmuEnabledMt(void* this_void, bool enabled)
{
    Writer* this = (Writer*)this_void;
    this->sdEmuEnabled = enabled;
}

void writer_setMicEmuEnabledMt(void* this_void, bool enabled)
{
    Writer* this = (Writer*)this_void;
    this->micEmuEnabled = enabled;
}

void writer_setMaxAllocEnabledMt(void* this_void, bool enabled)
{
    Writer* this = (Writer*)this_void;
    this->maxAllocEnabled = enabled;
}

void writer_getInfoMt(void* this_void, WriterInfo* out)
{
    Writer* this = (Writer*)this_void;
    out->enabled            = this->enabled;
    out->fileNumber         = atomic_load(&this->fileNumber);
    out->lastFsError        = atomic_load(&this->lastFsError);
    out->fsErrors           = atomic_load(&this->fsErrors);
    out->lostBytes          = cycleBuffer_getLostBytesMt(&this->cycleBuffer);
    out->payloadBytesInFile = atomic_load(&this->payloadBytesInFile);
    out->fileOpened         = this->fileOpened;
    out->realFileOpened     = this->realFileOpened;
    out->sdEmuEnabled       = this->sdEmuEnabled;
    out->micEmuEnabled      = this->micEmuEnabled;
    out->maxAllocEnabled    = this->maxAllocEnabled;
    out->maxFileSize        = atomic_load(&this->maxFileSize);
    out->sdEmuMinDelayMs    = atomic_load(&this->sdEmuMinDelayMs);
    out->sdEmuMaxDelayMs    = atomic_load(&this->sdEmuMaxDelayMs);
}

void writer_create(Writer* this)
{
    // При открытии файла создавать файл максимального размера. Это снизит задержки при записи, так как
    // не потребуется модифицировать FAT
    this->maxAllocEnabled = false;

    this->sdEmuEnabled = true;
    this->micEmuEnabled = true;
    atomic_store(&this->maxFileSize, 16 * 1024 * 1024);
    atomic_store(&this->sdEmuMinDelayMs, 10);
    atomic_store(&this->sdEmuMaxDelayMs, 375);

    // Переменные по умолчанию
    this->fatfsMounted   = false;
    this->micEmuCounter  = 0;
    this->enabled        = false;
    this->fileOpened     = false;
    this->realFileOpened = false;
    atomic_store(&this->fsErrors, 0);
    atomic_store(&this->payloadBytesInFile, 0);
    atomic_store(&this->lastFsError, FR_OK);

    // Выход объекта i2sReader
    this->i2sReader.interface.dataReadyArg = this;
    this->i2sReader.interface.dataReady = writer_i2sDataReadyMt;

    // Запуск потока
    this->taskAttributes.name       = "writerTask";
    this->taskAttributes.stack_mem  = &this->taskBuffer[0];
    this->taskAttributes.stack_size = sizeof(this->taskBuffer);
    this->taskAttributes.cb_mem     = &this->taskControlBlock;
    this->taskAttributes.cb_size    = sizeof(this->taskControlBlock);
    this->taskAttributes.priority   = (osPriority_t)osPriorityHigh;
    this->taskHandle = osThreadNew(writer_main, (void*)this, &this->taskAttributes);
}

void writer_setSdEmuMaxDelayMs(void* this_void, unsigned value)
{
    Writer* this = (Writer*)this_void;
    atomic_store(&this->sdEmuMaxDelayMs, value);
}

void writer_setSdEmuMinDelayMs(void* this_void, unsigned value)
{
    Writer* this = (Writer*)this_void;
    atomic_store(&this->sdEmuMinDelayMs, value);
}

void writer_setMaxFileSize(void* this_void, unsigned value)
{
    Writer* this = (Writer*)this_void;
    atomic_store(&this->maxFileSize, value);
}
