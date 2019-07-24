/*
dmf.h
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Provides functions for loading a .dmf file according to the 
spec sheet at http://www.deflemask.com/DMF_SPECS.txt. 

Requires zlib1.dll from the zlib compression library at https://zlib.net. 
*/

#ifndef __DMF_H__
#define __DMF_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

// For inflating .dmf files so that they can be read 
#include "zlib.h"

// Deflemask allows four effects columns per channel regardless of the system 
#define MAX_EFFECTS_COLUMN_COUNT 4 

typedef struct System
{
    uint8_t id;
    char *name;
    uint8_t channels;
} System;

typedef struct VisualInfo
{
    uint8_t songNameLength; 
    char *songName; 
    uint8_t songAuthorLength;
    char *songAuthor; 
    uint8_t highlightAPatterns;
    uint8_t highlightBPatterns;
} VisualInfo; 

typedef struct ModuleInfo
{
    uint8_t timeBase, tickTime1, tickTime2, framesMode, usingCustomHZ, customHZValue1, customHZValue2, customHZValue3;    
    uint32_t totalRowsPerPattern; 
    uint8_t totalRowsInPatternMatrix; 
} ModuleInfo; 

typedef struct Instrument
{
    char *name;
    uint8_t mode;

    // FM Instruments 
    uint8_t fmALG, fmFB, fmLFO, fmLFO2;
    uint8_t fmAM, fmAR, fmDR, fmMULT, fmRR, fmSL, fmTL, fmDT2, fmRS, fmDT, fmD2R, fmSSGMODE; 

    // Standard Instruments
    uint8_t stdVolEnvSize, stdArpEnvSize, stdDutyNoiseEnvSize, stdWavetableEnvSize;
    int32_t *stdVolEnvValue, *stdArpEnvValue, *stdDutyNoiseEnvValue, *stdWavetableEnvValue; 
    int8_t stdVolEnvLoopPos, stdArpEnvLoopPos, stdDutyNoiseEnvLoopPos, stdWavetableEnvLoopPos; 
    uint8_t stdArpMacroMode; 

    // Standard Instruments - Commodore 64 exclusive 
    uint8_t stdC64TriWaveEn, stdC64SawWaveEn, stdC64PulseWaveEn, stdC64NoiseWaveEn, 
        stdC64Attack, stdC64Decay, stdC64Sustain, stdC64Release, stdC64PulseWidth, stdC64RingModEn,
        stdC64SyncModEn, stdC64ToFilter, stdC64VolMacroToFilterCutoffEn, stdC64UseFilterValuesFromInst; 
    uint8_t stdC64FilterResonance, stdC64FilterCutoff, stdC64FilterHighPass, stdC64FilterLowPass, stdC64FilterCH2Off; 

    // Standard Instruments - GameBoy exclusive 
    uint8_t stdGBEnvVol, stdGBEnvDir, stdGBEnvLen, stdGBSoundLen;  

} Instrument;

typedef struct PCMSample 
{
    uint32_t size; 
    char *name; 
    uint8_t rate, pitch, amp, bits;
    uint16_t *data; 
} PCMSample; 

typedef struct PatternRow
{
    uint16_t note; 
    uint16_t octave; 
    int16_t volume; 
    int16_t effectCode[MAX_EFFECTS_COLUMN_COUNT];
    int16_t effectValue[MAX_EFFECTS_COLUMN_COUNT];
    int16_t instrument;
} PatternRow; 

typedef struct DMFContents
{
    uint8_t dmfFileVersion; 
    System sys; 
    VisualInfo visualInfo; 
    ModuleInfo moduleInfo; 
    uint8_t **patternMatrixValues; 
    uint8_t *patternMatrixMaxValues; 
    uint8_t totalInstruments; 
    Instrument* instruments; 
    uint8_t totalWavetables; 
    uint32_t *wavetableSizes;   
    uint32_t **wavetableValues; 
    PatternRow ***patternValues; 
    uint8_t *channelEffectsColumnsCount; 
    uint8_t totalPCMSamples; 
    PCMSample *pcmSamples; 
} DMFContents; 

typedef enum NOTE {
    EMPTY=0, 
    CS=1, 
    D=2, 
    DS=3, 
    E=4, 
    F=5, 
    FS=6, 
    G=7, 
    GS=8, 
    A=9, 
    AS=10, 
    B=11, 
    C=12, 
    OFF=100
} NOTE;

// Deflemask effects shared by all systems: 
typedef enum EFFECT {
    ARP=0x0, PORTUP=0x1, PORTDOWN=0x2, PORT2NOTE=0x3, VIBRATO=0x4, PORT2NOTEVOLSLIDE=0x5, VIBRATOVOLSLIDE=0x6,
    TREMOLO=0x7, PANNING=0x8, SETSPEEDVAL1=0x9, VOLSLIDE=0xA, POSJUMP=0xB, RETRIG=0xC, PATBREAK=0xD, 
    ARPTICKSPEED=0xE0, NOTESLIDEUP=0xE1, NOTESLIDEDOWN=0xE2, SETVIBRATOMODE=0xE3, SETFINEVIBRATODEPTH=0xE4, 
    SETFINETUNE=0xE5, SETSAMPLESBANK=0xEB, NOTECUT=0xEC, NOTEDELAY=0xED, SYNCSIGNAL=0xEE, SETGLOBALFINETUNE=0xEF, 
    SETSPEEDVAL2=0xF
} EFFECT; 

// Deflemask effects exclusive to the GameBoy system:
typedef enum GAMEBOY_EFFECT {
    SETWAVE=0x10, SETNOISEPOLYCOUNTERMODE=0x11, SETDUTYCYCLE=0x12, SETSWEEPTIMESHIFT=0x13, SETSWEEPDIR=0x14
} GAMEBOY_EFFECT;

// To do: Add enums for effects exclusive to the rest of Deflemask's systems. 

// Imports the .dmf file "fname" and stores it in the struct "dmf" 
int importDMF(const char *fname, DMFContents *dmf); 

System getSystem(uint8_t systemByte);
void loadVisualInfo(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
void loadModuleInfo(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
void loadPatternMatrixValues(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
void loadInstrumentsData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf);
Instrument loadInstrument(uint8_t **fBuff, uint32_t *pos, System systemType); 
void loadWavetablesData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
void loadPatternsData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
PatternRow loadPatternRow(uint8_t **fBuff, uint32_t *pos, int effectsColumnsCount); 
void loadPCMSamplesData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
PCMSample loadPCMSample(uint8_t **fBuff, uint32_t *pos); 

// Frees the dynamically allocated memory used by a DMFContents struct 
void freeDMF(DMFContents *dmf); 

const char *getFilenameExt(const char *fname); 

#endif 