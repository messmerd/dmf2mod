/*
    dmf.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares all classes used for Deflemask's DMF files.
*/

#pragma once

#include "core/module.h"
#include "utils/stream_reader.h"
#include <zstr/zstr.hpp>

#include <string>
#include <map>
#include <array>

namespace d2m {

///////////////////////////////////////////////////////////
// Setup template specializations used by DMF
///////////////////////////////////////////////////////////

class DMF;

template<>
struct ModuleGlobalData<DMF> : public ModuleGlobalDataDefault<DataStorageType::kCOR> {};

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

inline constexpr bool operator==(const SoundIndex<DMF>::Square& lhs, const SoundIndex<DMF>::Square& rhs) { return lhs.id == rhs.id; }
inline constexpr bool operator==(const SoundIndex<DMF>::Wave& lhs, const SoundIndex<DMF>::Wave& rhs) { return lhs.id == rhs.id; }
inline constexpr bool operator==(const SoundIndex<DMF>::Noise& lhs, const SoundIndex<DMF>::Noise& rhs) { return lhs.id == rhs.id; }

///////////////////////////////////////////////////////////
// dmf namespace
///////////////////////////////////////////////////////////

namespace dmf {

inline constexpr int kDMFNoInstrument = -1;
inline constexpr int kDMFNoVolume = -1;
inline constexpr int kDMFVolumeMax = 15; /* ??? */
inline constexpr int kDMFGameBoyVolumeMax = 15;
inline constexpr int kDMFNoEffectVal = -1;

// Effect codes used by the DMF format:
namespace EffectCode
{
    enum
    {
        kNoEffect=-1,
        kArp                    =0x0,
        kPortUp                 =0x1,
        kPortDown               =0x2,
        kPort2Note              =0x3,
        kVibrato                =0x4,
        kPort2NoteVolSlide      =0x5,
        kVibratoVolSlide        =0x6,
        kTremolo                =0x7,
        kPanning                =0x8,
        kSetSpeedVal1           =0x9,
        kVolSlide               =0xA,
        kPosJump                =0xB,
        kRetrig                 =0xC,
        kPatBreak               =0xD,
        kArpTickSpeed           =0xE0,
        kNoteSlideUp            =0xE1,
        kNoteSlideDown          =0xE2,
        kSetVibratoMode         =0xE3,
        kSetFineVibratoDepth    =0xE4,
        kSetFinetune            =0xE5,
        kSetSamplesBank         =0xEB,
        kNoteCut                =0xEC,
        kNoteDelay              =0xED,
        kSyncSignal             =0xEE,
        kSetGlobalFinetune      =0xEF,
        kSetSpeedVal2           =0xF,

        // Game Boy exclusive
        kGameBoySetWave                 =0x10,
        kGameBoySetNoisePolyCounterMode =0x11,
        kGameBoySetDutyCycle            =0x12,
        kGameBoySetSweepTimeShift       =0x13,
        kGameBoySetSweepDir             =0x14

        // TODO: Add enums for effects exclusive to the rest of Deflemask's systems.
    };
}

// Custom dmf2mod internal effect codes (see effects.h)
namespace Effects
{
    enum
    {
        kArpTickSpeed=1,
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


struct System
{
    enum class Type
    {
        kError=0, kYMU759, kGenesis, kGenesis_CH3, kSMS, kGameBoy,
        kPCEngine, kNES, kC64_SID_8580, kC64_SID_6581, kArcade,
        kNeoGeo, kNeoGeo_CH2, kSMS_OPLL, kNES_VRC7, kNES_FDS
    };

    Type type;
    uint8_t id;
    std::string name;
    uint8_t channels;

    System() = default;
    System(Type type, uint8_t id, std::string name, uint8_t channels)
        : type(type), id(id), name(name), channels(channels) {}
};

struct VisualInfo
{
    uint8_t highlight_a_patterns;
    uint8_t highlight_b_patterns;
};

struct ModuleInfo
{
    uint8_t time_base, tick_time1, tick_time2, frames_mode, using_custom_hz, custom_hz_value1, custom_hz_value2, custom_hz_value3;
    uint32_t total_rows_per_pattern; // TODO: Should remove this eventually (duplicate w/ ModuleData)
    uint8_t total_rows_in_pattern_matrix; // (orders) TODO: Should remove this eventually (duplicate w/ ModuleData)
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
        kInvalidMode=0,
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
            int32_t *vol_env_value, *arp_env_value, *duty_noise_env_value, *wavetable_env_value;
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

            union {
                struct {
                    uint8_t lfo, lfo2;
                };
                struct {
                    uint8_t dc, dm; // SMS OPLL / NES VRC7 exclusive
                };
            };

            FMOps ops[4];
        } fm;
    };
};

struct PCMSample
{
    uint32_t size;
    std::string name;
    uint8_t rate, pitch, amp, bits;
    uint16_t* data;
};

// Deflemask Game Boy channels
namespace GameBoyChannel
{
    enum
    {
        kSquare1=0, kSquare2=1, kWave=2, kNoise=3
    };
}

} // namespace dmf

///////////////////////////////////////////////////////////
// DMF primary classes
///////////////////////////////////////////////////////////

class DMFConversionOptions : public ConversionOptionsInterface<DMFConversionOptions>
{
private:

    // Only allow the Factory to construct this class
    friend class Builder<DMFConversionOptions, ConversionOptionsBase>;

    DMFConversionOptions() = default;

public:

    // Factory requires destructor to be public
    ~DMFConversionOptions() = default;
};

class DMF : public ModuleInterface<DMF>
{
public:

    using options_t = DMFConversionOptions;

    enum ImportError
    {
        kSuccess=0,
        kUnspecifiedError
    };

    enum class ImportWarning {};
    enum class ExportError {};
    enum class ExportWarning {};
    enum class ConvertError {};
    enum class ConvertWarning {};

    using SystemType = dmf::System::Type;

    static const dmf::System& Systems(SystemType system_type);

private:

    // Only allow the Factory to construct this class
    friend class Builder<DMF, ModuleBase>;

    DMF();
    void CleanUp();

public:

    // Factory requires destructor to be public
    ~DMF();

    // Returns the initial BPM of the module
    void GetBPM(unsigned& numerator, unsigned& denominator) const;
    double GetBPM() const;

    const dmf::System& GetSystem() const { return system_; }

    uint8_t GetTotalWavetables() const { return total_wavetables_; }
    uint32_t** GetWavetableValues() const { return wavetable_values_; }
    uint32_t GetWavetableValue(unsigned wavetable, unsigned index) const { return wavetable_values_[wavetable][index]; }

private:

    void ImportImpl(const std::string& filename) override;
    void ExportImpl(const std::string& filename) override;
    void ConvertImpl(const ModulePtr& input) override;
    size_t GenerateDataImpl(size_t data_flags) const override;

    using Reader = StreamReader<zstr::ifstream, Endianness::kLittle>;

    dmf::System GetSystem(uint8_t system_byte) const;
    void LoadVisualInfo(Reader& fin);
    void LoadModuleInfo(Reader& fin);
    void LoadPatternMatrixValues(Reader& fin);
    void LoadInstrumentsData(Reader& fin);
    dmf::Instrument LoadInstrument(Reader& fin, SystemType system_type);
    void LoadWavetablesData(Reader& fin);
    void LoadPatternsData(Reader& fin);
    Row<DMF> LoadPatternRow(Reader& fin, uint8_t effect_columns_count);
    void LoadPCMSamplesData(Reader& fin);
    dmf::PCMSample LoadPCMSample(Reader& fin);

private:
    uint8_t file_version_;
    dmf::System system_;
    dmf::VisualInfo visual_info_;
    dmf::ModuleInfo module_info_;
    uint8_t total_instruments_;
    dmf::Instrument* instruments_;
    uint8_t total_wavetables_;
    uint32_t* wavetable_sizes_;
    uint32_t** wavetable_values_;
    uint8_t total_pcm_samples_;
    dmf::PCMSample* pcm_samples_;
};

} // namespace d2m
