#pragma once

#include <stdint.h>

#if __BYTE_ORDER != __LITTLE_ENDIAN
#error Этот файл написан под LITTLE_ENDIAN
#endif

const static uint32_t wavHeaderId1 = 0x46464952;
const static uint32_t wavHeaderId2 = 0x45564157;
const static uint32_t wavHeaderId3 = 0x20746d66;
const static uint32_t wavHeaderId4 = 16;
const static uint16_t wavHeaderId5 = 1;
const static uint32_t wavHeaderId6 = 0x61746164;

//! Тут можно в макрос завернуть
//! const static uint32_t wavHeaderId1 = LETOH(0x46464952);

#pragma pack(push, 1)

typedef struct WavPcmHeader
{
    uint32_t id1;            // = wavHeaderId1
    uint32_t fileSizeMinus8; // Размезмер файла минус 8
    uint32_t id2;            // = wavHeaderId2
    uint32_t id3;            // = wavHeaderId3
    uint32_t id4;            // = wavHeaderId4
    uint16_t id5;            // = wavHeaderId5
    uint16_t numChannels;    // Кол-во каналов. Моно = 1, Стерео = 2 и т.д.
    uint32_t sampleRate;     // Частота дискретизации. 8000 Гц, 44100 Гц и т.д.
    uint32_t byteRate;       // Количество байт, переданных за секунду воспроизведения.
    uint16_t blockAlign;     // Количество байт для одного сэмпла, включая все каналы.
    uint16_t bitsPerSample;  // Количество бит в сэмпле. 8 бит, 16 бит и т.д.
    uint32_t id6;            // = wavHeaderId6
    uint32_t dataSize;       // Количество байт в области данных.
} WavPcmHeader;

#pragma pack(pop)

void makeWavPcmHeader(WavPcmHeader* h, uint32_t sampleRate, uint16_t numChannels,
                             uint16_t bitsPerSample, uint32_t dataSize);

