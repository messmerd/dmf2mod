/*
    dmf.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines all classes used for Deflemask's DMF files.

    DMF file support was written according to the specs at
    http://www.deflemask.com/DMF_SPECS.txt.
*/

#include "modules/dmf.h"
#include "utils/utils.h"
#include "utils/hash.h"

// For inflating DMF files
#include "zlib/zlib.h"
#include "zlib/zconf.h"
#include "zstr/zstr.hpp"

// Compile time math
#include "gcem.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <map>
#include <cmath>
#include <array>
#include <unordered_set>
#include <optional>

using namespace d2m;
using namespace d2m::dmf;
// DO NOT use any module namespace other than d2m::dmf

static constexpr uint8_t kDMFFileVersionMin = 17; // DMF files as old as version 17 (0x11) are supported
static constexpr uint8_t kDMFFileVersionMax = 26; // DMF files as new as version 26 (0x1a) are supported

class DMF::Importer
{
public:
    using Reader = StreamReader<zstr::ifstream, Endianness::kLittle>;
    Importer(DMF* parent) : parent_(parent) {}

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
    DMF* parent_;
};

// Effect codes used by the DMF format:
namespace d2m::dmf::EffectCode
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

// Information about all the systems Deflemask supports
static const std::map<DMF::SystemType, System> kDMFSystems =
{
    {DMF::SystemType::kError, System(DMF::SystemType::kError, 0x00, "ERROR", 0)},
    {DMF::SystemType::kYMU759, System(DMF::SystemType::kYMU759, 0x01, "YMU759", 17)}, // Removed since DMF version 19 (0x13)
    {DMF::SystemType::kGenesis, System(DMF::SystemType::kGenesis, 0x02, "Genesis", 10)},
    {DMF::SystemType::kGenesis_CH3, System(DMF::SystemType::kGenesis_CH3, 0x42, "Genesis (Ext. CH3)", 13)},
    {DMF::SystemType::kSMS, System(DMF::SystemType::kSMS, 0x03, "SMS", 4)},
    {DMF::SystemType::kSMS_OPLL, System(DMF::SystemType::kSMS_OPLL, 0x43, "SMS + OPLL", 13)},
    {DMF::SystemType::kGameBoy, System(DMF::SystemType::kGameBoy, 0x04, "Game Boy", 4)},
    {DMF::SystemType::kPCEngine, System(DMF::SystemType::kPCEngine, 0x05, "PC Engine", 6)},
    {DMF::SystemType::kNES, System(DMF::SystemType::kNES, 0x06, "NES", 5)},
    {DMF::SystemType::kNES_VRC7, System(DMF::SystemType::kNES_VRC7, 0x46, "NES + VRC7", 11)},
    {DMF::SystemType::kC64_SID_8580, System(DMF::SystemType::kC64_SID_8580, 0x07, "C64 (SID 8580)", 3)},
    {DMF::SystemType::kC64_SID_6581, System(DMF::SystemType::kC64_SID_6581, 0x47, "C64 (SID 6581)", 3)},
    {DMF::SystemType::kArcade, System(DMF::SystemType::kArcade, 0x08, "Arcade", 13)},
    {DMF::SystemType::kNeoGeo, System(DMF::SystemType::kNeoGeo, 0x09, "Neo Geo", 13)},
    {DMF::SystemType::kNeoGeo_CH2, System(DMF::SystemType::kNeoGeo_CH2, 0x49, "Neo Geo (Ext. CH2)", 16)},
    {DMF::SystemType::kNES_FDS, System(DMF::SystemType::kNES_FDS, 0x86, "NES + FDS", 6)}
};

const System& DMF::Systems(DMF::SystemType system_type) { return kDMFSystems.at(system_type); }

static constexpr auto kPeriodTable = []() constexpr
{
    std::array<double, 12 * 9> ret{};
    for (int i = 0; i < 12 * 9; ++i)
    {
        ret[i] = 262144.0 / (27.5 * gcem::pow(2, (i + 3) / 12.0));
    }
    return ret;
}();

static constexpr double GetPeriod(Note note)
{
    assert(static_cast<uint16_t>(note.pitch) < 12 && note.octave < 9);
    return kPeriodTable[static_cast<uint16_t>(note.pitch) + 12*note.octave];
}

DMF::DMF()
{
    // Initialize pointers to nullptr to prevent segfault when freeing memory if the import fails:
    instruments_ = nullptr;
    wavetable_sizes_ = nullptr;
    wavetable_values_ = nullptr;
    pcm_samples_ = nullptr;

    importer_pimpl_ = std::make_unique<DMF::Importer>(this);
}

DMF::~DMF()
{
    CleanUp();
}

void DMF::CleanUp()
{
    // TODO: This function is old and nearly unchanged from when dmf2mod was a C program.
    //       Need to use RAII instead.

    // Free memory allocated for members
    if (instruments_)
    {
        for (int i = 0; i < total_instruments_; i++) 
        {
            if (instruments_[i].mode == Instrument::kStandardMode)
            {
                delete[] instruments_[i].std.arp_env_value;
                delete[] instruments_[i].std.duty_noise_env_value;
                delete[] instruments_[i].std.vol_env_value;
                delete[] instruments_[i].std.wavetable_env_value;
            }
        }
        delete[] instruments_;
        instruments_ = nullptr;
    }

    delete[] wavetable_sizes_;
    wavetable_sizes_ = nullptr;

    if (wavetable_values_)
    {
        for (int i = 0; i < total_wavetables_; i++)
        {
            delete[] wavetable_values_[i];
            wavetable_values_[i] = nullptr;
        }
        delete[] wavetable_values_;
        wavetable_values_ = nullptr;
    }

    if (pcm_samples_)
    {
        for (int sample = 0; sample < total_pcm_samples_; sample++) 
        {
            delete[] pcm_samples_[sample].data;
        }
        delete[] pcm_samples_;
        pcm_samples_ = nullptr;
    }

    GetData().CleanUp();
    GetGeneratedDataMut()->ClearAll();
}

void DMF::ImportImpl(const std::string& filename)
{
    CleanUp();

    const bool verbose = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kVerbose).GetValue<bool>();

    if (verbose)
        std::cout << "Starting to import the DMF file...\n";

    if (Utils::GetTypeFromFilename(filename) != ModuleType::kDMF)
    {
        throw ModuleException(ModuleException::Category::kImport, DMF::ImportError::kUnspecifiedError, "Input file has the wrong file extension.\nPlease use a DMF file.");
    }

    if (verbose)
        std::cout << "DMF Filename: " << filename << "\n";

    Importer::Reader fin{filename, std::ios_base::binary};
    if (fin.stream().fail())
    {
        throw ModuleException(ModuleException::Category::kImport, DMF::ImportError::kUnspecifiedError, "Failed to open DMF file.");
    }

    ///////////////// FORMAT FLAGS

    // Check header
    if (fin.ReadStr(16) != ".DelekDefleMask.")
    {
        throw ModuleException(ModuleException::Category::kImport, DMF::ImportError::kUnspecifiedError, "DMF format header is bad.");
    }

    file_version_ = fin.ReadInt();
    if (file_version_ < kDMFFileVersionMin || file_version_ > kDMFFileVersionMax)
    {
        const bool too_high = file_version_ > kDMFFileVersionMax;
        const int extreme_version = too_high ? kDMFFileVersionMax : kDMFFileVersionMin;

        std::stringstream stream;
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << extreme_version;
        std::string hex = stream.str();

        std::string error_msg = "Deflemask file version must be " + std::to_string(extreme_version) + " (" + hex + ") or ";
        error_msg += too_high ? "lower.\n" : "higher.\n";
        
        stream.clear();
        stream.str("");
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << (int)file_version_;
        hex = stream.str();

        error_msg += "The given DMF file is version " + std::to_string(file_version_) + " (" + hex + ").\n";
        if (too_high)
            error_msg += "       Dmf2mod needs to be updated to support this newer version.";
        else
            error_msg += "       You can convert older DMF files to a supported version by opening them in a newer version of DefleMask and then saving them.";

        throw ModuleException(ModuleException::Category::kImport, DMF::ImportError::kUnspecifiedError, error_msg);
    }
    else if (verbose)
    {
        std::stringstream stream;
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << (int)file_version_;
        std::string hex = stream.str();

        std::cout << "DMF version " + std::to_string(file_version_) + " (" + hex + ")\n";
    }

    ///////////////// SYSTEM SET

    system_ = GetSystem(fin.ReadInt());

    if (verbose)
        std::cout << "System: " << system_.name << " (channels: " << std::to_string(system_.channels) << ")\n";

    ///////////////// VISUAL INFORMATION
    importer_pimpl_->LoadVisualInfo(fin);
    if (verbose)
    {
        std::cout << "Title: " << GetTitle() << "\n";
        std::cout << "Author: " << GetAuthor() << "\n";
        std::cout << "Loaded visual information." << "\n";
    }

    ///////////////// MODULE INFORMATION
    importer_pimpl_->LoadModuleInfo(fin);
    if (verbose)
        std::cout << "Loaded module information.\n";

    ///////////////// PATTERN MATRIX VALUES
    importer_pimpl_->LoadPatternMatrixValues(fin);
    if (verbose)
        std::cout << "Loaded pattern matrix values.\n";

    ///////////////// INSTRUMENTS DATA
    importer_pimpl_->LoadInstrumentsData(fin);
    if (verbose)
        std::cout << "Loaded instruments.\n";

    ///////////////// WAVETABLES DATA
    importer_pimpl_->LoadWavetablesData(fin);
    if (verbose)
        std::cout << "Loaded " << std::to_string(total_wavetables_) << " wavetable(s).\n";

    ///////////////// PATTERNS DATA
    importer_pimpl_->LoadPatternsData(fin);
    if (verbose)
        std::cout << "Loaded patterns.\n";

    ///////////////// PCM SAMPLES DATA
    importer_pimpl_->LoadPCMSamplesData(fin);
    if (verbose)
        std::cout << "Loaded PCM samples.\n";

    if (verbose)
        std::cout << "Done importing DMF file.\n\n";
}

void DMF::ExportImpl(const std::string& filename)
{
    // Not implemented
    throw NotImplementedException{};
}

void DMF::ConvertImpl(const ModulePtr& input)
{
    // Not implemented
    throw NotImplementedException{};
}

System DMF::GetSystem(uint8_t system_byte) const
{
    for (const auto& map_pair : kDMFSystems)
    {
        if (map_pair.second.id == system_byte)
            return map_pair.second;
    }
    return kDMFSystems.at(DMF::SystemType::kError); // Error: System byte invalid
}

void DMF::Importer::LoadVisualInfo(Reader& fin)
{
    parent_->GetGlobalData().title = fin.ReadPStr();
    parent_->GetGlobalData().author = fin.ReadPStr();
    parent_->visual_info_.highlight_a_patterns = fin.ReadInt();
    parent_->visual_info_.highlight_b_patterns = fin.ReadInt();
}

void DMF::Importer::LoadModuleInfo(Reader& fin)
{
    auto& module_info = parent_->module_info_;
    module_info.time_base = fin.ReadInt() + 1;
    module_info.tick_time1 = fin.ReadInt();
    module_info.tick_time2 = fin.ReadInt();
    module_info.frames_mode = fin.ReadInt();
    module_info.using_custom_hz = fin.ReadInt();
    module_info.custom_hz_value1 = fin.ReadInt();
    module_info.custom_hz_value2 = fin.ReadInt();
    module_info.custom_hz_value3 = fin.ReadInt();

    if (parent_->file_version_ >= 24) // DMF version 24 (0x18) and newer.
    {
        // Newer versions read 4 bytes here
        module_info.total_rows_per_pattern = fin.ReadInt<false, 4>();
    }
    else // DMF version 23 (0x17) and older. WARNING: I don't have the specs for version 23 (0x17), so this may be wrong.
    {
        // Earlier versions such as 22 (0x16) only read one byte here
        module_info.total_rows_per_pattern = fin.ReadInt();
    }

    module_info.total_rows_in_pattern_matrix = fin.ReadInt();

    // Prior to Deflemask Version 0.11.1, arpeggio tick speed was stored here
    // I don't have the specs for DMF version 20 (0x14), but based on a real DMF file of that version, it is the first DMF version 
    //      to NOT contain the arpeggio tick speed byte.
    if (parent_->file_version_ <= 19) // DMF version 19 (0x13) and older
    {
        fin.ReadInt(); // arpTickSpeed: Discard for now
    }
}

void DMF::Importer::LoadPatternMatrixValues(Reader& fin)
{
    auto& module_data = parent_->GetData();
    module_data.AllocatePatternMatrix(
        parent_->system_.channels,
        parent_->module_info_.total_rows_in_pattern_matrix,
        parent_->module_info_.total_rows_per_pattern);

    std::map<std::pair<ChannelIndex, PatternIndex>, std::string> channel_pattern_id_to_pattern_name_map;

    for (ChannelIndex channel = 0; channel < module_data.GetNumChannels(); ++channel)
    {
        for (OrderIndex order = 0; order < module_data.GetNumOrders(); ++order)
        {
            const PatternIndex pattern_id = fin.ReadInt();
            module_data.SetPatternId(channel, order, pattern_id);

            // Version 1.1 introduces pattern names
            if (parent_->file_version_ >= 25) // DMF version 25 (0x19) and newer
            {
                std::string pattern_name = fin.ReadPStr();
                if (pattern_name.size() > 0)
                {
                    channel_pattern_id_to_pattern_name_map[{channel, pattern_id}] = std::move(pattern_name);
                }
            }
        }
    }

    module_data.AllocateChannels();
    module_data.AllocatePatterns();

    // Pattern metadata must be set AFTER AllocatePatterns is called
    for (auto& [channel_pattern_id, pattern_name] : channel_pattern_id_to_pattern_name_map)
    {
        module_data.SetPatternMetadata(channel_pattern_id.first, channel_pattern_id.second, {std::move(pattern_name)});
    }
}

void DMF::Importer::LoadInstrumentsData(Reader& fin)
{
    parent_->total_instruments_ = fin.ReadInt();
    parent_->instruments_ = new Instrument[parent_->total_instruments_];

    for (int i = 0; i < parent_->total_instruments_; i++)
    {
        parent_->instruments_[i] = LoadInstrument(fin, parent_->system_.type);
    }
}

Instrument DMF::Importer::LoadInstrument(Reader& fin, DMF::SystemType system_type)
{
    Instrument inst{};

    inst.name = fin.ReadPStr();

    // Get instrument mode (Standard or FM)
    inst.mode = Instrument::kInvalidMode;
    switch (fin.ReadInt())
    {
        case 0:
            inst.mode = Instrument::kStandardMode; break;
        case 1:
            inst.mode = Instrument::kFMMode; break;
        default:
            throw ModuleException(Status::Category::kImport, DMF::ImportError::kUnspecifiedError, "Invalid instrument mode");
    }

    // Now we can import the instrument depending on the mode (Standard/FM)

    if (inst.mode == Instrument::kStandardMode)
    {
        if (parent_->file_version_ <= 17) // DMF version 17 (0x11) or older
        {
            // Volume macro
            inst.std.vol_env_size = fin.ReadInt();
            inst.std.vol_env_value = new int32_t[inst.std.vol_env_size];

            for (int i = 0; i < inst.std.vol_env_size; i++)
            {
                // 4 bytes, little-endian
                inst.std.vol_env_value[i] = fin.ReadInt<true, 4>();
            }

            // Always get envelope loop position byte regardless of envelope size
            inst.std.vol_env_loop_pos = fin.ReadInt();
        }
        else if (system_type != DMF::SystemType::kGameBoy) // Not a Game Boy and DMF version 18 (0x12) or newer
        {
            // Volume macro
            inst.std.vol_env_size = fin.ReadInt();
            inst.std.vol_env_value = new int32_t[inst.std.vol_env_size];

            for (int i = 0; i < inst.std.vol_env_size; i++)
            {
                // 4 bytes, little-endian
                inst.std.vol_env_value[i] = fin.ReadInt<true, 4>();
            }

            if (inst.std.vol_env_size > 0)
                inst.std.vol_env_loop_pos = fin.ReadInt();
        }

        // Arpeggio macro
        inst.std.arp_env_size = fin.ReadInt();
        inst.std.arp_env_value = new int32_t[inst.std.arp_env_size];

        for (int i = 0; i < inst.std.arp_env_size; i++)
        {
            // 4 bytes, little-endian
            inst.std.arp_env_value[i] = fin.ReadInt<true, 4>();
        }

        if (inst.std.arp_env_size > 0 || parent_->file_version_ <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.std.arp_env_loop_pos = fin.ReadInt();
        
        inst.std.arp_macro_mode = fin.ReadInt();

        // Duty/Noise macro
        inst.std.duty_noise_env_size = fin.ReadInt();
        inst.std.duty_noise_env_value = new int32_t[inst.std.duty_noise_env_size];

        for (int i = 0; i < inst.std.duty_noise_env_size; i++)
        {
            // 4 bytes, little-endian
            inst.std.duty_noise_env_value[i] = fin.ReadInt<true, 4>();
        }

        if (inst.std.duty_noise_env_size > 0 || parent_->file_version_ <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.std.duty_noise_env_loop_pos = fin.ReadInt();

        // Wavetable macro
        inst.std.wavetable_env_size = fin.ReadInt();
        inst.std.wavetable_env_value = new int32_t[inst.std.wavetable_env_size];

        for (int i = 0; i < inst.std.wavetable_env_size; i++)
        {
            // 4 bytes, little-endian
            inst.std.wavetable_env_value[i] = fin.ReadInt<true, 4>();
        }

        if (inst.std.wavetable_env_size > 0 || parent_->file_version_ <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.std.wavetable_env_loop_pos = fin.ReadInt();

        // Per system data
        if (system_type == DMF::SystemType::kC64_SID_8580 || system_type == DMF::SystemType::kC64_SID_6581) // Using Commodore 64
        {
            inst.std.c64_tri_wave_en = fin.ReadInt();
            inst.std.c64_saw_wave_en = fin.ReadInt();
            inst.std.c64_pulse_wave_en = fin.ReadInt();
            inst.std.c64_noise_wave_en = fin.ReadInt();
            inst.std.c64_attack = fin.ReadInt();
            inst.std.c64_decay = fin.ReadInt();
            inst.std.c64_sustain = fin.ReadInt();
            inst.std.c64_release = fin.ReadInt();
            inst.std.c64_pulse_width = fin.ReadInt();
            inst.std.c64_ring_mod_en = fin.ReadInt();
            inst.std.c64_sync_mod_en = fin.ReadInt();
            inst.std.c64_to_filter = fin.ReadInt();
            inst.std.c64_vol_macro_to_filter_cutoff_en = fin.ReadInt();
            inst.std.c64_use_filter_values_from_inst = fin.ReadInt();

            // Filter globals
            inst.std.c64_filter_resonance = fin.ReadInt();
            inst.std.c64_filter_cutoff = fin.ReadInt();
            inst.std.c64_filter_high_pass = fin.ReadInt();
            inst.std.c64_filter_low_pass = fin.ReadInt();
            inst.std.c64_filter_ch2_off = fin.ReadInt();
        }
        else if (system_type == DMF::SystemType::kGameBoy && parent_->file_version_ >= 18) // Using Game Boy and DMF version is 18 or newer
        {
            inst.std.gb_env_vol = fin.ReadInt();
            inst.std.gb_env_dir = fin.ReadInt();
            inst.std.gb_env_len = fin.ReadInt();
            inst.std.gb_sound_len = fin.ReadInt();
        }
    }
    else if (inst.mode == Instrument::kFMMode)
    {
        // Initialize to nullptr just in case
        inst.std.vol_env_value = nullptr;
        inst.std.arp_env_value = nullptr;
        inst.std.duty_noise_env_value = nullptr;
        inst.std.wavetable_env_value = nullptr;

        if (parent_->file_version_ > 18) // Newer than DMF version 18 (0x12)
        {
            if (system_type == DMF::SystemType::kSMS_OPLL || system_type == DMF::SystemType::kNES_VRC7)
            {
                inst.fm.sus = fin.ReadInt();
                inst.fm.fb = fin.ReadInt();
                inst.fm.dc = fin.ReadInt();
                inst.fm.dm = fin.ReadInt();
            }
            else
            {
                inst.fm.alg = fin.ReadInt();
                inst.fm.fb = fin.ReadInt();
                inst.fm.lfo = fin.ReadInt();
                inst.fm.lfo2 = fin.ReadInt();
            }

            inst.fm.num_operators = 4;
        }
        else
        {
            inst.fm.alg = fin.ReadInt();
            fin.ReadInt(); // Reserved byte (must be 0)
            inst.fm.fb = fin.ReadInt();
            fin.ReadInt(); // Reserved byte (must be 0)
            inst.fm.lfo = fin.ReadInt();
            fin.ReadInt(); // Reserved byte (must be 0)

            const bool totalOperatorsBool = fin.ReadInt();
            inst.fm.num_operators = totalOperatorsBool ? 4 : 2;

            inst.fm.lfo2 = fin.ReadInt();
        }

        for (int i = 0; i < inst.fm.num_operators; i++)
        {
            if (parent_->file_version_ > 18) // Newer than DMF version 18 (0x12)
            {
                inst.fm.ops[i].am = fin.ReadInt();
                inst.fm.ops[i].ar = fin.ReadInt();
                inst.fm.ops[i].dr = fin.ReadInt();
                inst.fm.ops[i].mult = fin.ReadInt();
                inst.fm.ops[i].rr = fin.ReadInt();
                inst.fm.ops[i].sl = fin.ReadInt();
                inst.fm.ops[i].tl = fin.ReadInt();

                if (system_type == DMF::SystemType::kSMS_OPLL || system_type == DMF::SystemType::kNES_VRC7)
                {
                    const uint8_t opll_preset = fin.ReadInt();
                    if (i == 0)
                        inst.fm.opll_preset = opll_preset;

                    inst.fm.ops[i].ksr = fin.ReadInt();
                    inst.fm.ops[i].vib = fin.ReadInt();
                    inst.fm.ops[i].ksl = fin.ReadInt();
                    inst.fm.ops[i].egs = fin.ReadInt(); // EG-S in Deflemask. 0 if OFF; 8 if ON.
                }
                else
                {
                    inst.fm.ops[i].dt2 = fin.ReadInt();
                    inst.fm.ops[i].rs = fin.ReadInt();
                    inst.fm.ops[i].dt = fin.ReadInt();
                    inst.fm.ops[i].d2r = fin.ReadInt();
                    inst.fm.ops[i].ssg_mode = fin.ReadInt();
                }
            }
            else // DMF version 17 (0x11) or older
            {
                inst.fm.ops[i].am = fin.ReadInt();
                inst.fm.ops[i].ar = fin.ReadInt();
                inst.fm.ops[i].dam = fin.ReadInt();
                inst.fm.ops[i].dr = fin.ReadInt();
                inst.fm.ops[i].dvb = fin.ReadInt();
                inst.fm.ops[i].egt = fin.ReadInt();
                inst.fm.ops[i].ksl = fin.ReadInt();
                inst.fm.ops[i].mult = fin.ReadInt();
                inst.fm.ops[i].rr = fin.ReadInt();
                inst.fm.ops[i].sl = fin.ReadInt();
                inst.fm.ops[i].sus = fin.ReadInt();
                inst.fm.ops[i].tl = fin.ReadInt();
                inst.fm.ops[i].vib = fin.ReadInt();
                inst.fm.ops[i].ws = fin.ReadInt();
                inst.fm.ops[i].ksr = fin.ReadInt(); // RS on SEGA Genesis
                inst.fm.ops[i].dt = fin.ReadInt();
                inst.fm.ops[i].d2r = fin.ReadInt();
                inst.fm.ops[i].ssg_mode = fin.ReadInt();
            }
        }
    }

    return inst;
}

void DMF::Importer::LoadWavetablesData(Reader& fin)
{
    parent_->total_wavetables_ = fin.ReadInt();

    parent_->wavetable_sizes_ = new uint32_t[parent_->total_wavetables_];
    parent_->wavetable_values_ = new uint32_t*[parent_->total_wavetables_];

    uint32_t data_mask = 0xFFFFFFFF;
    if (parent_->GetSystem().type == DMF::SystemType::kGameBoy)
    {
        data_mask = 0xF;
    }
    else if (parent_->GetSystem().type == DMF::SystemType::kNES_FDS)
    {
        data_mask = 0x3F;
    }

    for (int i = 0; i < parent_->total_wavetables_; i++)
    {
        parent_->wavetable_sizes_[i] = fin.ReadInt<false, 4>();

        parent_->wavetable_values_[i] = new uint32_t[parent_->wavetable_sizes_[i]];

        for (unsigned j = 0; j < parent_->wavetable_sizes_[i]; j++)
        {
            parent_->wavetable_values_[i][j] = fin.ReadInt<false, 4>() & data_mask;

            // Bug fix for DMF version 25 (0x19): Transform 4-bit FDS wavetables into 6-bit
            if (parent_->GetSystem().type == DMF::SystemType::kNES_FDS && parent_->file_version_ <= 25)
            {
                parent_->wavetable_values_[i][j] <<= 2; // x4
            }
        }
    }
}

void DMF::Importer::LoadPatternsData(Reader& fin)
{
    auto& module_data = parent_->GetData();
    auto& channel_metadata = module_data.ChannelMetadataRef();

    std::unordered_set<std::pair<ChannelIndex, PatternIndex>, PairHash> patterns_visited; // Storing channel/patternId pairs

    for (ChannelIndex channel = 0; channel < module_data.GetNumChannels(); ++channel)
    {
        channel_metadata[channel].effect_columns_count = fin.ReadInt();

        for (OrderIndex order = 0; order < module_data.GetNumOrders(); ++order)
        {
            const PatternIndex pattern_id = module_data.GetPatternId(channel, order);

            if (patterns_visited.count({channel, pattern_id}) > 0) // If pattern has been loaded previously
            {
                // Skip patterns that have already been loaded (unnecessary information)
                // zstr's seekg method does not seem to work, so I will use this:
                unsigned seek_amount = (8 + 4 * channel_metadata[channel].effect_columns_count) * module_data.GetNumRows();
                while (0 < seek_amount--)
                {
                    fin.ReadInt();
                }
                continue;
            }
            else
            {
                // Mark this pattern_id for this channel as visited
                patterns_visited.insert({channel, pattern_id});
            }

            for (RowIndex row = 0; row < module_data.GetNumRows(); ++row)
            {
                module_data.SetRowById(channel, pattern_id, row, LoadPatternRow(fin, channel_metadata[channel].effect_columns_count));
            }
        }
    }
}

Row<DMF> DMF::Importer::LoadPatternRow(Reader& fin, uint8_t effect_columns_count)
{
    Row<DMF> row;

    const uint16_t temp_pitch = fin.ReadInt<false, 2>();
    uint8_t temp_octave = fin.ReadInt<false, 2>(); // Upper byte is unused

    switch (temp_pitch)
    {
        case 0:
            if (temp_octave == 0)
                row.note = NoteTypes::Empty{};
            else
                row.note = NoteTypes::Note{static_cast<NotePitch>(temp_pitch), temp_octave};
            break;
        case 100:
            row.note = NoteTypes::Off{};
            break;
        default:
            assert(temp_pitch <= 12);
            // Apparently, the note pitch for C- can be either 0 or 12. I'm setting it to 0 always.
            if (temp_pitch == 12)
                row.note = NoteTypes::Note{NotePitch::kC, ++temp_octave};
            else
                row.note = NoteTypes::Note{static_cast<NotePitch>(temp_pitch), temp_octave};
            break;
    }

    row.volume = fin.ReadInt<true, 2>();

    for (uint8_t col = 0; col < effect_columns_count; ++col)
    {
        const int16_t dmf_effect_code = fin.ReadInt<true, 2>();
        row.effect[col].value = fin.ReadInt<true, 2>();
        assert(kEffectValueless == -1); // DMF valueless effect magic number is -1, and so must be kEffectValueless

        // Convert DMF effect code to dmf2mod's internal representation
        EffectCode effect_code = Effects::kNoEffect;
        switch (dmf_effect_code)
        {
            case dmf::EffectCode::kNoEffect:             effect_code = Effects::kNoEffect; break;
            case dmf::EffectCode::kArp:                  effect_code = Effects::kArp; break;
            case dmf::EffectCode::kPortUp:               effect_code = Effects::kPortUp; break;
            case dmf::EffectCode::kPortDown:             effect_code = Effects::kPortDown; break;
            case dmf::EffectCode::kPort2Note:            effect_code = Effects::kPort2Note; break;
            case dmf::EffectCode::kVibrato:              effect_code = Effects::kVibrato; break;
            case dmf::EffectCode::kPort2NoteVolSlide:    effect_code = Effects::kPort2NoteVolSlide; break;
            case dmf::EffectCode::kVibratoVolSlide:      effect_code = Effects::kVibratoVolSlide; break;
            case dmf::EffectCode::kTremolo:              effect_code = Effects::kTremolo; break;
            case dmf::EffectCode::kPanning:              effect_code = Effects::kPanning; break;
            case dmf::EffectCode::kSetSpeedVal1:         effect_code = Effects::kSpeedA; break;
            case dmf::EffectCode::kVolSlide:             effect_code = Effects::kVolSlide; break;
            case dmf::EffectCode::kPosJump:              effect_code = Effects::kPosJump; break;
            case dmf::EffectCode::kRetrig:               effect_code = Effects::kRetrigger; break;
            case dmf::EffectCode::kPatBreak:             effect_code = Effects::kPatBreak; break;
            case dmf::EffectCode::kArpTickSpeed:         effect_code = dmf::Effects::kArpTickSpeed; break;
            case dmf::EffectCode::kNoteSlideUp:          effect_code = dmf::Effects::kNoteSlideUp; break;
            case dmf::EffectCode::kNoteSlideDown:        effect_code = dmf::Effects::kNoteSlideDown; break;
            case dmf::EffectCode::kSetVibratoMode:       effect_code = dmf::Effects::kSetVibratoMode; break;
            case dmf::EffectCode::kSetFineVibratoDepth:  effect_code = dmf::Effects::kSetFineVibratoDepth; break;
            case dmf::EffectCode::kSetFinetune:          effect_code = dmf::Effects::kSetFinetune; break;
            case dmf::EffectCode::kSetSamplesBank:       effect_code = dmf::Effects::kSetSamplesBank; break;
            case dmf::EffectCode::kNoteCut:              effect_code = Effects::kNoteCut; break;
            case dmf::EffectCode::kNoteDelay:            effect_code = Effects::kNoteDelay; break;
            case dmf::EffectCode::kSyncSignal:           effect_code = dmf::Effects::kSyncSignal; break;
            case dmf::EffectCode::kSetGlobalFinetune:    effect_code = dmf::Effects::kSetGlobalFinetune; break;
            case dmf::EffectCode::kSetSpeedVal2:         effect_code = Effects::kSpeedB; break;

            // Game Boy exclusive:
            case dmf::EffectCode::kGameBoySetWave:                   effect_code = dmf::Effects::kGameBoySetWave; break;
            case dmf::EffectCode::kGameBoySetNoisePolyCounterMode:   effect_code = dmf::Effects::kGameBoySetNoisePolyCounterMode; break;
            case dmf::EffectCode::kGameBoySetDutyCycle:              effect_code = dmf::Effects::kGameBoySetDutyCycle; break;
            case dmf::EffectCode::kGameBoySetSweepTimeShift:         effect_code = dmf::Effects::kGameBoySetSweepTimeShift; break;
            case dmf::EffectCode::kGameBoySetSweepDir:               effect_code = dmf::Effects::kGameBoySetSweepDir; break;

            default:
                // Set a warning here? (unable to parse effect code)
                break;
        }
        row.effect[col].code = effect_code;
    }

    // Initialize the rest to zero
    for (uint8_t col = effect_columns_count; col < 4; ++col) // Max total of 4 effects columns in Deflemask
    {
        row.effect[col] = {Effects::kNoEffect, 0};
    }

    row.instrument = fin.ReadInt<true, 2>();

    return row;
}

void DMF::Importer::LoadPCMSamplesData(Reader& fin)
{
    parent_->total_pcm_samples_ = fin.ReadInt();
    parent_->pcm_samples_ = new PCMSample[parent_->total_pcm_samples_];

    for (unsigned sample = 0; sample < parent_->total_pcm_samples_; sample++)
    {
        parent_->pcm_samples_[sample] = LoadPCMSample(fin);
    }
}

PCMSample DMF::Importer::LoadPCMSample(Reader& fin)
{
    PCMSample sample;

    sample.size = fin.ReadInt<false, 4>();

    if (parent_->file_version_ >= 24) // DMF version 24 (0x18)
    {
        // Read PCM sample name
        sample.name = fin.ReadPStr();
    }
    else // DMF version 23 (0x17) and older. WARNING: I don't have the specs for version 23 (0x17), so this may be wrong.
    {
        // PCM samples don't have names in this DMF version
        sample.name = "";
    }

    sample.rate = fin.ReadInt();
    sample.pitch = fin.ReadInt();
    sample.amp = fin.ReadInt();

    if (parent_->file_version_ >= 22) // DMF version 22 (0x16) and newer
    {
        sample.bits = fin.ReadInt();
    }

    sample.data = new uint16_t[sample.size];
    for (uint32_t i = 0; i < sample.size; i++)
    {
        sample.data[i] = fin.ReadInt<false, 2>();
    }

    return sample;
}

void DMF::GetBPM(unsigned& numerator, unsigned& denominator) const
{
    // Gets the initial BPM of the module
    unsigned int global_tick;
    if (module_info_.using_custom_hz)
    {
        if (module_info_.custom_hz_value1 == 0) // No digits filled in
        {
            // NTSC is used by default if custom global tick box is selected but the value is left blank:
            global_tick = 60;
        }
        else if (module_info_.custom_hz_value2 == 0) // One digit filled in
        {
            global_tick = (module_info_.custom_hz_value1 - 48);
        }
        else if (module_info_.custom_hz_value3 == 0) // Two digits filled in
        {
            global_tick = (module_info_.custom_hz_value1 - 48) * 10 + (module_info_.custom_hz_value2 - 48);
        }
        else // All three digits filled in
        {
            global_tick = (module_info_.custom_hz_value1 - 48) * 100 + (module_info_.custom_hz_value2 - 48) * 10 + (module_info_.custom_hz_value3 - 48);
        }
    }
    else
    {
        global_tick = module_info_.frames_mode ? 60 : 50; // NTSC (60 Hz) or PAL (50 Hz)
    }

    // Experimentally determined equation for BPM:
    numerator = 15 * global_tick;
    denominator = module_info_.time_base * (module_info_.tick_time1 + module_info_.tick_time2);

    if (denominator == 0)
        throw std::runtime_error("Tried to divide by zero when calculating BPM.\n");
}

double DMF::GetBPM() const
{
    // Returns the initial BPM of the module
    unsigned numerator, denominator;
    GetBPM(numerator, denominator);
    return numerator * 1.0 / denominator;
}

/*
 * Currently only supports the Game Boy system.
 *
 * Flags:
 * 0:  Default generation (generates all data)
 * 1:  MOD-compatible portamentos (no port2note auto-off, ...)
 * 2:  MOD-compatible loops (notes, sound index, and channel volume carry over)
 *      Adds a Note OFF to loopback points if needed
 * Return flags:
 * 0:  kSuccess
 * 1:  Error
 * 2:  An extra "loopback order" is needed
 */
size_t DMF::GenerateDataImpl(size_t data_flags) const
{
    auto& gen_data = *GetGeneratedDataMut();
    const auto& data = GetData();

    // Currently can only generate data for the Game Boy system
    if (GetSystem().type != System::Type::kGameBoy)
        return 1;

    // Initialize state
    auto& state_data = gen_data.GetState().emplace();
    state_data.Initialize(GetSystem().channels);

    // Get reader/writers
    auto state_reader_writers = state_data.GetReaderWriters();
    auto& global_state = state_reader_writers->global_reader_writer;
    auto& channel_states = state_reader_writers->channel_reader_writers;

    // Initialize other generated data
    using GenDataEnumCommon = GeneratedData<DMF>::GenDataEnumCommon;
    auto& sound_indexes_used = gen_data.Get<GenDataEnumCommon::kSoundIndexesUsed>().emplace();
    auto& sound_index_note_extremes = gen_data.Get<GenDataEnumCommon::kSoundIndexNoteExtremes>().emplace();
    //auto& channel_note_extremes = gen_data.Get<GenDataEnumCommon::kChannelNoteExtremes>().emplace();
    gen_data.Get<GenDataEnumCommon::kNoteOffUsed>() = false;
    gen_data.Get<GenDataEnumCommon::kTotalOrders>() = data.GetNumOrders();

    // Data flags
    size_t return_val = 0;
    const bool no_port2note_auto_off = data_flags & 0x1;
    const bool mod_compat_loops = data_flags & 0x2;

    // For convenience:
    using GlobalCommon = GlobalState<DMF>::StateEnumCommon;
    using GlobalOneShotCommon = GlobalState<DMF>::OneShotEnumCommon;
    //using GlobalEnum = GlobalState<DMF>::StateEnum;
    using ChannelCommon = ChannelState<DMF>::StateEnumCommon;
    using ChannelOneShotCommon = ChannelState<DMF>::OneShotEnumCommon;
    //using ChannelEnum = ChannelState<DMF>::StateEnum;

    /*
     * In spite of what the Deflemask manual says, portamento effects are automatically turned off if they
     *  stay on long enough without a new note being played. I believe it's until C-2 is reached for port down
     *  and C-8 for port up. Port2Note also seems to have an "auto-off" point.
     * See order 0x14 in the "i wanna eat my ice cream alone (reprise)" demo song for an example of "auto-off" behavior.
     * In that order, for the F-2 note on SQ2, the port down effect turns off automatically if the next note
     *   comes 21 or more rows later. The number of rows it takes depends on the note pitch, port effect parameter,
     * and the tempo denominator (Speed A/B and the base time).
     * The UpdatePeriod function below updates the note period on each row, taking portamentos into account.
     * While it is based on the formulas apparently used by Deflemask, it is still not 100% accurate.
     */

    const int ticks[2] = {module_info_.time_base * module_info_.tick_time1, module_info_.time_base * module_info_.tick_time2}; // even, odd

    constexpr double lowest_period = GetPeriod({NotePitch::kC, 2}); // C-2
    constexpr double highest_period = GetPeriod({NotePitch::kC, 8}); // C-8

    // Given the current note period, 0/1 for even/odd row, the current portamento effect, and the target note (for port2note),
    //  calculates and returns the next note period.
    auto UpdatePeriod = [&, ticks, lowest_period, highest_period]
        (double period, int even_odd_row, const PortamentoStateData& port, double target_period) -> double
    {
        switch (port.type)
        {
            case PortamentoStateData::kUp:
                return std::max(period - (port.value * ticks[even_odd_row] * 4 / 3.0), highest_period);
            case PortamentoStateData::kDown:
                return std::min(period + (port.value * ticks[even_odd_row]), lowest_period);
            case PortamentoStateData::kToNote:
            {
                assert(target_period >= highest_period && target_period <= lowest_period);
                if (target_period < period)
                {
                    // Target is a higher pitch
                    const double amount = port.value * ticks[even_odd_row] * 4 / 3.0;
                    if (std::abs(target_period - period) < amount) // Close enough to target - snap to it
                        return target_period;
                    return period - amount;
                }
                else
                {
                    // Target is a lower pitch
                    const double amount = port.value * ticks[even_odd_row];
                    if (std::abs(target_period - period) < amount) // Close enough to target - snap to it
                        return target_period;
                    return period + amount;
                }
            }
            default:
                return period;
        }
    };

    // The current period of the note playing in each channel. Is affected by portamentos. 0 is off.
    std::vector<double> periods(data.GetNumChannels(), 0.0);

    // The target period for an active port2note effect
    std::vector<double> target_periods(data.GetNumChannels(), lowest_period);

    // Notes can be "cancelled" by Port2Note effects under certain conditions
    std::vector<bool> note_cancelled(data.GetNumChannels(), false);

    // Loopback points - take note of them during the main loop then set the state afterward
    std::vector<std::pair<OrderRowPosition, OrderRowPosition>> loopbacks_temp; // From/To

    // The following variables are used for order/row and PosJump/PatBreak-related stuff
    std::vector<OrderIndex> order_map(data.GetNumOrders(), (OrderIndex)-1); // Maps DMF order to DMF state order (-1 = not set, though use skipped_orders instead)
    order_map[0] = 0;
    std::vector<bool> skipped_orders(data.GetNumOrders(), false); // DMF orders as indexes
    std::vector<RowIndex> starting_row(data.GetNumOrders(), 0); // DMF orders as indexes
    std::vector<RowIndex> last_row(data.GetNumOrders(), data.GetNumRows()); // DMF orders as indexes
    OrderIndex total_gen_data_orders = 0;
    OrderIndex num_orders_skipped = 0; // TODO: May be unnecessary now that there's skipped_orders

    // Sound indexes
    std::array<std::pair<OrderRowPosition, SoundIndexType<DMF>>, 4> current_sound_index
    {
        std::pair{-1, SoundIndex<DMF>::Square{0}},
        std::pair{-1, SoundIndex<DMF>::Square{0}},
        std::pair{-1, SoundIndex<DMF>::Wave{0}},
        std::pair{-1, SoundIndex<DMF>::Noise{0}}
    };

    // Set initial state (global)
    global_state.Reset(); // Just in case
    global_state.SetInitial<GlobalCommon::kSpeedA>(module_info_.tick_time1); // * timebase?
    global_state.SetInitial<GlobalCommon::kSpeedB>(module_info_.tick_time2); // * timebase?
    global_state.SetInitial<GlobalCommon::kTempo>(0); // TODO: How should tempo/speed info be stored?

    // Set initial state (per-channel)
    for (unsigned i = 0; i < channel_states.size(); ++i)
    {
        auto& channel_state = channel_states[i];
        channel_state.Reset(); // Just in case

        channel_state.SetInitial<ChannelCommon::kSoundIndex>(current_sound_index[i].second);
        channel_state.SetInitial<ChannelCommon::kNoteSlot>(NoteTypes::Empty{});
        channel_state.SetInitial<ChannelCommon::kNotePlaying>(false);
        channel_state.SetInitial<ChannelCommon::kVolume>(kDMFGameBoyVolumeMax);
        channel_state.SetInitial<ChannelCommon::kArp>(0);
        channel_state.SetInitial<ChannelCommon::kPort>({PortamentoStateData::kNone, 0});
        channel_state.SetInitial<ChannelCommon::kVibrato>(0);
        channel_state.SetInitial<ChannelCommon::kPort2NoteVolSlide>(0);
        channel_state.SetInitial<ChannelCommon::kVibratoVolSlide>(0);
        channel_state.SetInitial<ChannelCommon::kTremolo>(0);
        channel_state.SetInitial<ChannelCommon::kPanning>(127);
        channel_state.SetInitial<ChannelCommon::kVolSlide>(0);
    }

    // Main loop
    for (ChannelIndex channel = 0; channel < data.GetNumChannels(); ++channel)
    {
        global_state.Reset();
        auto& channel_state = channel_states[channel];

        if (channel == dmf::GameBoyChannel::kNoise)
            continue;

        for (OrderIndex order = 0; order < data.GetNumOrders(); ++order)
        {
            // Handle skipped orders for PosJump
            if (skipped_orders[order])
                continue;

            OrderIndex gen_data_order = order_map[order];
            RowIndex row_offset = starting_row[order];

            for (RowIndex row = row_offset; row < last_row[order]; ++row)
            {
                RowIndex gen_data_row = row - row_offset;
                channel_state.SetWritePos(gen_data_order, gen_data_row);
                const auto& row_data = data.GetRow(channel, order, row);

                // CHANNEL STATE - PORT2NOTE
                if (!NoteIsEmpty(row_data.note))
                {
                    // Portamento to note stops when next note is reached or on Note OFF
                    if (channel_state.Get<ChannelCommon::kPort>().type == PortamentoStateData::kToNote)
                        channel_state.Set<ChannelCommon::kPort>(PortamentoStateData{PortamentoStateData::kNone, 0});
                }

                // CHANNEL STATE - PORT2NOTE
                if (!no_port2note_auto_off) // If using port2note auto off
                {
                    // This breaks bergentruckung.dmf --> MOD because while the port2note effects are being automatically stopped
                    // at the correct time in Deflemask, in ProTracker the effects need to stay on for an extra row to reach
                    // their target period. I think this is due to the sample splitting and/or inaccuracies.
                    if (periods[channel] == target_periods[channel])
                    {
                        // Portamento to note stops when it reaches its target period
                        if (channel_state.Get<ChannelCommon::kPort>().type == PortamentoStateData::kToNote)
                        {
                            channel_state.Set<ChannelCommon::kPort>(PortamentoStateData{PortamentoStateData::kNone, 0});
                        }
                    }
                }

                // CHANNEL STATE - PORTAMENTOS
                if (periods[channel] >= lowest_period || periods[channel] <= highest_period)
                {
                    // If the period is at the highest or lowest value, automatically stop any portamento effects
                    if (channel_state.Get<ChannelCommon::kPort>().type != PortamentoStateData::kNone)
                    {
                        channel_state.Set<ChannelCommon::kPort>(PortamentoStateData{PortamentoStateData::kNone, 0});
                    }
                }

                // CHANNEL STATE - EFFECTS
                // TODO: Could this be done during the import step for greater efficiency?
                bool port2note_used = false;
                {
                    // If these don't have a value, the port effect wasn't used in this row, else it is the active (left-most) port's effect value.
                    // If the left-most port effect was valueless, it is set to 0 since valueless/0 seem to have the same behavior in Deflemask.
                    std::optional<EffectValueXX> port_up, port_down, port2note;

                    // Any port effect regardless of value/valueless/priority cancels any active port effect from a previous row.
                    bool prev_port_cancelled = false;

                    // When no note w/ pitch has played in the channel yet, and there is a note with pitch
                    // on the current row, if the left-most Port2Note were to be used with value > 0, that note will not play.
                    // In addition, all subsequent notes in the channel will also be cancelled until the port2note is stopped by
                    // a future port effect, note OFF, or it auto-off's. Port2Note auto-off is not implemented here though.
                    const bool port2note_note_cancellation_possible = channel_state.GetSize<ChannelCommon::kNoteSlot>() == 1 && NoteHasPitch(row_data.note);
                    bool just_cancelled_note = false;
                    bool temp_note_cancelled = note_cancelled[channel];

                    // Other effects:
                    std::optional<EffectValueXX> arp, vibrato, port2note_volslide, vibrato_volslide, tremolo, panning, volslide, retrigger, note_cut, note_delay;
                    SoundIndexType<DMF> sound_index{SoundIndex<DMF>::None{}};

                    // Loop right to left because left-most effects in effects column have priority
                    for (auto iter = std::crbegin(row_data.effect); iter != std::crend(row_data.effect); ++iter)
                    {
                        const auto& effect = *iter;
                        if (effect.code == Effects::kNoEffect)
                            continue;

                        const EffectValue effect_value = effect.value;
                        const uint8_t effect_value_normal = effect_value != kEffectValueless ? effect_value : 0;

                        switch (effect.code)
                        {
                        case Effects::kArp:
                            arp = effect_value_normal;
                            break;
                        case Effects::kPortUp:
                            prev_port_cancelled = true;
                            temp_note_cancelled = false; // Will "uncancel" notes if a port2note in this row isn't cancelling them
                            port_up = effect_value_normal;
                            break;
                        case Effects::kPortDown:
                            prev_port_cancelled = true;
                            temp_note_cancelled = false; // Will "uncancel" notes if a port2note in this row isn't cancelling them
                            port_down = effect_value_normal;
                            break;
                        case Effects::kPort2Note:
                            prev_port_cancelled = true;

                            if (port2note_note_cancellation_possible)
                            {
                                note_cancelled[channel] = effect_value > 0;
                                just_cancelled_note = effect_value > 0;
                            }
                            port2note = effect_value_normal;
                            break;
                        case Effects::kVibrato:
                            vibrato = effect_value_normal;
                            break;
                        case Effects::kPort2NoteVolSlide:
                            port2note_volslide = effect_value_normal;
                            break;
                        case Effects::kVibratoVolSlide:
                            vibrato_volslide = effect_value_normal;
                            break;
                        case Effects::kTremolo:
                            tremolo = effect_value_normal;
                            break;
                        case Effects::kPanning:
                            panning = effect_value_normal;
                            break;
                        case Effects::kSpeedA:
                            // Handled by global state
                            break;
                        case Effects::kVolSlide:
                            volslide = effect_value_normal;
                            break;
                        case Effects::kPosJump:
                            // Handled by global state
                            break;
                        case Effects::kRetrigger:
                            retrigger = effect_value_normal;
                            break;
                        case Effects::kPatBreak:
                            // Handled by global state
                            break;
                        case Effects::kNoteCut:
                            note_cut = effect_value_normal;
                            break;
                        case Effects::kNoteDelay:
                            note_delay = effect_value_normal;
                            break;
                        case Effects::kTempo:
                            // Handled by global state
                            break;
                        case Effects::kSpeedB:
                            // Handled by global state
                            break;

                        // DMF-specific effects

                        case dmf::Effects::kGameBoySetWave:
                            if (channel != dmf::GameBoyChannel::kWave || effect_value < 0)
                                break; // TODO: Is this behavior correct?
                            if (effect_value >= GetTotalWavetables())
                                break; // TODO: An invalid SetWave parameter exhibits strange behavior in Deflemask
                            sound_index = SoundIndex<DMF>::Wave{effect_value_normal};
                            // TODO: If a sound index is set but a note with it is never played, it should later be removed from the channel state
                            break;
                        case dmf::Effects::kGameBoySetDutyCycle:
                            if (channel > dmf::GameBoyChannel::kSquare2 || effect_value < 0 || effect_value >= 4)
                                break; // Valueless of invalid 12xx effects do not do anything. TODO: What is the effect in WAVE and NOISE channels?
                            sound_index = SoundIndex<DMF>::Square{effect_value_normal};
                            // TODO: If a sound index is set but a note with it is never played, it should later be removed from the channel state
                            break;
                        default:
                            break;
                        }
                    }

                    if (!just_cancelled_note && !temp_note_cancelled) // No port effects are set if port2note just cancelled notes
                    {
                        // A port up/down/2note "uncancelled" the notes
                        note_cancelled[channel] = false;

                        bool need_to_set_port = false;
                        PortamentoStateData temp_port;

                        // Set port effects in order of priority (highest to lowest):
                        if (port2note)
                        {
                            need_to_set_port = true;
                            temp_port = { PortamentoStateData::kToNote, static_cast<uint8_t>(port2note.value()) };
                            port2note_used = true;
                        }
                        else if (port_down)
                        {
                            need_to_set_port = true;
                            temp_port = { PortamentoStateData::kDown, static_cast<uint8_t>(port_down.value()) };
                        }
                        else if (port_up)
                        {
                            need_to_set_port = true;
                            temp_port = { PortamentoStateData::kUp, static_cast<uint8_t>(port_up.value()) };
                        }
                        else if (prev_port_cancelled)
                        {
                            need_to_set_port = true;
                            temp_port = { PortamentoStateData::kNone, 0 };
                        }

                        if (need_to_set_port)
                        {
                            // If setting a port to a value of zero, use kNone instead
                            if (temp_port.value != 0)
                                channel_state.Set<ChannelCommon::kPort>(temp_port);
                            else
                                channel_state.Set<ChannelCommon::kPort>(PortamentoStateData{PortamentoStateData::kNone, 0});
                        }
                    }

                    if (just_cancelled_note)
                    {
                        // TODO: Set warning here?
                        // Can notes be cancelled for longer than they should be?
                    }

                    // Set other effects' states (WIP)
                    if (arp)
                        channel_state.Set<ChannelCommon::kArp>(arp.value());
                    if (vibrato)
                        channel_state.Set<ChannelCommon::kVibrato>(vibrato.value());
                    if (port2note_volslide)
                        channel_state.Set<ChannelCommon::kPort2NoteVolSlide>(port2note_volslide.value());
                    if (vibrato_volslide)
                        channel_state.Set<ChannelCommon::kVibratoVolSlide>(vibrato_volslide.value());
                    if (tremolo)
                        channel_state.Set<ChannelCommon::kTremolo>(tremolo.value());
                    if (panning)
                        channel_state.Set<ChannelCommon::kPanning>(panning.value());
                    if (volslide)
                        channel_state.Set<ChannelCommon::kVolSlide>(volslide.value());
                    if (retrigger)
                        channel_state.SetOneShot<ChannelOneShotCommon::kRetrigger>(retrigger.value());
                    if (note_cut)
                        channel_state.SetOneShot<ChannelOneShotCommon::kNoteCut>(note_cut.value());
                    if (note_delay)
                        channel_state.SetOneShot<ChannelOneShotCommon::kNoteDelay>(note_delay.value());

                    if (sound_index.index() != SoundIndex<DMF>::kNone)
                    {
                        current_sound_index[channel] = { GetOrderRowPosition(gen_data_order, gen_data_row), sound_index };
                    }
                }

                // CHANNEL STATE - NOTES AND SOUND INDEXES
                // NOTE: Empty notes are not added to state between notes with pitch
                const NoteSlot& note_slot = row_data.note;
                if (NoteIsOff(note_slot))
                {
                    channel_state.Set<ChannelCommon::kNoteSlot>(note_slot); // channel_state.SetSingle<ChannelCommon::kNoteSlot>(note_slot, NoteTypes::Empty{});
                    channel_state.Set<ChannelCommon::kNotePlaying>(false);
                    gen_data.Get<GenDataEnumCommon::kNoteOffUsed>() = true;
                    note_cancelled[channel] = false; // An OFF also "uncancels" notes cancelled by a port2note effect
                    // NOTE: Note OFF does not affect the current note period
                }
                else if (NoteHasPitch(note_slot) && !note_cancelled[channel])
                {
                    channel_state.Set<ChannelCommon::kNoteSlot, true>(note_slot);
                    channel_state.Set<ChannelCommon::kNotePlaying>(true);
                    const Note& note = GetNote(note_slot);

                    // Update the period
                    if (!port2note_used)
                        periods[channel] = GetPeriod(note);
                    else
                        target_periods[channel] = GetPeriod(note);

                    const auto& sound_index = current_sound_index[channel].second;

                    // Mark this square wave or wavetable as used
                    sound_indexes_used.insert(sound_index);

                    // Write the sound index. Might set the order/row write position back a bit
                    // temporarily, but it will still be guaranteed to write to the end of the
                    // underlying vector and not mess up the always-increasing position ordering.
                    channel_state.SetWritePos(current_sound_index[channel].first);
                    channel_state.Set<ChannelCommon::kSoundIndex>(sound_index);
                    channel_state.SetWritePos(gen_data_order, gen_data_row);

                    // Get lowest/highest notes
                    if (sound_index_note_extremes.count(sound_index) == 0) // 1st time
                    {
                        sound_index_note_extremes[sound_index] = { note, note };
                    }
                    else
                    {
                        auto& note_pair = sound_index_note_extremes[sound_index];
                        if (note > note_pair.second)
                        {
                            // Found a new highest note
                            note_pair.second = note;
                        }
                        if (note < note_pair.first)
                        {
                            // Found a new lowest note
                            note_pair.first = note;
                        }
                    }
                }

                // Update current period
                periods[channel] = UpdatePeriod(periods[channel], row % 2, channel_state.Get<ChannelCommon::kPort>(), target_periods[channel]);

                // CHANNEL STATE - VOLUME
                if (row_data.volume != kDMFNoVolume)
                {
                    // The WAVE channel volume changes whether a note is attached or not, but SQ1/SQ2 need a note
                    if (channel == dmf::GameBoyChannel::kWave)
                    {
                        // WAVE volume is actually more quantized:
                        switch (row_data.volume)
                        {
                            case 0: case 1: case 2: case 3:
                                channel_state.Set<ChannelCommon::kVolume>(0); break;
                            case 4: case 5: case 6: case 7:
                                channel_state.Set<ChannelCommon::kVolume>(5); break;
                            case 8: case 9: case 10: case 11:
                                channel_state.Set<ChannelCommon::kVolume>(10); break;
                            case 12: case 13: case 14: case 15:
                                channel_state.Set<ChannelCommon::kVolume>(15); break;
                            default:
                                assert(false && "Invalid DMF volume");
                                break;
                        }
                    }
                    else if (NoteHasPitch(row_data.note))
                    {
                        channel_state.Set<ChannelCommon::kVolume>(static_cast<EffectValueXX>(row_data.volume));
                    }
                }

                // GLOBAL STATE
                if (channel == 0) // Only update global state once, the first time this order/row is encountered
                {
                    global_state.SetWritePos(gen_data_order, gen_data_row);

                    // Deflemask PosJump/PatBreak behavior (experimentally determined in Deflemask 1.1.3):
                    // The left-most PosJump or PatBreak in a given row is the one that takes effect.
                    // If the left-most PosJump or PatBreak is invalid (no value or invalid value),
                    //  every other effect of that type in the row is ignored.
                    // PosJump effects are ignored if a valid and non-ignored PatBreak is present in the row.
                    std::optional<EffectValueXX> pos_jump, pat_break, speed_a, speed_b, tempo;
                    bool ignore_pos_jump = false, ignore_pat_break = false;

                    // Want to check all channels to update the global state for this row
                    for (ChannelIndex channel2 = 0; channel2 < data.GetNumChannels(); ++channel2)
                    {
                        const auto& row_data2 = data.GetRow(channel2, order, row);
                        for (const auto& effect : row_data2.effect)
                        {
                            //const uint8_t effect_value_normal = effect.value != kEffectValueless ? effect.value : 0; // ???
                            switch (effect.code)
                            {
                                case Effects::kPosJump:
                                    if (ignore_pos_jump)
                                        break;
                                    if (!pos_jump.has_value() && (effect.value == kEffectValueless || effect.value >= data.GetNumOrders()))
                                    {
                                        ignore_pos_jump = true;
                                        break;
                                    }
                                    pos_jump = static_cast<EffectValueXX>(effect.value);
                                    ignore_pos_jump = true;
                                    break;
                                case Effects::kPatBreak:
                                    if (ignore_pat_break)
                                        break;
                                    if (order + 1 == data.GetNumOrders())
                                        break; // PatBreak on last order has no effect
                                    if (!pat_break.has_value() && (effect.value == kEffectValueless || effect.value >= data.GetNumRows()))
                                    {
                                        ignore_pat_break = true;
                                        break;
                                    }
                                    pat_break = static_cast<EffectValueXX>(effect.value);
                                    ignore_pat_break = true;
                                    break;
                                case Effects::kSpeedA:
                                    // TODO
                                    //speed = effect.value;
                                    break;
                                case Effects::kSpeedB:
                                    // TODO
                                    //speed = effect.value;
                                    break;
                                case Effects::kTempo:
                                    // TODO
                                    //tempo = effect.value;
                                    break;
                                default:
                                    break;
                            }
                        }
                    }

                    // If we're on an order that starts on a row > 0 (due to a PatBreak),
                    // and we're at the end the order, and PatBreak/PosJump isn't already used,
                    // then we need to add a PatBreak/PosJump to ensure row_offset extra rows aren't played.
                    if (row_offset > 0 && !pat_break.has_value() && !pos_jump.has_value() && row == data.GetNumRows() - row_offset)
                    {
                        // If we're on the last order, a PosJump should be used instead
                        if (order + 1 != data.GetNumOrders())
                            pat_break = 0;
                        else
                            pos_jump = 0;
                    }

                    // Set the global state if needed

                    if (speed_a)
                        global_state.Set<GlobalCommon::kSpeedA>(speed_a.value());
                    if (speed_b)
                        global_state.Set<GlobalCommon::kSpeedB>(speed_b.value());
                    if (tempo)
                        global_state.Set<GlobalCommon::kTempo>(tempo.value());

                    if (pat_break)
                    {
                        // Always 0 b/c we're using row offsets
                        global_state.SetOneShot<GlobalOneShotCommon::kPatBreak>(0);

                        // If PatBreak value > 0, rows in gen data will shifted by an offset so that they start on row 0.
                        assert(order < data.GetNumOrders());
                        starting_row[order + 1] = pat_break.value();

                        // Any further rows in this order/pattern are skipped because they unreachable.
                        last_row[order] = row + 1;
                        break;
                    }
                    else if (pos_jump) // PosJump only takes effect if PatBreak isn't used
                    {
                        if (pos_jump.value() > order) // If not a loop
                        {
                            // In Deflemask, orders skipped by a forward PosJump are unplayable.
                            // For generated data, those orders will be omitted, so no PosJump is needed.
                            unsigned orders_to_skip = pos_jump.value() - order - 1;
                            num_orders_skipped += orders_to_skip;
                            while (orders_to_skip != 0)
                            {
                                skipped_orders[order + orders_to_skip] = true;
                                --orders_to_skip;
                            }

                            // If not on the last row, use a PatBreak. PosJump is not needed.
                            if (row + 1 != data.GetNumRows())
                            {
                                global_state.SetOneShot<GlobalOneShotCommon::kPatBreak>(0);
                            }

                            // Any further rows in this order/pattern are skipped because they unreachable.
                            last_row[order] = row + 1;
                            break;
                        }
                        else // A loop
                        {
                            // If we attempt to jump back to an order that was skipped,
                            // the next non-skipped order after that is used instead.
                            while (skipped_orders[pos_jump.value()])
                            {
                                ++pos_jump.value();
                                assert(pos_jump.value() < data.GetNumOrders());
                            }

                            // TODO: Could two PosJumps go to the same destination, creating situation with two loopback oneshots with the same order/row pos? Currently only allowing one loopback.
                            loopbacks_temp.push_back({ GetOrderRowPosition(gen_data_order, gen_data_row), GetOrderRowPosition(order_map.at(pos_jump.value()), 0) }); // From/To
                            global_state.SetOneShot<GlobalOneShotCommon::kPosJump>(order_map.at(pos_jump.value()));

                            // Any further orders or rows in this song are ignored because they unreachable.
                            // Break out of entire nested loop.
                            last_row[order] = row + 1;
                            for (OrderIndex i = order + 1; i < data.GetNumOrders(); ++i)
                            {
                                skipped_orders[i] = true;
                            }
                            break;
                        }
                    }
                }

                // TODO: Would COR and ORC affect this? EDIT: Shouldn't matter as long as O comes before R?
            }

            // Handle data order to gen data order mapping
            if (channel == 0)
            {
                ++total_gen_data_orders;

                if (order + 1 < data.GetNumOrders() && !skipped_orders[order + 1])
                {
                    order_map[order + 1] = total_gen_data_orders;
                }
            }
        }
    }

    // Gen data's total orders may be less than data's if any orders are skipped due to PosJump or
    // unreachable due to being an order after a loopback.
    gen_data.Get<GenDataEnumCommon::kTotalOrders>().value() -= num_orders_skipped;

    const auto last_order_temp = data.GetNumOrders() - 1 - num_orders_skipped;
    const auto last_row_temp = last_row[last_order_temp] - 1;
    const auto last_order_row = GetOrderRowPosition(last_order_temp, last_row_temp);

    // Handle loopback at end of song
    if (loopbacks_temp.empty())
    {
        // No pos jump + loopback has been added for the end of the song - need to add them here
        global_state.Reset();
        global_state.SetWritePos(last_order_row);
        global_state.SetOneShot<GlobalOneShotCommon::kPosJump>(0);
        loopbacks_temp.push_back({last_order_row, 0});
    }

    // Write loopbacks to state
    // loopbacks_temp is guaranteed to be non-empty at this point

    global_state.Reset();

    // These must be ordered by increasing OrderRowPosition in second pair element (the "to" order)
    using ElementType = std::pair<OrderRowPosition, OrderRowPosition>;
    std::sort(loopbacks_temp.begin(), loopbacks_temp.end(), [](const ElementType& lhs, const ElementType& rhs)
    {
        // If the "to" order/rows are identical, compare the "from" order/rows
        return lhs.second == rhs.second ? lhs.first < rhs.first : lhs.second < rhs.second;
    });

    OrderRowPosition last = -1;
    for (const auto& [from, to] : loopbacks_temp)
    {
        assert(last != to && "More than one oneshot is being written to the same order/row which might cause issues when reading");
        if (last != to)
        {
            global_state.SetWritePos(to);
            global_state.SetOneShot<GlobalOneShotCommon::kLoopback>(from);
            last = to;

            // Only proceed if using MOD-compatible loops
            if (!mod_compat_loops)
                continue;

            // When looping back, notes might carry over and need to be stopped with a Note OFF.
            // This for-loop checks whether that is true for any channel, and inserts a Note OFF if needed and possible.
            for (ChannelIndex channel = 0; channel < data.GetNumChannels(); ++channel)
            {
                auto& channel_state = channel_states[channel];
                channel_state.Reset();
                channel_state.SetReadPos(to);
                channel_state.SetWritePos(to);

                const auto state_before_loop = channel_state.ReadAt(from);
                const bool playing_before = channel_state.GetValue<ChannelCommon::kNotePlaying>(state_before_loop);
                if (playing_before)
                {
                    // A note was playing just before looping back
                    const auto current_row = channel_state.GetImpulse<ChannelCommon::kNoteSlot>();
                    if (!current_row.has_value() || NoteIsEmpty(current_row.value()))
                    {
                        // There's an empty slot on this row
                        const bool playing_now = channel_state.Get<ChannelCommon::kNotePlaying>();
                        if (playing_now)
                        {
                            // In Deflemask, a note would be playing on this row the first time through, but when looping back
                            // to this row, it would act as if there's a Note OFF here. Protracker doesn't do this.
                            // An extra "loopback order" would be needed to emulate this behavior in Protracker.
                            // For now, do nothing and just emit a warning.
                            return_val |= 2; // Warning about loopback inaccuracy
                        }
                        else
                        {
                            // Can safely insert a Note OFF in this row to stop notes carrying over from the loop
                            channel_state.Insert<ChannelCommon::kNoteSlot, true>(NoteTypes::Off{});
                            gen_data.Get<GenDataEnumCommon::kNoteOffUsed>() = true;
                        }
                    }
                }
            }
        }
    }

    return return_val;
}
