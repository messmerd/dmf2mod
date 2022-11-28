/*
    mod.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares all classes used for ProTracker's MOD files.
*/

#pragma once

#include "core/module.h"
#include "modules/dmf.h"

#include <string>
#include <sstream>
#include <map>
#include <array>

namespace d2m {

///////////////////////////////////////////////////////////
// Setup template specializations used by MOD
///////////////////////////////////////////////////////////

class MOD;

template<>
struct ModuleGlobalData<MOD> : public ModuleGlobalDataDefault<DataStorageType::kORC>
{
    // In the future, we'll be able to detect when a MOD module
    // was created with dmf2mod, which will help when converting
    // from MOD to another module type.
    bool madeWithDmf2mod;
};

template<>
struct Row<MOD>
{
    SoundIndexType<MOD> sample;
    NoteSlot note;
    Effect effect;
};

///////////////////////////////////////////////////////////
// mod namespace
///////////////////////////////////////////////////////////

namespace mod {

// Custom dmf2mod internal effect codes (see effects.h)
namespace Effects
{
    enum
    {
        kSetSampleOffset=1,
        kSetVolume,
        kSetFilter,
        kFineSlideUp,
        kFineSlideDown,
        kSetGlissando,
        kSetVibratoWaveform,
        kSetFinetune,
        kLoopPattern,
        kSetTremoloWaveform,
        kFineVolSlideUp,
        kFineVolSlideDown,
        kDelayPattern,
        kInvertLoop
    };
}

// Higher enum value = higher priority
enum EffectPriority
{
    kEffectPriorityUnsupportedEffect=0,
    kEffectPriorityOtherEffect,
    kEffectPriorityVibrato,
    //kEffectPriorityVibratoVolSlide,
    kEffectPriorityArp,
    //kEffectPriorityVolSlide,
    kEffectPriorityVolumeChange,
    kEffectPriorityPort2Note,
    //kEffectPriorityPort2NoteVolSlide,
    kEffectPriorityPortDown,
    kEffectPriorityPortUp,
    kEffectPriorityTempoChange,
    kEffectPrioritySampleChange, /* Can always be performed */
    kEffectPriorityStructureRelated, /* Can always be performed */
};

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
        kSilence, kSquare, kWave
    };

    enum class NoteRange
    {
        kFirst=0, kSecond=1, kThird=2
    };

    enum class NoteRangeName
    {
        kNone=-1, kLow=0, kMiddle=1, kHigh=2
    };

    DMFSampleMapper();

    SoundIndexType<MOD> Init(SoundIndexType<DMF> dmf_sound_index, SoundIndexType<MOD> starting_sound_index, const std::pair<Note, Note>& dmf_note_range);
    SoundIndexType<MOD> InitSilence();

    Note GetMODNote(const Note& dmf_note, NoteRange& mod_note_range) const;
    NoteRange GetMODNoteRange(const Note& dmf_note) const;
    SoundIndexType<MOD> GetMODSampleId(const Note& dmf_note) const;
    SoundIndexType<MOD> GetMODSampleId(NoteRange mod_note_range) const;
    unsigned GetMODSampleLength(NoteRange mod_note_range) const;
    NoteRange GetMODNoteRange(SoundIndexType<MOD> mod_sound_index) const;
    NoteRangeName GetMODNoteRangeName(NoteRange mod_note_range) const;

    int GetNumMODSamples() const { return num_mod_samples_; }
    SampleType GetSampleType() const { return sample_type_; }
    SoundIndexType<MOD> GetFirstMODSampleId() const { return mod_sound_indexes_[0]; }
    bool IsDownsamplingNeeded() const { return downsampling_needed_; }

private:
    SoundIndexType<DMF> dmf_sound_index_;
    std::array<SoundIndexType<MOD>, 3> mod_sound_indexes_; // Up to 3 MOD samples from one DMF sample
    std::array<unsigned, 3> mod_sample_lengths_;
    std::vector<Note> range_start_;
    int num_mod_samples_;
    SampleType sample_type_;
    bool downsampling_needed_;
    int mod_octave_shift_;
};

// Stores a MOD sample
struct Sample
{
    std::string name; // 22 characters
    SoundIndexType<MOD> id;
    unsigned length;
    int finetune;
    unsigned volume;
    unsigned repeat_offset;
    unsigned repeat_length;

    std::vector<int8_t> data;
};

using PriorityEffect = std::pair<mod::EffectPriority, d2m::Effect>;

} // namespace mod

///////////////////////////////////////////////////////////
// MOD primary classes
///////////////////////////////////////////////////////////

class MODException : public ModuleException
{
public:
    template<class T>
    MODException(Category category, T error_code, const std::string& args = "")
        : ModuleException(category, static_cast<int>(error_code), CreateErrorMessage(category, static_cast<int>(error_code), args)) {}

private:
    // Creates module-specific error message from an error code and string argument
    std::string CreateErrorMessage(Category category, int error_code, const std::string& arg);
};

class MODConversionOptions : public ConversionOptionsInterface<MODConversionOptions>
{
public:

    // Factory requires destructor to be public
    ~MODConversionOptions() = default;

    enum class OptionEnum
    {
        kAmigaFilter, kArpeggio, kPortamento, kPort2Note, kVibrato, kTempoType
    };

    enum class TempoType
    {
        kAccuracy, kEffectCompatibility
    };

    inline bool UseAmigaFilter() const { return GetOption(OptionEnum::kAmigaFilter).GetValue<bool>(); }
    inline bool AllowArpeggio() const { return GetOption(OptionEnum::kArpeggio).GetValue<bool>(); }
    inline bool AllowPortamento() const { return GetOption(OptionEnum::kPortamento).GetValue<bool>(); }
    inline bool AllowPort2Note() const { return GetOption(OptionEnum::kPort2Note).GetValue<bool>(); }
    inline bool AllowVibrato() const { return GetOption(OptionEnum::kVibrato).GetValue<bool>(); }
    inline TempoType GetTempoType() const { return TempoType{GetOption(OptionEnum::kTempoType).GetValueAsIndex()}; }

    inline bool AllowEffects() const { return AllowArpeggio() || AllowPortamento() || AllowPort2Note() || AllowVibrato(); }

private:

    // Only allow the Factory to construct this class
    friend class Builder<MODConversionOptions, ConversionOptionsBase>;

    MODConversionOptions() = default;
};

class MOD : public ModuleInterface<MOD>
{
public:

    // Factory requires destructor to be public
    ~MOD() = default;

    enum class ImportError {kSuccess=0};
    enum class ImportWarning {};

    enum class ExportError {kSuccess=0};
    enum class ExportWarning {};

    enum class ConvertError
    {
        kSuccess=0,
        kNotGameBoy,
        kTooManyPatternMatrixRows,
        kOver64RowPattern,
        kWrongChannelCount
    };

    enum class ConvertWarning
    {
        kNone=0,
        kPitchHigh,
        kTempoLow,
        kTempoHigh,
        kTempoLowCompat,
        kTempoHighCompat,
        kTempoAccuracy,
        kEffectIgnored,
        kWaveDownsample,
        kMultipleEffects,
        kLoopbackInaccuracy
    };

    static constexpr unsigned kVolumeMax = 64u; // Yes, there are 65 different values for the volume

private:

    // Only allow the Factory to construct this class
    friend class Builder<MOD, ModuleBase>;

    MOD();
    void CleanUp() {};

    using SampleMap = std::map<SoundIndexType<DMF>, mod::DMFSampleMapper>;

    // Module requirements:
    void ImportImpl(const std::string& filename) override;
    void ExportImpl(const std::string& filename) override;
    void ConvertImpl(const ModulePtr& input) override;
    size_t GenerateDataImpl(size_t data_flags) const override { return 1; }

    // Conversion from DMF:
    void ConvertFromDMF(const DMF& dmf);
    void DMFConvertSamples(const DMF& dmf, SampleMap& sample_map);
    void DMFConvertSampleData(const DMF& dmf, const SampleMap& sample_map);
    void DMFConvertPatterns(const DMF& dmf, const SampleMap& sample_map);
    mod::PriorityEffect DMFConvertEffects(ChannelStateReader<DMF>& state);
    Row<MOD> DMFConvertNote(ChannelStateReader<DMF>& state, mod::DMFSampleMapper::NoteRange& note_range, bool& set_sample, int& set_vol_if_not, const SampleMap& sample_map, mod::PriorityEffect& mod_effect);
    void ApplyEffects(std::array<Row<MOD>, 4>& row_data, const std::array<mod::PriorityEffect, 4>& mod_effect, std::vector<mod::PriorityEffect>& global_effects);

    void DMFConvertInitialBPM(const DMF& dmf, unsigned& tempo, unsigned& speed);

    // Export:
    void ExportModuleName(std::ofstream& fout) const;
    void ExportSampleInfo(std::ofstream& fout) const;
    void ExportModuleInfo(std::ofstream& fout) const;
    void ExportPatterns(std::ofstream& fout) const;
    void ExportSampleData(std::ofstream& fout) const;

    //////////// Temporaries used during DMF-->MOD conversion
    static constexpr bool using_setup_pattern_ = true; // Whether to use a pattern at the start of the module to set up the initial tempo and other stuff.

    //////////// MOD file info
    int8_t total_mod_samples_;
    std::map<SoundIndexType<MOD>, mod::Sample> samples_;
};

} // namespace d2m
