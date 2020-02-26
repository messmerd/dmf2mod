/*
mod.h
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Provides functions for exporting the contents of a .dmf file 
to ProTracker's .mod format. 

Several limitations apply in order to export. For example, the 
.dmf file must use the GameBoy system, patterns must have 64 
rows, only one effect column is allowed per channel, etc.  
*/

#ifndef __MOD_H__
#define __MOD_H__ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>

#include "dmf.h"

#ifndef CMD_Options 
    typedef struct CMD_Options {
        bool useEffects; 
    } CMD_Options;
    #define CMD_Options CMD_Options
#endif

// ProTracker effects
// An effect is represented with 12 bits, which is 3 groups of 4 bits: [e][x][y]. 
// The effect code is [e] or [e][x], and the effect value is [x][y] or [y]. 
// Effect codes of the form [e] are stored as [e][0x0] below 
typedef enum PT_EFFECT {
    PT_NOEFFECT=0x00, PT_NOEFFECTVAL=0x00, PT_NOEFFECT_CODE=0x00, /* PT_NOEFFECT_CODE is the same as ((uint16_t)PT_NOEFFECT << 4) | PT_NOEFFECTVAL */
    PT_ARP=0x00, PT_PORTUP=0x10, PT_PORTDOWN=0x20, PT_PORT2NOTE=0x30, PT_VIBRATO=0x40, PT_PORT2NOTEVOLSLIDE=0x50, PT_VIBRATOVOLSLIDE=0x60,
    PT_TREMOLO=0x70, PT_PANNING=0x80, PT_SETSAMPLEOFFSET=0x90, PT_VOLSLIDE=0xA0, PT_POSJUMP=0xB0, PT_SETVOLUME=0xC0, PT_PATBREAK=0xD0, 
    PT_SETFILTER=0xE0, PT_FINESLIDEUP=0xE1, PT_FINESLIDEDOWN=0xE2, PT_SETGLISSANDO=0xE3, PT_SETVIBRATOWAVEFORM=0xE4, 
    PT_SETFINETUNE=0xE5, PT_LOOPPATTERN=0xE6, PT_SETTREMOLOWAVEFORM=0xE7, PT_RETRIGGERSAMPLE=0xE9, PT_FINEVOLSLIDEUP=0xEA, 
    PT_FINEVOLSLIDEDOWN=0xEB, PT_CUTSAMPLE=0xEC, PT_DELAYSAMPLE=0xED, PT_DELAYPATTERN=0xEE, PT_INVERTLOOP=0xEF,
    PT_SETSPEED=0xF0
} PT_EFFECT; 

// The current square wave duty cycle, note volume, and other information that the 
//      tracker stores for each channel while playing a tracker file.
typedef struct MODChannelState
{
    uint8_t dutyCycle; 
    int16_t volume;
    uint8_t wavetable;
} MODChannelState; 

#define PT_NOTE_VOLUMEMAX 64

// Exports a DMFContents struct "dmf" to a .mod file "fname" using the options "opt" 
int exportMOD(char *fname, DMFContents *dmf, CMD_Options opt); 

int8_t getProTrackerRepeatPatterns(DMFContents *dmf);  

int writeProTrackerPatternRow(FILE *fout, PatternRow *pat, MODChannelState *state, CMD_Options opt); 
uint16_t getProTrackerEffect(int16_t effectCode, int16_t effectValue);

// Deflemask/ProTracker pattern matrix row number to ProTracker pattern index 
int8_t *patternMatrixRowToProTrackerPattern;

// ProTracker pattern index to Deflemask/ProTracker pattern matrix row number. 
// If a pattern is used more than once, the first pattern matrix row number where it appears is used 
int8_t *proTrackerPatternToPatternMatrixRow;

const uint16_t sqwSampleLength;
const int8_t sqwSampleDuty[4][32];
const char sqwSampleNames[4][22];

uint16_t proTrackerPeriodTable[5][12]; 

#endif 


