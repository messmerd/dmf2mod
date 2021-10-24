/*
mod.h
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Provides functions for exporting the contents of a .dmf file 
to ProTracker's .mod format. 

Several limitations apply in order to export. For example, the 
.dmf file must use the Game Boy system, patterns must have 64 
rows, only one effect column is allowed per channel, etc.  
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>

#include "dmf.h"

typedef struct CMD_Options {
    uint8_t effects; // 0 == none; 1 == minimum; 2 == maximum 
    bool allowDownsampling; 
} CMD_Options;

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

// Error codes 
typedef enum MOD_ERROR {
    MOD_ERROR_NONE=0, MOD_ERROR_NOT_GAMEBOY, MOD_ERROR_TOO_MANY_PAT_MAT_ROWS, MOD_ERROR_NOT_64_ROW_PATTERN, 
    MOD_ERROR_WAVE_DOWNSAMPLE, MOD_ERROR_EFFECT_VOLUME, MOD_ERROR_MULTIPLE_EFFECT 
} MOD_ERROR; 

// Error information used by multiple functions 
typedef struct MODError {
    MOD_ERROR errorCode;
    char *errorInfo; 
} MODError; 

// Warning codes 
typedef enum MOD_WARNING { 
    MOD_WARNING_NONE=0, MOD_WARNING_PITCH_HIGH=1, MOD_WARNING_TEMPO_LOW=2, 
    MOD_WARNING_TEMPO_HIGH=4, MOD_WARNING_EFFECT_IGNORED=8
} MOD_WARNING; 

// Warning information used by multiple functions 
typedef struct MODWarning {
    uint16_t warningCode, multipleWarnings; 
} MODWarning; 

typedef struct MODConversionStatus {
    MODError error; 
    MODWarning warnings; 
} MODConversionStatus; 

class MOD : public Module, public ModuleStatic<MOD>
{
public:
    MOD() {};
    ~MOD() {};
    void CleanUp() {};

    bool Load(const char* filename) override
    {
        return false;
    }

    bool Save(const char* filename) override
    {
        return false;
    }

    ModuleType GetType() override { return _Type; }

    std::string GetFileExtension() override { return _FileExtension; }

    std::string GetName() override { return ""; }
};

// Exports a DMF object "dmf" to a MOD file "fname" using the options "options"
MODConversionStatus exportMOD(char *fname, DMF *dmfObj, CMD_Options options);

void cleanUp();

void printError();
void printWarnings();

