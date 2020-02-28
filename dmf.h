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
#include <stdbool.h>

// For inflating .dmf files so that they can be read 
#include "zlib.h"
#include "zconf.h"

// Deflemask allows four effects columns per channel regardless of the system 
#define MAX_EFFECTS_COLUMN_COUNT 4 

#ifndef CMD_Options 
    typedef struct CMD_Options {
        bool useEffects; 
    } CMD_Options;
    #define CMD_Options CMD_Options
#endif

typedef struct System
{
    uint8_t id;
    char *name;
    uint8_t channels;
} System;

// SYSTEM_TYPE values also correspond to indices in Systems array. 
typedef enum SYSTEM_TYPE 
{
    SYS_ERROR, SYS_GENESIS, SYS_GENESIS_CH3, SYS_SMS, SYS_GAMEBOY, 
    SYS_PCENGINE, SYS_NES, SYS_C64_SID_8580, SYS_C64_SID_6581, SYS_YM2151
} SYSTEM_TYPE; 

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

typedef enum DMF_NOTE {
    DMF_NOTE_EMPTY=0, 
    DMF_NOTE_CS=1, 
    DMF_NOTE_D=2, 
    DMF_NOTE_DS=3, 
    DMF_NOTE_E=4, 
    DMF_NOTE_F=5, 
    DMF_NOTE_FS=6, 
    DMF_NOTE_G=7, 
    DMF_NOTE_GS=8, 
    DMF_NOTE_A=9, 
    DMF_NOTE_AS=10, 
    DMF_NOTE_B=11, 
    DMF_NOTE_C=12, 
    DMF_NOTE_OFF=100, 
    
    DMF_NOTE_NOINSTRUMENT=-1,
    DMF_NOTE_NOVOLUME=-1,
    DMF_NOTE_VOLUMEMAX=15 /* ??? */
} DMF_NOTE;

// Deflemask effects shared by all systems: 
typedef enum DMF_EFFECT {
    DMF_NOEFFECT=-1, DMF_NOEFFECTVAL=-1,
    DMF_ARP=0x0, DMF_PORTUP=0x1, DMF_PORTDOWN=0x2, DMF_PORT2NOTE=0x3, DMF_VIBRATO=0x4, DMF_PORT2NOTEVOLSLIDE=0x5, DMF_VIBRATOVOLSLIDE=0x6,
    DMF_TREMOLO=0x7, DMF_PANNING=0x8, DMF_SETSPEEDVAL1=0x9, DMF_VOLSLIDE=0xA, DMF_POSJUMP=0xB, DMF_RETRIG=0xC, DMF_PATBREAK=0xD, 
    DMF_ARPTICKSPEED=0xE0, DMF_NOTESLIDEUP=0xE1, DMF_NOTESLIDEDOWN=0xE2, DMF_SETVIBRATOMODE=0xE3, DMF_SETFINEVIBRATODEPTH=0xE4, 
    DMF_SETFINETUNE=0xE5, DMF_SETSAMPLESBANK=0xEB, DMF_NOTECUT=0xEC, DMF_NOTEDELAY=0xED, DMF_SYNCSIGNAL=0xEE, DMF_SETGLOBALFINETUNE=0xEF, 
    DMF_SETSPEEDVAL2=0xF
} DMF_EFFECT; 

// Deflemask effects exclusive to the GameBoy system:
typedef enum DMF_GAMEBOY_EFFECT {
    DMF_SETWAVE=0x10, DMF_SETNOISEPOLYCOUNTERMODE=0x11, DMF_SETDUTYCYCLE=0x12, DMF_SETSWEEPTIMESHIFT=0x13, DMF_SETSWEEPDIR=0x14
} DMF_GAMEBOY_EFFECT;

// To do: Add enums for effects exclusive to the rest of Deflemask's systems. 

// Imports the .dmf file "fname" and stores it in the struct "dmf" using the options "opt" 
int importDMF(const char *fname, DMFContents *dmf, CMD_Options opt); 

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