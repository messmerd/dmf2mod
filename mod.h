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
typedef enum MOD_EFFECT {
    MOD_NOEFFECT=0x00, MOD_NOEFFECTVAL=0x00, MOD_NOEFFECT_CODE=0x00, /* MOD_NOEFFECT_CODE is the same as ((uint16_t)MOD_NOEFFECT << 4) | MOD_NOEFFECTVAL */
    MOD_ARP=0x00, MOD_PORTUP=0x10, MOD_PORTDOWN=0x20, MOD_PORT2NOTE=0x30, MOD_VIBRATO=0x40, MOD_PORT2NOTEVOLSLIDE=0x50, MOD_VIBRATOVOLSLIDE=0x60,
    MOD_TREMOLO=0x70, MOD_PANNING=0x80, MOD_SETSAMPLEOFFSET=0x90, MOD_VOLSLIDE=0xA0, MOD_POSJUMP=0xB0, MOD_SETVOLUME=0xC0, MOD_PATBREAK=0xD0, 
    MOD_SETFILTER=0xE0, MOD_FINESLIDEUP=0xE1, MOD_FINESLIDEDOWN=0xE2, MOD_SETGLISSANDO=0xE3, MOD_SETVIBRATOWAVEFORM=0xE4, 
    MOD_SETFINETUNE=0xE5, MOD_LOOPPATTERN=0xE6, MOD_SETTREMOLOWAVEFORM=0xE7, MOD_RETRIGGERSAMPLE=0xE9, MOD_FINEVOLSLIDEUP=0xEA, 
    MOD_FINEVOLSLIDEDOWN=0xEB, MOD_CUTSAMPLE=0xEC, MOD_DELAYSAMPLE=0xED, MOD_DELAYPATTERN=0xEE, MOD_INVERTLOOP=0xEF,
    MOD_SETSPEED=0xF0
} MOD_EFFECT;

struct MODNote
{
    uint16_t pitch;
    uint16_t octave;

    operator Note() const;
};

struct ChannelRow
{
    uint8_t SampleNumber;
    uint16_t SamplePeriod;
    uint8_t EffectCode;
    uint8_t EffectValue;
};

typedef int dmf_sample_id_t;
typedef int mod_sample_id_t;

struct MODSampleInfo
{
    mod_sample_id_t lowId;

    // -1 in the case that the upper range is not needed
    mod_sample_id_t highId;

    // Sample lengths:
    unsigned lowLength;
    unsigned highLength;

    /*
        Specifies the first note of high note range for a given SQW / WAVE sample, or
        the first note of the range if it does not have both a high and low note range.
        A note range always contains 36 notes. I.e. C-2 thru B-4 (Deflemask tracker note format).
    */
    MODNote splitPoint;
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

    // Conversion from DMF:
    bool CreateSampleMapping(const DMF& dmf);
    bool SampleSplittingAndAssignment();
    bool ConvertSampleData(const DMF& dmf);
    
    bool ConvertPatterns(const DMF& dmf);
    bool ConvertChannelRow(const DMF& dmf, const PatternRow& pat, MODChannelState& state, ChannelRow& modChannelRow);
    bool ConvertEffect(const PatternRow& pat, MODChannelState& state, uint16_t& effectCode, uint16_t& effectValue);
    void ConvertEffectCodeAndValue(int16_t dmfEffectCode, int16_t dmfEffectValue, uint16_t& modEffectCode, uint16_t& modEffectValue);
    

    // Export:
    void ExportModuleName(std::ofstream& fout) const;
    void ExportSampleInfo(std::ofstream& fout) const;
    void ExportModuleInfo(std::ofstream& fout) const;
    void ExportPatterns(std::ofstream& fout) const;
    void ExportSampleData(std::ofstream& fout) const;

    uint8_t GetMODTempo(double bpm);

    mod_sample_id_t GetMODSampleId(dmf_sample_id_t dmfSampleId, const Note& dmfNote);
    MODSampleInfo* GetMODSampleInfo(dmf_sample_id_t dmfSampleId);
    bool IsDMFSampleUsed(dmf_sample_id_t dmfSampleId);

    inline const ChannelRow& GetChannelRow(unsigned pattern, unsigned row, unsigned channel)
    {
        return m_Patterns.at(pattern).at((row << m_NumberOfChannelsPowOfTwo) + channel);
    }

    inline void SetChannelRow(unsigned pattern, unsigned row, unsigned channel, ChannelRow& channelRow)
    {
        //std::cout << "pattern:" << pattern << "; (row << m_NumberOfChannelsPowOfTwo) + channel" << ((row << m_NumberOfChannelsPowOfTwo) + channel) << ";\n";
        m_Patterns.at(pattern).at((row << m_NumberOfChannelsPowOfTwo) + channel) = std::move(channelRow);
    }


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
    //std::vector<int8_t> m_SampleMap;
    
    inline unsigned char GetMODSampleNumberLowFromDMFSquare(unsigned squareDuty) const;
    inline unsigned char GetMODSampleNumberHighFromDMFSquare(unsigned squareDuty) const;
    inline unsigned char GetMODSampleNumberLowFromDMFWavetable(unsigned wavetableNumber) const;
    inline unsigned char GetMODSampleNumberHighFromDMFWavetable(unsigned wavetableNumber) const;

    /*
    Specifies the point at which the note range starts for a given SQW / WAVE sample (high or low note range).
    A note range always contains 36 notes. I.e. C-2 thru B-4 (Deflemask tracker note format).
    Uses the same index format of sampMap minus the high note range indices.  
    If a certain SQW / WAVE sample is unused, then pitch = 0 and octave = 0. 
    */
    //std::vector<Note> m_NoteRangeStart;

    /*
    Specifies the ProTracker sample length for a given SQW / WAVE sample.
    Uses the same index format of sampMap.  
    If a certain SQW / WAVE sample is unused, then pitch = 0 and octave = 0. 
    The value of sampleLength is -1 if a PT sample is not needed for the given SQW / WAVE sample.
    */
    //std::vector<int> m_SampleLength;

    // Lowest/highest note for each square wave duty cycle or wavetable instrument.
    // First 4 indicies are for square waves, rest are for wavetables.
    //std::vector<Note> m_LowestNotes;
    //std::vector<Note> m_HighestNotes;

    
    

    

    MODConversionOptions* m_Options;


    //////////// Temporaries used during DMF-->MOD conversion
    const bool m_UsingSetupPattern = true; // Whether to use a pattern at the start of the module to set up the initial tempo and other stuff. 
    bool m_SilentSampleNeeded = false;
    unsigned char m_DMFTotalWavetables; // For effeciency. dmf->GetTotalWavetables()
    uint8_t m_InitialTempo;

    // Lowest/highest note for each square wave duty cycle or wavetable instrument.
    std::map<dmf_sample_id_t, std::pair<MODNote, MODNote>> m_SampleIdLowestHighestNotesMap;

    // TODO: This map uses DMF sample IDs but is used during export. Make it module-independent
    std::map<dmf_sample_id_t, MODSampleInfo> m_SampleMap2;

    //////////// MOD file info
    std::string m_ModuleName;
    int8_t m_TotalMODSamples;
    unsigned m_NumberOfChannels;
    unsigned char m_NumberOfChannelsPowOfTwo; // For efficiency. 2^m_NumberOfChannelsPowOfTwo = m_NumberOfChannels.
    unsigned m_NumberOfRowsInPatternMatrix;
    std::vector<std::vector<ChannelRow>> m_Patterns; // Per pattern: Vector of channel rows that together contain data for entire pattern
    std::map<mod_sample_id_t, std::vector<int8_t>> m_Samples;

};
