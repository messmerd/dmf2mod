
#ifndef __INSTRUMENTS_H__
#define __INSTRUMENTS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "system_info.h"

typedef struct Instrument
{
    char *name;
    unsigned char mode;

    // FM Instruments 
    unsigned char fmALG, fmFB, fmLFO, fmLFO2;
    unsigned char fmAM, fmAR, fmDR, fmMULT, fmRR, fmSL, fmTL, fmDT2, fmRS, fmDT, fmD2R, fmSSGMODE; 

    // Standard Instruments
    unsigned char stdVolEnvSize, stdArpEnvSize, stdDutyNoiseEnvSize, stdWavetableEnvSize;
    int32_t *stdVolEnvValue, *stdArpEnvValue, *stdDutyNoiseEnvValue, *stdWavetableEnvValue; 
    char stdVolEnvLoopPos, stdArpEnvLoopPos, stdDutyNoiseEnvLoopPos, stdWavetableEnvLoopPos; 
    unsigned char stdArpMacroMode; 

    unsigned char stdC64TriWaveEn, stdC64SawWaveEn, stdC64PulseWaveEn, stdC64NoiseWaveEn, 
        stdC64Attack, stdC64Decay, stdC64Sustain, stdC64Release, stdC64PulseWidth, stdC64RingModEn,
        stdC64SyncModEn, stdC64ToFilter, stdC64VolMacroToFilterCutoffEn, stdC64UseFilterValuesFromInst; 
    unsigned char stdC64FilterResonance, stdC64FilterCutoff, stdC64FilterHighPass, stdC64FilterLowPass, stdC64FilterCH2Off; 

    unsigned char stdGBEnvVol, stdGBEnvDir, stdGBEnvLen, stdGBSoundLen;  

} Instrument;

typedef struct PCMSample 
{
    uint32_t size; 
    char *name; 
    uint8_t rate, pitch, amp, bits;
    uint16_t *data; 
} PCMSample; 

const int16_t sqwSampleLength;
const int8_t sqwSampleDuty12_5[32];
const int8_t sqwSampleDuty25[32];
const int8_t sqwSampleDuty50[32];
const int8_t sqwSampleDuty75[32];
const char sqwSampleNames[4][22];


Instrument loadInstrument(FILE *filePointer, System systemType);
PCMSample loadPCMSample(FILE *filePointer); 

#endif 