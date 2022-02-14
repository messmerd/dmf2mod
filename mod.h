/*
    mod.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares the ModuleInterface-derived class for ProTracker's 
    MOD files.

    Several limitations apply in order to export. For example, 
    for DMF --> MOD, the DMF file must use the Game Boy system, 
    patterns must have 64 rows, only one effect column is allowed 
    per channel, etc.
*/

#pragma once

#include "modules.h"

#include <string>
#include <sstream>
#include <map>

// Begin setup
REGISTER_MODULE_HEADER(MOD, MODConversionOptions)

// Forward defines
struct DMFNote;
struct DMFChannelRow;
struct MODChannelState;

// MOD effects:
// An effect is represented with 12 bits, which is 3 groups of 4 bits: [a][x][y] or [a][b][x]
// The effect code is [a] or [a][b], and the effect value is [x][y] or [x]. [x][y] codes are the
// extended effects. All effect codes are stored below. Non-extended effects have 0x0 in the right-most
// nibble in order to line up with the extended effects:
namespace MODEffectCode
{
    enum MODEffectCode
    {
        NoEffect=0x00, NoEffectVal=0x00, NoEffectCode=0x00, /* NoEffect is the same as ((uint16_t)NoEffectCode << 4) | NoEffectVal */
        Arp=0x00, PortUp=0x10, PortDown=0x20, Port2Note=0x30, Vibrato=0x40, Port2NoteVolSlide=0x50, VibratoVolSlide=0x60,
        Tremolo=0x70, Panning=0x80, SetSampleOffset=0x90, VolSlide=0xA0, PosJump=0xB0, SetVolume=0xC0, PatBreak=0xD0,
        SetFilter=0xE0, FineSlideUp=0xE1, FineSlideDown=0xE2, SetGlissando=0xE3, SetVibratoWaveform=0xE4,
        SetFinetune=0xE5, LoopPattern=0xE6, SetTremoloWaveform=0xE7, RetriggerSample=0xE9, FineVolSlideUp=0xEA,
        FineVolSlideDown=0xEB, CutSample=0xEC, DelaySample=0xED, DelayPattern=0xEE, InvertLoop=0xEF,
        SetSpeed=0xF0
    };
}

struct MODEffect
{
    uint16_t effect;
    uint16_t value;
};

// Lower enum value = higher priority
enum MODEffectPriority
{
    EffectPriorityStructureRelated, /* Can always be performed */
    EffectPrioritySampleChange, /* Can always be performed */
    EffectPriorityTempoChange,
    EffectPriorityVolumeChange,
    EffectPriorityOtherEffect,
    EffectPriorityUnsupportedEffect
};

struct MODNote
{
    uint16_t pitch;
    uint16_t octave;

    operator DMFNote() const;
};

struct MODChannelRow
{
    uint8_t SampleNumber;
    uint16_t SamplePeriod;
    unsigned EffectCode;
    uint8_t EffectValue;
};

typedef int dmf_sample_id_t;
typedef int mod_sample_id_t;

/*
 * Used during conversion from DMF to MOD. For mapping DMF square waves and 
 * wavetables ("DMF samples") with corresponding MOD samples. For DMF samples 
 * that span a wide note range, the corresponding MOD sample is split into two 
 * samples - low and high - in order to allow it to be played in the severely 
 * limited MOD format.
 */
struct MODMappedDMFSample
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

// Stores a MOD sample
struct MODSample
{
    std::string name; // 22 characters
    mod_sample_id_t id;
    unsigned length;
    int finetune;
    unsigned volume;
    unsigned repeatOffset;
    unsigned repeatLength;

    std::vector<int8_t> data;
};

class MODException : public ModuleException
{
public:
    template <class T, 
        class = typename std::enable_if<
        (std::is_enum<T>{} || std::is_integral<T>{}) &&
        (!std::is_enum<T>{} || std::is_convertible<std::underlying_type_t<T>, int>{})
        >::type>
    MODException(Category category, T errorCode, const std::string errorMessage = "")
        : ModuleException(category, errorCode, CreateErrorMessage(category, (int)errorCode, errorMessage))
    {}

private:
    // Creates module-specific error message from an error code and string argument
    std::string CreateErrorMessage(Category category, int errorCode, const std::string& arg);
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

    void Import(const std::string& filename) override
    {
        m_Status.Clear();
    }

    void Export(const std::string& filename) override;

    std::string GetName() const override { return m_ModuleName; }

    enum class ImportError {Success=0};
    enum class ImportWarning {};

    enum class ExportError {Success=0};
    enum class ExportWarning {};

    enum class ConvertError
    {
        Success=0,
        NotGameBoy,
        TooManyPatternMatrixRows,
        Over64RowPattern,
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
        TempoPrecision,
        EffectIgnored
    };

    
    static constexpr unsigned VolumeMax = 64u; // Yes, there are 65 different values for the volume

private:
    void ConvertFrom(const Module* input, const ConversionOptionsPtr& options) override;

    // Conversion from DMF:
    void ConvertFromDMF(const DMF& dmf, const ConversionOptionsPtr& options);
    void DMFConvertSamples(const DMF& dmf, std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap);
    void DMFCreateSampleMapping(const DMF& dmf, std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap, std::map<dmf_sample_id_t, std::pair<MODNote, MODNote>>& sampleIdLowestHighestNotesMap);
    void DMFSampleSplittingAndAssignment(std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap, const std::map<dmf_sample_id_t, std::pair<MODNote, MODNote>>& sampleIdLowestHighestNotesMap);
    void DMFConvertSampleData(const DMF& dmf, const std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap);
    void DMFConvertPatterns(const DMF& dmf, const std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap);
    void DMFConvertChannelRow(const DMF& dmf, const std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap, const DMFChannelRow& pat, MODChannelState& state, MODChannelRow& modChannelRow, MODEffect& noiseChannelEffect);
    std::multimap<MODEffectPriority, MODEffect> DMFConvertEffects(MODChannelState& state, const DMFChannelRow& pat);
    MODNote DMFConvertNote(MODChannelState& state, const DMFChannelRow& pat, const std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap, std::multimap<MODEffectPriority, MODEffect>& modEffects, mod_sample_id_t& sampleId, uint16_t& period, bool& volumeChangeNeeded);
    void DMFConvertInitialBPM(const DMF& dmf, unsigned& tempo, unsigned& speed);
    
    // Export:
    void ExportModuleName(std::ofstream& fout) const;
    void ExportSampleInfo(std::ofstream& fout) const;
    void ExportModuleInfo(std::ofstream& fout) const;
    void ExportPatterns(std::ofstream& fout) const;
    void ExportSampleData(std::ofstream& fout) const;

    // Other:
    uint8_t GetMODTempo(double bpm);

    inline const MODChannelRow& GetChannelRow(unsigned pattern, unsigned row, unsigned channel)
    {
        return m_Patterns.at(pattern).at((row << m_NumberOfChannelsPowOfTwo) + channel);
    }

    inline void SetChannelRow(unsigned pattern, unsigned row, unsigned channel, MODChannelRow& channelRow)
    {
        m_Patterns.at(pattern).at((row << m_NumberOfChannelsPowOfTwo) + channel) = std::move(channelRow);
    }

private:
    MODConversionOptions* m_Options;

    //////////// Temporaries used during DMF-->MOD conversion
    const bool m_UsingSetupPattern = true; // Whether to use a pattern at the start of the module to set up the initial tempo and other stuff.

    //////////// MOD file info
    std::string m_ModuleName;
    int8_t m_TotalMODSamples;
    unsigned m_NumberOfChannels;
    unsigned char m_NumberOfChannelsPowOfTwo; // For efficiency. 2^m_NumberOfChannelsPowOfTwo = m_NumberOfChannels.
    unsigned m_NumberOfRowsInPatternMatrix;
    std::vector<std::vector<MODChannelRow>> m_Patterns; // Per pattern: Vector of channel rows that together contain data for entire pattern
    std::map<mod_sample_id_t, MODSample> m_Samples;
};
