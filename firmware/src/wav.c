#include "wav.h"

void makeWavPcmHeader(WavPcmHeader* h, uint32_t sampleRate, uint16_t numChannels, 
                      uint16_t bitsPerSample, uint32_t dataSize)
{
    h->id1            = wavHeaderId1;
    h->id2            = wavHeaderId2;
    h->id3            = wavHeaderId3;
    h->id4            = wavHeaderId4;
    h->id5            = wavHeaderId5;
    h->numChannels    = numChannels;
    h->sampleRate     = sampleRate;
    h->blockAlign     = numChannels * ((bitsPerSample + 7) / 8);
    h->byteRate       = sampleRate * h->blockAlign;
    h->bitsPerSample  = bitsPerSample;
    h->id6            = wavHeaderId6;
    h->dataSize       = dataSize;
    h->fileSizeMinus8 = dataSize + sizeof(WavPcmHeader) - 8;
}

