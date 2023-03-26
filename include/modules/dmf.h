/*
 * dmf.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Declares all classes used for Deflemask's DMF files.
 */

#pragma once

#include "core/module.h"
#include "utils/stream_reader.h"

#include <string>
#include <map>
#include <array>

namespace d2m {

///////////////////////////////////////////////////////////
// Setup template specializations used by DMF
///////////////////////////////////////////////////////////

class DMF;

namespace dmf {

struct System
{
    enum class Type
    {
        kError = 0,
        kYMU759,
        kGenesis,
        kGenesis_CH3,
        kSMS,
        kGameBoy,
        kPCEngine,
        kNES,
        kC64_SID_8580,
        kC64_SID_6581,
        kArcade,
        kNeoGeo,
        kNeoGeo_CH2,
        kSMS_OPLL,
        kNES_VRC7,
        kNES_FDS
    };

    Type type;
    uint8_t id;
    std::string name;
    uint8_t channels;
};

} // namespace dmf

template<>
struct ModuleGlobalData<DMF> : public ModuleGlobalDataDefault<DataStorageType::kCOR>
{
    uint8_t dmf_format_version;
    dmf::System system;

    // Visual info
    uint8_t highlight_a_patterns;
    uint8_t highlight_b_patterns;

    // Module info
    uint8_t frames_mode;
    std::optional<uint16_t> custom_hz_value;
    uint16_t global_tick;
};

template<>
struct Row<DMF>
{
    NoteSlot note;
    int16_t volume;
    std::array<Effect, 4> effect; // Deflemask allows four effects columns per channel regardless of the system
    int16_t instrument;
};

template<>
struct ChannelMetadata<DMF>
{
    uint8_t effect_columns_count;
};

template<>
struct PatternMetadata<DMF>
{
    std::string name;
};

template<>
struct SoundIndex<DMF>
{
    enum Types { kNone, kSquare, kWave, kNoise /*Other options here*/ };

    using None = std::monostate;
    struct Square { uint8_t id; operator uint8_t() const { return id; } };
    struct Wave { uint8_t id; operator uint8_t() const { return id; } };
    struct Noise { uint8_t id; operator uint8_t() const { return id; } }; // Placeholder

    using type = std::variant<None, Square, Wave, Noise>;
};

inline constexpr auto operator==(const SoundIndex<DMF>::Square& lhs, const SoundIndex<DMF>::Square& rhs) -> bool { return lhs.id == rhs.id; }
inline constexpr auto operator==(const SoundIndex<DMF>::Wave& lhs, const SoundIndex<DMF>::Wave& rhs) -> bool { return lhs.id == rhs.id; }
inline constexpr auto operator==(const SoundIndex<DMF>::Noise& lhs, const SoundIndex<DMF>::Noise& rhs) -> bool { return lhs.id == rhs.id; }

///////////////////////////////////////////////////////////
// dmf namespace
///////////////////////////////////////////////////////////

namespace dmf {

//inline constexpr int kVolumeMax = 15; /* ??? */
inline constexpr int kGameBoyVolumeMax = 15;

// Custom dmf2mod internal effect codes (see effects.h)
namespace Effects
{
    enum
    {
        kArpTickSpeed = 1,
        kNoteSlideUp,
        kNoteSlideDown,
        kSetVibratoMode,
        kSetFineVibratoDepth,
        kSetFinetune,
        kSetSamplesBank,
        kSyncSignal,
        kSetGlobalFinetune,
        kGameBoySetWave,
        kGameBoySetNoisePolyCounterMode,
        kGameBoySetDutyCycle,
        kGameBoySetSweepTimeShift,
        kGameBoySetSweepDir
    };
}

struct ModuleInfo
{
    uint8_t time_base, tick_time1, tick_time2;
};

struct FMOps
{
    // TODO: Use unions depending on DMF version?
    uint8_t am;
    uint8_t ar;     // Attack
    uint8_t dr;     // Decay?
    uint8_t mult;
    uint8_t rr;     // Release
    uint8_t sl;     // Sustain
    uint8_t tl;

    uint8_t dt2;
    uint8_t rs;
    uint8_t dt;
    uint8_t d2r;

    union
    {
        uint8_t ssg_mode;
        uint8_t egs; // EG-S in SMS OPLL / NES VRC7. 0 if OFF; 8 if ON.
    };

    uint8_t dam, dvb, egt, ksl, sus, vib, ws, ksr; // Exclusive to DMF version 18 (0x12) and older
};

struct Instrument
{
    enum InstrumentMode
    {
        kInvalidMode = 0,
        kStandardMode,
        kFMMode
    };

    std::string name;
    InstrumentMode mode; // TODO: Use union depending on mode? Would save space

    union
    {
        // Standard Instruments
        struct
        {
            uint8_t vol_env_size, arp_env_size, duty_noise_env_size, wavetable_env_size;
            int32_t* vol_env_value;
            int32_t* arp_env_value;
            int32_t* duty_noise_env_value;
            int32_t* wavetable_env_value;
            int8_t vol_env_loop_pos, arp_env_loop_pos, duty_noise_env_loop_pos, wavetable_env_loop_pos;
            uint8_t arp_macro_mode;

            // Commodore 64 exclusive
            uint8_t c64_tri_wave_en, c64_saw_wave_en, c64_pulse_wave_en, c64_noise_wave_en,
                c64_attack, c64_decay, c64_sustain, c64_release, c64_pulse_width, c64_ring_mod_en,
                c64_sync_mod_en, c64_to_filter, c64_vol_macro_to_filter_cutoff_en, c64_use_filter_values_from_inst;
            uint8_t c64_filter_resonance, c64_filter_cutoff, c64_filter_high_pass, c64_filter_low_pass, c64_filter_ch2_off;

            // Game Boy exclusive
            uint8_t gb_env_vol, gb_env_dir, gb_env_len, gb_sound_len;
        } std;

        // FM Instruments
        struct
        {
            uint8_t num_operators;

            union
            {
                uint8_t alg;
                uint8_t sus; // SMS OPLL / NES VRC7 exclusive
            };

            uint8_t fb;
            uint8_t opll_preset; // SMS OPLL / NES VRC7 exclusive

            union
            {
                struct { uint8_t lfo, lfo2; };
                struct { uint8_t dc, dm; }; // SMS OPLL / NES VRC7 exclusive
            };

            std::array<FMOps, 4> ops;
        } fm;
    };
};

struct PCMSample
{
    uint32_t size;
    std::string name;
    uint8_t rate, pitch, amp, bits;
    uint32_t cut_start, cut_end;
    uint16_t* data;
};

// Deflemask Game Boy channels
namespace GameBoyChannel
{
    enum
    {
        kSquare1 = 0, kSquare2 = 1, kWave = 2, kNoise = 3
    };
}

} // namespace dmf

///////////////////////////////////////////////////////////
// DMF primary classes
///////////////////////////////////////////////////////////

class DMFConversionOptions : public ConversionOptionsInterface<DMFConversionOptions>
{
public:

    // Factory requires destructor to be public
    ~DMFConversionOptions() override = default;

private:

    // Only allow the Factory to construct this class
    friend class Builder<DMFConversionOptions, ConversionOptionsBase>;

    DMFConversionOptions() = default;
};

class DMF final : public ModuleInterface<DMF>
{
public:

    enum ImportError
    {
        kSuccess = 0,
        kUnspecifiedError
    };

    enum class ImportWarning {};
    enum class ExportError {};
    enum class ExportWarning {};
    enum class ConvertError {};
    enum class ConvertWarning {};

    using SystemType = dmf::System::Type;

    // Factory requires destructor to be public
    ~DMF() override;

    // Returns the initial BPM of the module
    void GetBPM(unsigned& numerator, unsigned& denominator) const;
    [[nodiscard]] auto GetBPM() const -> double;

    [[nodiscard]] auto GetSystem() const -> const dmf::System& { return GetGlobalData().system; }
    [[nodiscard]] static auto SystemInfo(SystemType system_type) -> const dmf::System&;

    // TODO: Create a module-independent storage system for wavetables, PCM samples, instruments, etc.
    [[nodiscard]] auto GetTotalWavetables() const -> uint8_t { return total_wavetables_; }
    [[nodiscard]] auto GetWavetableValues() const -> uint32_t** { return wavetable_values_; }
    [[nodiscard]] auto GetWavetableValue(unsigned wavetable, unsigned index) const -> uint32_t { return wavetable_values_[wavetable][index]; }

private:

    // Only allow the Factory to construct this class
    friend class Builder<DMF, ModuleBase>;

    DMF();
    void CleanUp();

    void ImportImpl(const std::string& filename) override;
    void ExportImpl(const std::string& filename) override;
    void ConvertImpl(const ModulePtr& input) override;
    [[nodiscard]] auto GenerateDataImpl(size_t data_flags) const -> size_t override;

    // Import helper class
    class Importer;

    dmf::ModuleInfo module_info_; // TODO: Eventually remove
    uint8_t total_instruments_;
    dmf::Instrument* instruments_;
    uint8_t total_wavetables_;
    uint32_t* wavetable_sizes_;
    uint32_t** wavetable_values_;
    uint8_t total_pcm_samples_;
    dmf::PCMSample* pcm_samples_;
};

} // namespace d2m
