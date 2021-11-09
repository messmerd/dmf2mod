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

struct ChannelRow
{
    uint8_t SampleNumber;
    uint16_t SamplePeriod;
    uint8_t EffectCode;
    uint8_t EffectValue;
};

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
    bool ConvertFrom(const Module* input, const ConversionOptionsPtr& options) override;

    int InitSamples(const DMF& dmf);
    int FinalizeSampMap(const DMF& dmf);
    void ExportSampleInfo(const DMF& dmf, int8_t ptSampleNumLow, int8_t ptSampleNumHigh, uint8_t indexLow, uint8_t indexHigh, int8_t finetune);
    void ExportSampleData(const DMF& dmf);
    void ExportSampleDataHelper(const DMF& dmf, uint8_t ptSampleNum, uint8_t index);
    int WriteProTrackerPatternRow(const DMF& dmf, const PatternRow& pat, MODChannelState& state);
    int CheckEffects(const PatternRow& pat, MODChannelState& state, uint16_t& effect);
    uint16_t GetProTrackerEffect(int16_t effectCode, int16_t effectValue);

    uint8_t GetPTTempo(double bpm);


    const ChannelRow& GetChannelRow(unsigned pattern, unsigned row, unsigned channel)
    {
        return m_Patterns.at(pattern).at((row << m_NumberOfChannelsPowOfTwo) + channel);
    }

    std::vector<std::vector<ChannelRow>> m_Patterns; // Per pattern: Vector of channel rows that together contain data for entire pattern
    unsigned m_NumberOfChannels;
    unsigned char m_NumberOfChannelsPowOfTwo; // For efficiency. 2^m_NumberOfChannelsPowOfTwo = m_NumberOfChannels.

    enum class SampleType
    {
        Square, Wave
    };

    /*
    sampMap gives the ProTracker (PT) sample numbers for a given SQW / WAVE sample of either low note range or high note range. 
    The index for a particular SQW / WAVE sample is specified using the format below: 
    For index 0 thru 3: SQW samples, 12.5% duty cycle thru 75% duty cycle (low note range) 
    For index 4 thru totalWavetables + 3: WAVE samples (low note range) 
    For index 4 + totalWavetables thru 7 + totalWavetables: SQW samples, 12.5% duty cycle thru 75% duty cycle (high note range) 
    For index 8 + totalWavetables thru 7 + totalWavetables * 2: WAVE samples (high note range)  
    The value of sampMap is -1 if a PT sample is not needed for the given SQW / WAVE sample. 
    */
    std::vector<int8_t> m_SampleMap;
    //std::map<unsigned char, unsigned char> m_SampleMap;


    unsigned char m_DMFTotalWavetables; // For effeciency. dmf->GetTotalWavetables() 
    
    inline unsigned char GetMODSampleNumberLowFromDMFSquare(unsigned squareDuty)
    {
        return m_SampleMap.at(squareDuty);
    }

    inline unsigned char GetMODSampleNumberHighFromDMFSquare(unsigned squareDuty)
    {
        return m_SampleMap.at(4 + m_DMFTotalWavetables + squareDuty);
    }
    
    inline unsigned char GetMODSampleNumberLowFromDMFWavetable(unsigned wavetableNumber)
    {
        return m_SampleMap.at(4 + wavetableNumber);
    }

    inline unsigned char GetMODSampleNumberHighFromDMFWavetable(unsigned wavetableNumber)
    {
        return m_SampleMap.at(8 + m_DMFTotalWavetables + wavetableNumber);
    }

    /*
    Specifies the point at which the note range starts for a given SQW / WAVE sample (high or low note range).
    A note range always contains 36 notes. I.e. C-2 thru B-4 (Deflemask tracker note format).
    Uses the same index format of sampMap minus the high note range indices.  
    If a certain SQW / WAVE sample is unused, then pitch = 0 and octave = 0. 
    */
    std::vector<Note> m_NoteRangeStart;

    /*
    Specifies the ProTracker sample length for a given SQW / WAVE sample.
    Uses the same index format of sampMap.  
    If a certain SQW / WAVE sample is unused, then pitch = 0 and octave = 0. 
    The value of sampleLength is -1 if a PT sample is not needed for the given SQW / WAVE sample.
    */
    std::vector<int> m_SampleLength;

    // Lowest/highest note for each square wave duty cycle or wavetable instrument.
    // First 4 indicies are for square waves, rest are for wavetables.
    std::vector<Note> m_LowestNotes;
    std::vector<Note> m_HighestNotes;

    int8_t m_TotalMODSamples;

    MODConversionOptions* m_Options;

    const bool m_UsingSetupPattern = true; // Whether to use a pattern at the start of the module to set up the initial tempo and other stuff. 

    std::stringstream m_Stream;
};
