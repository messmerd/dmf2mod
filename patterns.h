
#ifndef __PATTERNS_H__
#define __PATTERNS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_EFFECTS_COLUMN_COUNT 4 

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

typedef struct PatternRow
{
    uint16_t note; 
    uint16_t octave; 
    int16_t volume; 
    int16_t effectCode[MAX_EFFECTS_COLUMN_COUNT];
    int16_t effectValue[MAX_EFFECTS_COLUMN_COUNT];
    int16_t instrument;
} PatternRow; 

PatternRow loadPatternRow(FILE *filePointer, int effectsColumnsCount); 

int8_t getProTrackerRepeatPatterns(uint8_t **patMatVal, int totalRows); 

void writeProTrackerPatternRow(FILE *filePointer, PatternRow *pat, uint8_t dutyCycle); 

// Deflemask/ProTracker pattern matrix row number to ProTracker pattern index 
int8_t *patternMatrixRowToProTrackerPattern;

// ProTracker pattern index to Deflemask/ProTracker pattern matrix row number. 
// If a pattern is used more than once, the first pattern matrix row number where it appears is used 
int8_t *proTrackerPatternToPatternMatrixRow;

uint16_t proTrackerPeriodTable[5][12]; 

#endif 