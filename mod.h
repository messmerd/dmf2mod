/*
    mod.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares the Module-derived class for ProTracker's MOD files.

    Several limitations apply in order to export. For example, 
    for DMF --> MOD, the DMF file must use the Game Boy system, 
    patterns must have 64 rows, only one effect column is allowed 
    per channel, etc.
*/

#pragma once

#include "modules.h"

#include <string>
#include <sstream>

// Begin setup
REGISTER_MODULE_HEADER(MOD, MODConversionOptions)

// Forward defines
struct Note;
struct PatternRow;
struct MODChannelState;

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

class MODConversionOptions : public ConversionOptionsInterface<MODConversionOptions>
{
public:
    MODConversionOptions()
    {
        Downsample = false;
        Effects = EffectsEnum::Max;
    }

    ~MODConversionOptions() {}

    enum class EffectsEnum
    {
        Min, Max
    };

    EffectsEnum GetEffects() const { return Effects; }
    bool GetDownsample() const { return Downsample; }

private:
    bool ParseArgs(std::vector<std::string>& args) override;
    void PrintHelp() override;

    bool Downsample;
    EffectsEnum Effects;
};

class MOD : public ModuleInterface<MOD, MODConversionOptions>
{
public:
    MOD();
    ~MOD() {};
    void CleanUp() {};

    bool Import(const std::string& filename) override
    {
        m_Status.Clear();
        return true;
    }

    bool Export(const std::string& filename) override;

    std::string GetName() const override { return ""; }

    enum class ImportError {Success=0};
    enum class ImportWarning {};

    enum class ExportError {Success=0};
    enum class ExportWarning {};

    enum class ConvertError
    {
        Success=0,
        NotGameBoy,
        TooManyPatternMatrixRows,
        Not64RowPattern,
        WaveDownsample,
        EffectVolume,
        MultipleEffects
    };

    enum class ConvertWarning
    {
        None=0,
        PitchHigh,
        TempoLow,
        TempoHigh,
        EffectIgnored
    };

private:
    bool ConvertFrom(const Module* input, ConversionOptionsPtr& options) override;

    int InitSamples(const DMF* dmf, Note **lowestNote, Note **highestNote);
    int FinalizeSampMap(const DMF* dmf, Note *lowestNote, Note *highestNote);
    void ExportSampleInfo(const DMF* dmf, int8_t ptSampleNumLow, int8_t ptSampleNumHigh, uint8_t indexLow, uint8_t indexHigh, int8_t finetune);
    void ExportSampleData(const DMF* dmf);
    void ExportSampleDataHelper(const DMF* dmf, uint8_t ptSampleNum, uint8_t index);
    int WriteProTrackerPatternRow(const DMF* dmf, PatternRow *pat, MODChannelState *state);
    int CheckEffects(PatternRow *pat, MODChannelState *state, uint16_t *effect);
    uint16_t GetProTrackerEffect(int16_t effectCode, int16_t effectValue);

    uint8_t GetPTTempo(double bpm);

    std::stringstream m_Stream;
};
