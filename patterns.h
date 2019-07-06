
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
    BS=12, 
    OFF=100
    } NOTE;

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

int8_t *proTrackerToDeflemaskIndices, *deflemaskToProTrackerIndices;

#endif 