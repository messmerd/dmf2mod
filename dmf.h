/*
    dmf.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares the Module-derived class for Deflemask's DMF files.

    DMF file support was written according to the specs at 
    http://www.deflemask.com/DMF_SPECS.txt.

    Requires the zlib compression library from https://zlib.net.
*/

#pragma once

#include "modules.h"
#include "zstr/zstr.hpp"

// Deflemask allows four effects columns per channel regardless of the system 
#define MAX_EFFECTS_COLUMN_COUNT 4 

typedef enum DMF_NOTE {
    DMF_NOTE_EMPTY=101, 
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

// Deflemask effects exclusive to the Game Boy system:
typedef enum DMF_GAMEBOY_EFFECT {
    DMF_SETWAVE=0x10, 
    DMF_SETNOISEPOLYCOUNTERMODE=0x11, 
    DMF_SETDUTYCYCLE=0x12, 
    DMF_SETSWEEPTIMESHIFT=0x13, 
    DMF_SETSWEEPDIR=0x14
} DMF_GAMEBOY_EFFECT;

// To do: Add enums for effects exclusive to the rest of Deflemask's systems. 

typedef struct Note
{
    uint16_t pitch;
    uint16_t octave;
} Note;

typedef struct System
{
    uint8_t id;
    const char* name;
    uint8_t channels;
} System;

// SYSTEM_TYPE values also correspond to indices in DMF::m_Systems array.
typedef enum SYSTEM_TYPE
{
    SYS_ERROR=0, SYS_GENESIS, SYS_GENESIS_CH3, SYS_SMS, SYS_GAMEBOY, 
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

    // Standard Instruments - Game Boy exclusive 
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
    Note note;
    int16_t volume;
    int16_t effectCode[MAX_EFFECTS_COLUMN_COUNT];
    int16_t effectValue[MAX_EFFECTS_COLUMN_COUNT];
    int16_t instrument;
} PatternRow; 

typedef enum DMF_IMPORT_ERROR
{
    IMPORT_ERROR_SUCCESS=0,
    IMPORT_ERROR_FAIL=1
} DMF_IMPORT_ERROR;

// Begin setup
REGISTER_MODULE_BEGIN(DMF, DMFConversionOptions)

class DMF : public ModuleInterface<DMF>
{
public:
    static const System SYSTEMS(SYSTEM_TYPE systemType) { return m_Systems[systemType]; }

    enum ImportError
    {
        Success=0,
        UnspecifiedError
    };

    enum class ImportWarning {};
    enum class ExportError {};
    enum class ExportWarning {};
    enum class ConvertError {};
    enum class ConvertWarning {};

    DMF();
    ~DMF();
    void CleanUp();

    bool Import(const std::string& filename) override;
    bool Export(const std::string& filename) override;

    std::string GetName() const override
    {
        if (!m_VisualInfo.songName)
            return "";
        return m_VisualInfo.songName;
    }

    ////////////

    // Returns the initial BPM of the module when given ModuleInfo
    double GetBPM() const;

    const System& GetSystem() const { return m_System; }
    const VisualInfo& GetVisualInfo() const { return m_VisualInfo; }
    const ModuleInfo& GetModuleInfo() const { return m_ModuleInfo; }

    uint8_t** GetPatternMatrixValues() const { return m_PatternMatrixValues; }

    uint8_t GetTotalWavetables() const { return m_TotalWavetables; }

    uint32_t** GetWavetableValues() const { return m_WavetableValues; }
    uint32_t GetWavetableValue(unsigned wavetable, unsigned index) const { return m_WavetableValues[wavetable][index]; }

    PatternRow*** GetPatternValues() const { return m_PatternValues; }

private:
    bool ConvertFrom(const Module* input, ConversionOptionsPtr& options) override
    {
        return true; // Not implemented
    };

    System GetSystem(uint8_t systemByte);
    void LoadVisualInfo(zstr::ifstream& fin);
    void LoadModuleInfo(zstr::ifstream& fin);
    void LoadPatternMatrixValues(zstr::ifstream& fin);
    void LoadInstrumentsData(zstr::ifstream& fin);
    Instrument LoadInstrument(zstr::ifstream& fin, System systemType);
    void LoadWavetablesData(zstr::ifstream& fin);
    void LoadPatternsData(zstr::ifstream& fin);
    PatternRow LoadPatternRow(zstr::ifstream& fin, int effectsColumnsCount);
    void LoadPCMSamplesData(zstr::ifstream& fin);
    PCMSample LoadPCMSample(zstr::ifstream& fin);

private:
    uint8_t         m_DMFFileVersion;
    System          m_System;
    VisualInfo      m_VisualInfo;
    ModuleInfo      m_ModuleInfo;
    uint8_t**       m_PatternMatrixValues;
    uint8_t*        m_PatternMatrixMaxValues;
    uint8_t         m_TotalInstruments;
    Instrument*     m_Instruments;
    uint8_t         m_TotalWavetables;
    uint32_t*       m_WavetableSizes;
    uint32_t**      m_WavetableValues;
    PatternRow***   m_PatternValues;
    uint8_t*        m_ChannelEffectsColumnsCount;
    uint8_t         m_TotalPCMSamples;
    PCMSample*      m_PCMSamples;

    static const System m_Systems[];
};

class DMFConversionOptions : public ConversionOptionsInterface<DMFConversionOptions>
{
public:
    DMFConversionOptions() {}
    ~DMFConversionOptions() {}

    bool ParseArgs(std::vector<std::string>& args) override { return false; } // DMF files don't have any conversion flags right now
    void PrintHelp() override;
};

// End setup
REGISTER_MODULE_END(DMF, DMFConversionOptions, ModuleType::DMF, "dmf")

// Deflemask Game Boy channels
typedef enum DMF_GAMEBOY_CHANNEL {
    DMF_GAMEBOY_SQW1=0, DMF_GAMEBOY_SQW2=1, DMF_GAMEBOY_WAVE=2, DMF_GAMEBOY_NOISE=3
} DMF_GAMEBOY_CHANNEL;

// Compares notes n1 and n2. Returns 1 if n1 > n2, -1 if n1 < n2, and 0 if n1 == n2.
int8_t NoteCompare(const Note *n1, const Note *n2);

int8_t NoteCompare(const Note* n1, const Note n2);
