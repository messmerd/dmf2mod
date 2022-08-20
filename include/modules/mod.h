/*
    mod.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares a ModuleInterface-derived class for ProTracker's
    MOD files.
*/

#pragma once

#include "core/module.h"
#include "dmf.h"

#include <string>
#include <sstream>
#include <map>

namespace d2m {

// Declare module

class MOD;

template<>
struct ModuleGlobalData<MOD> : public ModuleGlobalDataGeneric<PatternStorageType::ORC>
{
    // In the future, we'll be able to detect when a MOD module
    // was created with dmf2mod, which will help when converting
    // from MOD to another module type.
    bool madeWithDmf2mod;
};

MODULE_DECLARE(MOD, MODConversionOptions)

template<>
struct Row<MOD>
{
    uint8_t SampleNumber;
    uint16_t SamplePeriod;
    unsigned EffectCode;
    unsigned EffectValue;
};

namespace mod {

// MOD effects:
// An effect is represented with 12 bits, which is 3 groups of 4 bits: [a][x][y] or [a][b][x]
// The effect code is [a] or [a][b], and the effect value is [x][y] or [x]. [x][y] codes are the
// extended effects. All effect codes are stored below. Non-extended effects have 0x0 in the right-most
// nibble in order to line up with the extended effects:
namespace EffectCode
{
    enum EffectCode
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

struct Effect
{
    uint16_t effect;
    uint16_t value;
};

// Lower enum value = higher priority
enum EffectPriority
{
    EffectPriorityStructureRelated, /* Can always be performed */
    EffectPrioritySampleChange, /* Can always be performed */
    EffectPriorityTempoChange,
    EffectPriorityPortUp,
    EffectPriorityPortDown,
    //EffectPriorityPort2NoteVolSlide,
    EffectPriorityPort2Note,
    EffectPriorityVolumeChange,
    //EffectPriorityVolSlide,
    EffectPriorityArp,
    //EffectPriorityVibratoVolSlide,
    EffectPriorityVibrato,
    EffectPriorityOtherEffect,
    EffectPriorityUnsupportedEffect /* Must be the last enum value */
};

/*
struct ChannelRow
{
    uint8_t SampleNumber;
    uint16_t SamplePeriod;
    unsigned EffectCode;
    unsigned EffectValue;
};
*/

using dmf_sample_id_t = int;
using mod_sample_id_t = int;

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

    mod_sample_id_t Init(dmf_sample_id_t dmfSampleId, mod_sample_id_t startingId, const std::pair<Note, Note>& dmfNoteRange);
    mod_sample_id_t InitSilence();

    Note GetMODNote(const Note& dmfNote, NoteRange& modNoteRange) const;
    NoteRange GetMODNoteRange(const Note& dmfNote) const;
    mod_sample_id_t GetMODSampleId(const Note& dmfNote) const;
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
    std::vector<Note> m_RangeStart;
    int m_NumMODSamples;
    SampleType m_SampleType;
    bool m_DownsamplingNeeded;
    int m_ModOctaveShift;
};

// Stores a MOD sample
struct Sample
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

using PriorityEffectsMap = std::multimap<mod::EffectPriority, mod::Effect>;

// The current square wave duty cycle, note volume, and other information that the 
//      tracker stores for each channel while playing a tracker file.
struct ChannelState
{
    enum PORTDIR
    {
        PORT_UP, PORT_DOWN
    };

    int channel;
    uint16_t dutyCycle;
    uint16_t wavetable;
    bool sampleChanged; // MOD sample
    int16_t volume;
    bool notePlaying;
    DMFSampleMapper::NoteRange noteRange;
    PriorityEffectsMap persistentEffects;
    int rowsUntilPortAutoOff;
    PORTDIR portDirection;
    uint16_t portParam;
    NoteSlot currentNote; // DMF Note
};

struct State
{
    struct global
    {
        bool suspended;      // true == currently in part that a Position Jump skips over
        int jumpDestination; // DMF pattern matrix row where you are jumping to. Not a loop.
        Effect channelIndependentEffect;
        unsigned ticksPerRowPair; // DMF's time_base * (speed_a + speed_b)
        unsigned channel;    // The current channel in DMF or MOD
        unsigned order;      // The current pattern matrix row in DMF
        unsigned patternRow; // The current pattern row in DMF?
    } global;
    
    ChannelState channel[4];
    ChannelState channelCopy[4];

    Row<MOD> channelRows[4];

    State()
    {
        Init(6);
    }

    State(unsigned ticksPerRowPair)
    {
        Init(ticksPerRowPair);
    }

    void Init(unsigned ticksPerRowPair)
    {
        global.suspended = false;
        global.jumpDestination = -1;
        global.channelIndependentEffect = {EffectCode::NoEffectCode, EffectCode::NoEffectVal};
        global.ticksPerRowPair = ticksPerRowPair;

        global.channel = 0;
        global.order = 0;
        global.patternRow = 0;

        for (int i = 0; i < 4; i++)
        {
            channel[i].channel = i;
            channel[i].dutyCycle = 0; // Default is 0 or a 12.5% duty cycle square wave.
            channel[i].wavetable = 0; // Default is wavetable #0.
            channel[i].sampleChanged = true; // Whether dutyCycle or wavetable recently changed
            channel[i].volume = dmf::DMFVolumeMax; // The max volume for a channel (in DMF units)
            channel[i].notePlaying = false; // Whether a note is currently playing on a channel
            channel[i].noteRange = DMFSampleMapper::NoteRange::First; // Which MOD sample note range is currently being used
            channel[i].persistentEffects = {};
            channel[i].rowsUntilPortAutoOff = -1; // -1 = port not on; 0 = need to turn port off; >0 = rows until port auto off
            channel[i].portDirection = ChannelState::PORT_UP;
            channel[i].portParam = 0;
            channel[i].currentNote = {};

            channelCopy[i] = channel[i];
            channelRows[i] = {};
        }
    }

    void Save(unsigned jumpDestination)
    {
        // Save copy of channel states
        for (int i = 0; i < 4; i++)
        {
            channelCopy[i] = channel[i];
        }
        global.suspended = true;
        global.jumpDestination = (int)jumpDestination;
    }

    void Restore()
    {
        // Restore channel states from copy
        for (int i = 0; i < 4; i++)
        {
            channel[i] = channelCopy[i];
        }
        global.suspended = false;
        global.jumpDestination = -1;
    }
};

} // namespace mod

class MODException : public ModuleException
{
public:
    template <class T, class = std::enable_if_t<std::is_enum<T>{} && std::is_convertible<std::underlying_type_t<T>, int>{}>>
    MODException(Category category, T errorCode, const std::string& args = "")
        : ModuleException(category, static_cast<int>(errorCode), CreateErrorMessage(category, (int)errorCode, args)) {}

private:
    // Creates module-specific error message from an error code and string argument
    std::string CreateErrorMessage(Category category, int errorCode, const std::string& arg);
};

class MODConversionOptions : public ConversionOptionsInterface<MODConversionOptions>
{
public:
    MODConversionOptions() = default;
    ~MODConversionOptions() = default;

    enum class OptionEnum
    {
        AmigaFilter, Arpeggio, Portamento, Port2Note, Vibrato, TempoType
    };

    enum class EffectsEnum
    {
        Min, Max
    };

    enum class TempoType
    {
        Accuracy, EffectCompatibility
    };

    inline bool UseAmigaFilter() const { return GetOption(OptionEnum::AmigaFilter).GetValue<bool>(); }
    inline bool AllowArpeggio() const { return GetOption(OptionEnum::Arpeggio).GetValue<bool>(); }
    inline bool AllowPortamento() const { return GetOption(OptionEnum::Portamento).GetValue<bool>(); }
    inline bool AllowPort2Note() const { return GetOption(OptionEnum::Port2Note).GetValue<bool>(); }
    inline bool AllowVibrato() const { return GetOption(OptionEnum::Vibrato).GetValue<bool>(); }
    inline TempoType GetTempoType() const { return static_cast<TempoType>(GetOption(OptionEnum::TempoType).GetValueAsIndex()); }

    inline bool AllowEffects() const { return AllowArpeggio() || AllowPortamento() || AllowPort2Note() || AllowVibrato(); }

private:

};

class MOD : public ModuleInterface<MOD, MODConversionOptions>
{
public:
    MOD();
    ~MOD() = default;
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
        WrongChannelCount
    };

    enum class ConvertWarning
    {
        None=0,
        PitchHigh,
        TempoLow,
        TempoHigh,
        TempoLowCompat,
        TempoHighCompat,
        TempoAccuracy,
        EffectIgnored,
        WaveDownsample,
        MultipleEffects
    };
    
    static constexpr unsigned VolumeMax = 64u; // Yes, there are 65 different values for the volume

private:
    using SampleMap = std::map<mod::dmf_sample_id_t, mod::DMFSampleMapper>;
    using DMFSampleNoteRangeMap = std::map<mod::dmf_sample_id_t, std::pair<Note, Note>>;

    void ImportRaw(const std::string& filename) override;
    void ExportRaw(const std::string& filename) override;
    void ConvertRaw(const Module* input) override;

    // Conversion from DMF:
    void ConvertFromDMF(const DMF& dmf);
    void DMFConvertSamples(const DMF& dmf, SampleMap& sampleMap);
    void DMFCreateSampleMapping(const DMF& dmf, SampleMap& sampleMap, DMFSampleNoteRangeMap& sampleIdLowestHighestNotesMap);
    void DMFSampleSplittingAndAssignment(SampleMap& sampleMap, const DMFSampleNoteRangeMap& sampleIdLowestHighestNotesMap);
    void DMFConvertSampleData(const DMF& dmf, const SampleMap& sampleMap);

    void DMFConvertPatterns(const DMF& dmf, const SampleMap& sampleMap);
    mod::PriorityEffectsMap DMFConvertEffects(const Row<DMF>& row, mod::State& state);
    mod::PriorityEffectsMap DMFConvertEffects_NoiseChannel(const Row<DMF>& row);
    void DMFUpdateStatePre(const DMF& dmf, mod::State& state, const mod::PriorityEffectsMap& modEffects);
    void DMFGetAdditionalEffects(const DMF& dmf, mod::State& state, const Row<DMF>& row, mod::PriorityEffectsMap& modEffects);
    //void DMFUpdateStatePost(const DMF& dmf, mod::State& state, const mod::PriorityEffectsMap& modEffects);
    Note DMFConvertNote(mod::State& state, const Row<DMF>& row, const SampleMap& sampleMap, mod::PriorityEffectsMap& modEffects, mod::mod_sample_id_t& sampleId, uint16_t& period);
    Row<MOD> DMFApplyNoteAndEffect(mod::State& state, const mod::PriorityEffectsMap& modEffects, mod::mod_sample_id_t modSampleId, uint16_t period);

    void DMFConvertInitialBPM(const DMF& dmf, unsigned& tempo, unsigned& speed);

    // Export:
    void ExportModuleName(std::ofstream& fout) const;
    void ExportSampleInfo(std::ofstream& fout) const;
    void ExportModuleInfo(std::ofstream& fout) const;
    void ExportPatterns(std::ofstream& fout) const;
    void ExportSampleData(std::ofstream& fout) const;

    // Other:
    /*
    inline mod::ChannelRow& GetChannelRow(unsigned pattern, unsigned row, unsigned channel)
    {
        return m_Patterns[pattern][(row << m_NumberOfChannelsPowOfTwo) + channel];
    }

    inline void SetChannelRow(unsigned pattern, unsigned row, unsigned channel, mod::ChannelRow& channelRow)
    {
        m_Patterns.at(pattern).at((row << m_NumberOfChannelsPowOfTwo) + channel) = channelRow;
    }
    */

private:
    //////////// Temporaries used during DMF-->MOD conversion
    const bool m_UsingSetupPattern = true; // Whether to use a pattern at the start of the module to set up the initial tempo and other stuff.

    //////////// MOD file info
    std::string m_ModuleName;
    int8_t m_TotalMODSamples;
    ///unsigned m_NumberOfChannels;
    //unsigned char m_NumberOfChannelsPowOfTwo; // For efficiency. 2^m_NumberOfChannelsPowOfTwo = m_NumberOfChannels.
    ///unsigned m_NumberOfRowsInPatternMatrix;
    //std::vector<std::vector<mod::ChannelRow>> m_Patterns; // Per pattern: Vector of channel rows that together contain data for entire module
    std::map<mod::mod_sample_id_t, mod::Sample> m_Samples;
};

} // namespace d2m
