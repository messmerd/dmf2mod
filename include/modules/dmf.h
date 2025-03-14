/*
 * dmf.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Declares all classes used for Deflemask's DMF files.
 */

#pragma once

#include "core/module.h"
#include "utils/stream_reader.h"

#include <array>
#include <map>
#include <string>

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
	std::uint8_t id;
	std::string name;
	std::uint8_t channels;
};

} // namespace dmf

template<>
struct ModuleGlobalData<DMF> : public ModuleGlobalDataDefault<DataStorageType::kCOR>
{
	std::uint8_t dmf_format_version;
	dmf::System system;

	// Visual info
	std::uint8_t highlight_a_patterns;
	std::uint8_t highlight_b_patterns;

	// Module info
	std::uint8_t frames_mode;
	std::optional<std::uint16_t> custom_hz_value;
	std::uint16_t global_tick;
};

template<>
struct Row<DMF>
{
	NoteSlot note;
	std::int16_t volume;
	std::array<Effect, 4> effect; // Deflemask allows four effects columns per channel regardless of the system
	std::int16_t instrument;
};

template<>
struct ChannelMetadata<DMF>
{
	std::uint8_t effect_columns_count;
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
	struct Square { std::uint8_t id; operator std::uint8_t() const { return id; } };
	struct Wave { std::uint8_t id; operator std::uint8_t() const { return id; } };
	struct Noise { std::uint8_t id; operator std::uint8_t() const { return id; } }; // Placeholder

	using type = std::variant<None, Square, Wave, Noise>;
};

constexpr auto operator==(const SoundIndex<DMF>::Square& lhs, const SoundIndex<DMF>::Square& rhs) -> bool { return lhs.id == rhs.id; }
constexpr auto operator==(const SoundIndex<DMF>::Wave& lhs, const SoundIndex<DMF>::Wave& rhs) -> bool { return lhs.id == rhs.id; }
constexpr auto operator==(const SoundIndex<DMF>::Noise& lhs, const SoundIndex<DMF>::Noise& rhs) -> bool { return lhs.id == rhs.id; }

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
	std::uint8_t time_base, tick_time1, tick_time2;
};

struct FMOps
{
	// TODO: Use unions depending on DMF version?
	std::uint8_t am;
	std::uint8_t ar;     // Attack
	std::uint8_t dr;     // Decay?
	std::uint8_t mult;
	std::uint8_t rr;     // Release
	std::uint8_t sl;     // Sustain
	std::uint8_t tl;

	std::uint8_t dt2;
	std::uint8_t rs;
	std::uint8_t dt;
	std::uint8_t d2r;

	union
	{
		std::uint8_t ssg_mode;
		std::uint8_t egs; // EG-S in SMS OPLL / NES VRC7. 0 if OFF; 8 if ON.
	};

	std::uint8_t dam, dvb, egt, ksl, sus, vib, ws, ksr; // Exclusive to DMF version 18 (0x12) and older
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
			std::uint8_t vol_env_size, arp_env_size, duty_noise_env_size, wavetable_env_size;
			std::int32_t* vol_env_value;
			std::int32_t* arp_env_value;
			std::int32_t* duty_noise_env_value;
			std::int32_t* wavetable_env_value;
			std::int8_t vol_env_loop_pos, arp_env_loop_pos, duty_noise_env_loop_pos, wavetable_env_loop_pos;
			std::uint8_t arp_macro_mode;

			// Commodore 64 exclusive
			std::uint8_t c64_tri_wave_en, c64_saw_wave_en, c64_pulse_wave_en, c64_noise_wave_en,
				c64_attack, c64_decay, c64_sustain, c64_release, c64_pulse_width, c64_ring_mod_en,
				c64_sync_mod_en, c64_to_filter, c64_vol_macro_to_filter_cutoff_en, c64_use_filter_values_from_inst;
			std::uint8_t c64_filter_resonance, c64_filter_cutoff, c64_filter_high_pass, c64_filter_low_pass, c64_filter_ch2_off;

			// Game Boy exclusive
			std::uint8_t gb_env_vol, gb_env_dir, gb_env_len, gb_sound_len;
		} std;

		// FM Instruments
		struct
		{
			std::uint8_t num_operators;

			union
			{
				std::uint8_t alg;
				std::uint8_t sus; // SMS OPLL / NES VRC7 exclusive
			};

			std::uint8_t fb;
			std::uint8_t opll_preset; // SMS OPLL / NES VRC7 exclusive

			union
			{
				struct { std::uint8_t lfo, lfo2; };
				struct { std::uint8_t dc, dm; }; // SMS OPLL / NES VRC7 exclusive
			};

			std::array<FMOps, 4> ops;
		} fm;
	};
};

struct PCMSample
{
	std::uint32_t size;
	std::string name;
	std::uint8_t rate, pitch, amp, bits;
	std::uint32_t cut_start, cut_end;
	std::uint16_t* data;
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
	auto GetBPM() const -> double;

	auto GetSystem() const -> const dmf::System& { return GetGlobalData().system; }
	static auto SystemInfo(SystemType system_type) -> const dmf::System&;

	// TODO: Create a module-independent storage system for wavetables, PCM samples, instruments, etc.
	auto GetTotalWavetables() const -> std::uint8_t { return total_wavetables_; }
	auto GetWavetableValues() const -> std::uint32_t** { return wavetable_values_; }
	auto GetWavetableValue(unsigned wavetable, unsigned index) const -> std::uint32_t { return wavetable_values_[wavetable][index]; }

private:
	// Only allow the Factory to construct this class
	friend class Builder<DMF, ModuleBase>;

	DMF() = default;
	void CleanUp();

	void ImportImpl(const std::string& filename) override;
	void ExportImpl(const std::string& filename) override;
	void ConvertImpl(const ModulePtr& input) override;
	auto GenerateDataImpl(std::size_t data_flags) const -> std::size_t override;

	// Import helper class
	class Importer;

	dmf::ModuleInfo module_info_; // TODO: Eventually remove
	std::uint8_t total_instruments_ = 0;
	dmf::Instrument* instruments_ = nullptr;
	std::uint8_t total_wavetables_ = 0;
	std::uint32_t* wavetable_sizes_ = nullptr;
	std::uint32_t** wavetable_values_ = nullptr;
	std::uint8_t total_pcm_samples_ = 0;
	dmf::PCMSample* pcm_samples_ = nullptr;
};

} // namespace d2m
