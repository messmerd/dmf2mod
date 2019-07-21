
#ifndef __DMF_H__
#define __DMF_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_EFFECTS_COLUMN_COUNT 4 

typedef struct System
{
    unsigned char id;
    char *name;
    unsigned char channels;
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

// Deflemask effects found across all systems: 
typedef enum EFFECT {
    ARP=0x0, PORTUP=0x1, PORTDOWN=0x2, PORT2NOTE=0x3, VIBRATO=0x4, PORT2NOTEVOLSLIDE=0x5, VIBRATOVOLSLIDE=0x6,
    TREMOLO=0x7, PANNING=0x8, SETSPEEDVAL1=0x9, VOLSLIDE=0xA, POSJUMP=0xB, RETRIG=0xC, PATBREAK=0xD, 
    ARPTICKSPEED=0xE0, NOTESLIDEUP=0xE1, NOTESLIDEDOWN=0xE2, SETVIBRATOMODE=0xE3, SETFINEVIBRATODEPTH=0xE4, 
    SETFINETUNE=0xE5, SETSAMPLESBANK=0xEB, NOTECUT=0xEC, NOTEDELAY=0xED, SYNCSIGNAL=0xEE, SETGLOBALFINETUNE=0xEF, 
    SETSPEEDVAL2=0xF
} EFFECT; 

typedef enum GAMEBOY_EFFECT {
    SETWAVE=0x10, SETNOISEPOLYCOUNTERMODE=0x11, SETDUTYCYCLE=0x12, SETSWEEPTIMESHIFT=0x13, SETSWEEPDIR=0x14
} GAMEBOY_EFFECT;

void importDMF(char *fname, DMFContents *dmf); 
void loadVisualInfo(FILE *fin, DMFContents *dmf); 
void loadModuleInfo(FILE *fin, DMFContents *dmf); 
void loadPatternMatrixValues(FILE *fin, DMFContents *dmf); 
void loadInstrumentsData(FILE *fin, DMFContents *dmf); 
void loadWavetablesData(FILE *fin, DMFContents *dmf); 
void loadPatternsData(FILE *fin, DMFContents *dmf); 
void loadPCMSamplesData(FILE *fin, DMFContents *dmf); 

void freeDMF(DMFContents *dmf); 

System getSystem(uint8_t systemByte);

PatternRow loadPatternRow(FILE *filePointer, int effectsColumnsCount); 

Instrument loadInstrument(FILE *filePointer, System systemType);
PCMSample loadPCMSample(FILE *filePointer); 


#endif 