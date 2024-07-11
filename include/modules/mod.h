/*
 * mod.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Declares all classes used for ProTracker's MOD files.
 */

#pragma once

#include "core/module.h"

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
    bool made_with_dmf2mod;
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
        kSetSampleOffset = 1,
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

    std::vector<std::int8_t> data;
};

} // namespace mod

///////////////////////////////////////////////////////////
// MOD primary classes
///////////////////////////////////////////////////////////

class MODException : public ModuleException
{
public:
    template<class T>
    MODException(Category category, T error_code, std::string_view args = "")
        : ModuleException(category, static_cast<int>(error_code), CreateErrorMessage(category, static_cast<int>(error_code), args)) {}

private:
    // Creates module-specific error message from an error code and string argument
    static auto CreateErrorMessage(Category category, int error_code, std::string_view arg) -> std::string;
};

class MODConversionOptions final : public ConversionOptionsInterface<MODConversionOptions>
{
public:

    // Factory requires destructor to be public
    ~MODConversionOptions() override = default;

    enum class OptionEnum
    {
        kArpeggio, kPortamento, kPort2Note, kVibrato, kTempoType
    };

    enum class TempoType
    {
        kAccuracy, kEffectCompatibility
    };

    auto AllowArpeggio() const -> bool { return GetOption(OptionEnum::kArpeggio).GetValue<bool>(); }
    auto AllowPortamento() const -> bool { return GetOption(OptionEnum::kPortamento).GetValue<bool>(); }
    auto AllowPort2Note() const -> bool { return GetOption(OptionEnum::kPort2Note).GetValue<bool>(); }
    auto AllowVibrato() const -> bool { return GetOption(OptionEnum::kVibrato).GetValue<bool>(); }
    auto GetTempoType() const -> TempoType { return TempoType{GetOption(OptionEnum::kTempoType).GetValueAsIndex()}; }

    auto AllowEffects() const -> bool { return AllowArpeggio() || AllowPortamento() || AllowPort2Note() || AllowVibrato(); }

private:

    // Only allow the Factory to construct this class
    friend class Builder<MODConversionOptions, ConversionOptionsBase>;

    MODConversionOptions() = default;
};

class MOD final : public ModuleInterface<MOD>
{
public:

    // Factory requires destructor to be public
    ~MOD() override = default;

    enum class ImportError { kSuccess = 0 };
    enum class ImportWarning {};

    enum class ExportError { kSuccess = 0 };
    enum class ExportWarning {};

    enum class ConvertError
    {
        kSuccess = 0,
        kNotGameBoy,
        kTooManyPatternMatrixRows,
        kOver64RowPattern,
        kWrongChannelCount
    };

    enum class ConvertWarning
    {
        kNone = 0,
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

    MOD() = default;

    // Module requirements:
    void ImportImpl(const std::string& filename) override;
    void ExportImpl(const std::string& filename) override;
    void ConvertImpl(const ModulePtr& input) override;
    auto GenerateDataImpl(std::size_t data_flags) const -> std::size_t override { return 1; }

    // DMF -> MOD conversion
    class DMFConverter;

    // Export helpers:
    void ExportModuleName(std::ofstream& fout) const;
    void ExportSampleInfo(std::ofstream& fout) const;
    void ExportModuleInfo(std::ofstream& fout) const;
    void ExportPatterns(std::ofstream& fout) const;
    void ExportSampleData(std::ofstream& fout) const;

    // MOD file info:
    std::int8_t total_mod_samples_ = 0;
    std::map<SoundIndexType<MOD>, mod::Sample> samples_;
};

} // namespace d2m
