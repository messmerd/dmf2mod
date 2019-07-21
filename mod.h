#ifndef __MOD_H__
#define __MOD_H__ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "dmf.h"

void exportMOD(char *fname, DMFContents *dmf); 

int8_t getProTrackerRepeatPatterns(DMFContents *dmf);  

void writeProTrackerPatternRow(FILE *fout, PatternRow *pat, uint8_t dutyCycle); 

// Deflemask/ProTracker pattern matrix row number to ProTracker pattern index 
int8_t *patternMatrixRowToProTrackerPattern;

// ProTracker pattern index to Deflemask/ProTracker pattern matrix row number. 
// If a pattern is used more than once, the first pattern matrix row number where it appears is used 
int8_t *proTrackerPatternToPatternMatrixRow;

const int16_t sqwSampleLength;
const int8_t sqwSampleDuty[4][32];
const char sqwSampleNames[4][22];

uint16_t proTrackerPeriodTable[5][12]; 

#endif 


