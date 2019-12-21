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

#include "dmf.h"

// ProTracker effects: 
typedef enum PT_EFFECT {
    PT_NOEFFECT=0x0, PT_NOEFFECTVAL=0x0,
    PT_ARP=0x0, PT_PORTUP=0x1, PT_PORTDOWN=0x2, PT_PORT2NOTE=0x3, PT_VIBRATO=0x4, PT_PORT2NOTEVOLSLIDE=0x5, PT_VIBRATOVOLSLIDE=0x6,
    PT_TREMOLO=0x7, PT_PANNING=0x8, PT_SETSAMPLEOFFSET=0x9, PT_VOLSLIDE=0xA, PT_POSJUMP=0xB, PT_SETVOLUME=0xC, PT_PATBREAK=0xD, 
    PT_SETFILTER=0xE0, PT_FINESLIDEUP=0xE1, PT_FINESLIDEDOWN=0xE2, PT_SETGLISSANDO=0xE3, PT_SETVIBRATOWAVEFORM=0xE4, 
    PT_SETFINETUNE=0xE5, PT_LOOPPATTERN=0xE6, PT_SETTREMOLOWAVEFORM=0xE7, PT_RETRIGGERSAMPLE=0xE9, PT_FINEVOLSLIDEUP=0xEA, 
    PT_FINEVOLSLIDEDOWN=0xEB, PT_CUTSAMPLE=0xEC, PT_DELAYSAMPLE=0xED, PT_DELAYPATTERN=0xEE, PT_INVERTLOOP=0xEF,
    PT_SETSPEED=0xF
} PT_EFFECT; 

// Exports a DMFContents struct "dmf" to a .mod file "fname" 
int exportMOD(char *fname, DMFContents *dmf); 

int8_t getProTrackerRepeatPatterns(DMFContents *dmf);  

int writeProTrackerPatternRow(FILE *fout, PatternRow *pat, uint8_t dutyCycle); 
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


