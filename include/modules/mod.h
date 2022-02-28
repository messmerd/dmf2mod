/*
    mod.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares the ModuleInterface-derived class for ProTracker's 
    MOD files.
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
enum class DMFNotePitch;
struct MODChannelState;
struct MODState;

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
        FineVolSlideDown=0xEB, CutSample=0xEC, DelaySample=0xED, DelayPattern=0xEE, InvertLoop=0xEF, SetSpeed=0xF0,

        /* The following are not actual MOD effects, but they are useful during conversions */
        DutyCycleChange=0xFF0, WavetableChange=0xFF1 
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
    EffectPriorityUnsupportedEffect /* Must be the last enum value */
};

struct MODNote
{
    uint16_t pitch;
    uint16_t octave;

    MODNote() = default;
    MODNote(uint16_t p, uint16_t o)
        : pitch(p), octave(o)
    {}

    bool operator>(const MODNote& rhs) const
    {
        return (this->octave << 4) + this->pitch > (rhs.octave << 4) + rhs.pitch;
    }

    bool operator>=(const MODNote& rhs) const
    {
        return (this->octave << 4) + this->pitch >= (rhs.octave << 4) + rhs.pitch;
    }

    bool operator<(const MODNote& rhs) const
    {
        return (this->octave << 4) + this->pitch < (rhs.octave << 4) + rhs.pitch;
    }

    bool operator<=(const MODNote& rhs) const
    {
        return (this->octave << 4) + this->pitch <= (rhs.octave << 4) + rhs.pitch;
    }

    bool operator==(const MODNote& rhs) const
    {
        return this->octave == rhs.octave && this->pitch == rhs.pitch;
    }

    bool operator!=(const MODNote& rhs) const
    {
        return !(*this == rhs);
    }

    MODNote(const DMFNote& dmfNote);
    MODNote(DMFNotePitch p, uint16_t o);
    MODNote& operator=(const DMFNote& dmfNote);
    DMFNote ToDMFNote() const;
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
 * MOD has only a 3 octave range while Deflemask has 8.
 * For this reason, a DMF square wave or wavetable may
 * need to be broken up into 1 to 3 different MOD samples
 * of varying sample lengths to emulate the full range of 
 * Deflemask. DMFSampleMapper handles the details of this.
 */
class DMFSampleMapper
{
public:
    enum class SampleType
    {
        Silence, Square, Wave
    };

    enum class NoteRange
    {
        First=0, Second=1, Third=2
    };

    enum class NoteRangeName
    {
        None=-1, Low=0, Middle=1, High=2
    };

    DMFSampleMapper();

    mod_sample_id_t Init(dmf_sample_id_t dmfSampleId, mod_sample_id_t startingId, const std::pair<DMFNote, DMFNote>& dmfNoteRange);
    mod_sample_id_t InitSilence();

    MODNote GetMODNote(const DMFNote& dmfNote, NoteRange& modNoteRange) const;
    NoteRange GetMODNoteRange(DMFNote dmfNote) const;
    mod_sample_id_t GetMODSampleId(DMFNote dmfNote) const;
    mod_sample_id_t GetMODSampleId(NoteRange modNoteRange) const;
    unsigned GetMODSampleLength(NoteRange modNoteRange) const;
    NoteRange GetMODNoteRange(mod_sample_id_t modSampleId) const;
    NoteRangeName GetMODNoteRangeName(NoteRange modNoteRange) const;

    int GetNumMODSamples() const { return m_NumMODSamples; }
    SampleType GetSampleType() const { return m_SampleType; }
    mod_sample_id_t GetFirstMODSampleId() const { return m_ModIds[0]; }
    bool IsDownsamplingNeeded() const { return m_DownsamplingNeeded; }

private:
    dmf_sample_id_t m_DmfId;
    mod_sample_id_t m_ModIds[3]; // Up to 3 MOD samples from one DMF sample
    unsigned m_ModSampleLengths[3];
    std::vector<DMFNote> m_RangeStart;
    int m_NumMODSamples;
    SampleType m_SampleType;
    bool m_DownsamplingNeeded;
    int m_ModOctaveShift;
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
    using SampleMap = std::map<dmf_sample_id_t, DMFSampleMapper>;
    using PriorityEffectsMap = std::multimap<MODEffectPriority, MODEffect>;
    using DMFSampleNoteRangeMap = std::map<dmf_sample_id_t, std::pair<DMFNote, DMFNote>>;

    void ImportRaw(const std::string& filename) override;
    void ExportRaw(const std::string& filename) override;
    void ConvertRaw(const Module* input, const ConversionOptionsPtr& options) override;

    // Conversion from DMF:
    void ConvertFromDMF(const DMF& dmf, const ConversionOptionsPtr& options);
    void DMFConvertSamples(const DMF& dmf, SampleMap& sampleMap);
    void DMFCreateSampleMapping(const DMF& dmf, SampleMap& sampleMap, DMFSampleNoteRangeMap& sampleIdLowestHighestNotesMap);
    void DMFSampleSplittingAndAssignment(SampleMap& sampleMap, const DMFSampleNoteRangeMap& sampleIdLowestHighestNotesMap);
    void DMFConvertSampleData(const DMF& dmf, const SampleMap& sampleMap);
    
    void DMFConvertPatterns(const DMF& dmf, const SampleMap& sampleMap);
    PriorityEffectsMap DMFConvertEffects(const DMFChannelRow& pat);
    PriorityEffectsMap DMFConvertEffects_NoiseChannel(const DMFChannelRow& pat);
    void DMFUpdateStatePre(const DMF& dmf, MODState& state, const PriorityEffectsMap& modEffects);
    void DMFGetAdditionalEffects(const DMF& dmf, MODState& state, const DMFChannelRow& pat, PriorityEffectsMap& modEffects);
    //void UpdateStatePost(const DMF& dmf, MODState& state, const PriorityEffectsMap& modEffects);
    MODNote DMFConvertNote(MODState& state, const DMFChannelRow& pat, const SampleMap& sampleMap, PriorityEffectsMap& modEffects, mod_sample_id_t& sampleId, uint16_t& period);
    MODChannelRow DMFApplyNoteAndEffect(MODState& state, const PriorityEffectsMap& modEffects, mod_sample_id_t modSampleId, uint16_t period);
    
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
        m_Patterns.at(pattern).at((row << m_NumberOfChannelsPowOfTwo) + channel) = channelRow;
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
