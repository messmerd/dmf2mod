/*
    mod.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines all classes used for ProTracker's MOD files.

    Several limitations apply in order to export. For example,
    for DMF-->MOD, the DMF file must use the Game Boy system,
    patterns must have 64 or fewer rows, etc.
*/

#include "modules/mod.h"
#include "utils/utils.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <cstdio>
#include <set>
#include <cassert>
#include <numeric>

using namespace d2m;
using namespace d2m::mod;
// DO NOT use any module namespace other than d2m::mod

using MODOptionEnum = MODConversionOptions::OptionEnum;

static std::vector<int8_t> GenerateSquareWaveSample(unsigned duty_cycle, unsigned length);
static std::vector<int8_t> GenerateWavetableSample(uint32_t* wavetable_data, unsigned length);
static inline uint8_t GetEffectCode(int effect_code);

static std::string GetWarningMessage(MOD::ConvertWarning warning, const std::string& info = "");

/*
    Game Boy's range is:  C-0 -> C-8 (though notes lower than C-2 just play as C-2)
    ProTracker's range is:  C-1 -> B-3  (plus octaves 0 and 4 which are non-standard) 
    See DMFSampleMapper for how this issue is resolved.
*/

static constexpr uint16_t kProTrackerPeriodTable[5][12] = {
    {1712,1616,1525,1440,1357,1281,1209,1141,1077,1017, 961, 907},  /* C-0 to B-0 */
    {856,808,762,720,678,640,604,570,538,508,480,453},              /* C-1 to B-1 */
    {428,404,381,360,339,320,302,285,269,254,240,226},              /* C-2 to B-2 */
    {214,202,190,180,170,160,151,143,135,127,120,113},              /* C-3 to B-3 */
    {107,101, 95, 90, 85, 80, 76, 71, 67, 64, 60, 57}               /* C-4 to B-4 */
};

// Effect codes used by the MOD format:
// An effect is represented with 12 bits, which is 3 groups of 4 bits: [a][x][y] or [a][b][x]
// The effect code is [a] or [a][b], and the effect value is [x][y] or [x]. [x][y] codes are the
// extended effects. All effect codes are stored below. Non-extended effects have 0x0 in the right-most
// nibble in order to line up with the extended effects:
namespace d2m::mod::EffectCode
{
    enum
    {
        kNoEffect           =0x00,
        kNoEffectVal        =0x00,
        kNoEffectCode       =0x00, /* NoEffect is the same as ((uint16_t)NoEffectCode << 4) | NoEffectVal */
        kArp                =0x00,
        kPortUp             =0x10,
        kPortDown           =0x20,
        kPort2Note          =0x30,
        kVibrato            =0x40,
        kPort2NoteVolSlide  =0x50,
        kVibratoVolSlide    =0x60,
        kTremolo            =0x70,
        kPanning            =0x80,
        kSetSampleOffset    =0x90,
        kVolSlide           =0xA0,
        kPosJump            =0xB0,
        kSetVolume          =0xC0,
        kPatBreak           =0xD0,
        kSetFilter          =0xE0,
        kFineSlideUp        =0xE1,
        kFineSlideDown      =0xE2,
        kSetGlissando       =0xE3,
        kSetVibratoWaveform =0xE4,
        kSetFinetune        =0xE5,
        kLoopPattern        =0xE6,
        kSetTremoloWaveform =0xE7,
        kRetriggerSample    =0xE9,
        kFineVolSlideUp     =0xEA,
        kFineVolSlideDown   =0xEB,
        kCutSample          =0xEC,
        kDelaySample        =0xED,
        kDelayPattern       =0xEE,
        kInvertLoop         =0xEF,
        kSetSpeed           =0xF0
    };
}

MOD::MOD() {}

void MOD::ImportImpl(const std::string& filename)
{
    // Not implemented
    throw NotImplementedException();
}

void MOD::ConvertImpl(const ModulePtr& input)
{
    if (!input)
    {
        throw MODException(ModuleException::Category::kConvert, ModuleException::ConvertError::kInvalidArgument);
    }

    switch (input->GetType())
    {
        case ModuleType::kDMF:
            ConvertFromDMF(*input->Cast<const DMF>());
            break;
        // Add other input types here if support is added
        default:
            // Unsupported input type for conversion to MOD
            throw MODException(ModuleException::Category::kConvert, ModuleException::ConvertError::kUnsupportedInputType, input->GetInfo()->file_extension);
    }
}

///////// CONVERT FROM DMF /////////

void MOD::ConvertFromDMF(const DMF& dmf)
{
    const bool verbose = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kVerbose).GetValue<bool>();

    if (verbose)
        std::cout << "Starting to convert to MOD...\n";

    if (dmf.GetSystem().type != DMF::SystemType::kGameBoy) // If it's not a Game Boy
    {
        throw MODException(ModuleException::Category::kConvert, MOD::ConvertError::kNotGameBoy);
    }

    const auto& dmf_data = dmf.GetData();
    auto& mod_data = GetData();

    if (dmf_data.GetNumRows() > 64)
    {
        throw MODException(ModuleException::Category::kConvert, MOD::ConvertError::kOver64RowPattern);
    }

    ChannelIndex num_channels = dmf_data.GetNumChannels();
    if (num_channels != 4)
    {
        throw ModuleException(ModuleException::Category::kConvert, MOD::ConvertError::kWrongChannelCount, "Wrong number of channels. There should be exactly 4.");
    }

    ///////////////// GET DMF GENERATED DATA

    const size_t error_code = dmf.GenerateData(1 | 2); // MOD-compatibility flags
    if (error_code & 2)
        status_.AddWarning(GetWarningMessage(ConvertWarning::kLoopbackInaccuracy));

    auto dmf_gen_data = dmf.GetGeneratedData();

    const OrderIndex num_orders = dmf_gen_data->GetNumOrders().value() + (OrderIndex)using_setup_pattern_;
    if (num_orders > 64) // num_orders is 1 more than it actually is
    {
        throw MODException(ModuleException::Category::kConvert, MOD::ConvertError::kTooManyPatternMatrixRows);
    }

    ///////////////// SET UP DATA

    mod_data.AllocatePatternMatrix(num_channels, num_orders, 64);

    // Fill pattern matrix with pattern ids 0,1,2,...,N
    std::iota(mod_data.PatternMatrixRef().begin(), mod_data.PatternMatrixRef().end(), 0);

    mod_data.AllocateChannels();
    mod_data.AllocatePatterns();

    ///////////////// CONVERT SONG TITLE AND AUTHOR

    auto& mod_global_data = mod_data.GlobalData();

    mod_global_data.title = dmf_data.GlobalData().title;
    if (mod_global_data.title.size() > 20)
        mod_global_data.title.resize(20); // Don't pad with spaces b/c exporting to WAV in ProTracker will keep those spaces in the exported file name

    mod_global_data.author = dmf_data.GlobalData().author;
    mod_global_data.author.resize(22, ' '); // Author will be displayed in 1st sample; sample names have 22 character limit

    ///////////////// CONVERT SAMPLES

    if (verbose)
        std::cout << "Converting samples...\n";

    SampleMap sample_map;
    DMFConvertSamples(dmf, sample_map);
    assert(sample_map.find(SoundIndex<DMF>::None{}) == sample_map.end() || sample_map.at(SoundIndex<DMF>::None{}).GetFirstMODSampleId() == 1);

    ///////////////// CONVERT PATTERN DATA

    if (verbose)
        std::cout << "Converting pattern data...\n";

    DMFConvertPatterns(dmf, sample_map);

    ///////////////// CLEAN UP

    if (verbose)
        std::cout << "Done converting to MOD.\n\n";
}

void MOD::DMFConvertSamples(const DMF& dmf, SampleMap& sample_map)
{
    // This method determines whether a DMF sound index will need to be split into low, middle,
    //  or high ranges in MOD, then assigns MOD sample numbers, sample lengths, etc.

    const auto& dmf_sound_indexes = dmf.GetGeneratedData()->Get<GeneratedData<DMF>::kSoundIndexesUsed>().value();
    const auto& dmf_sound_index_note_extremes = dmf.GetGeneratedData()->Get<GeneratedData<DMF>::kSoundIndexNoteExtremes>().value();

    SoundIndexType<MOD> mod_current_sound_index = 1; // Sample #0 is special in ProTracker

    // Init silent sample if needed. It is always sample #1 if used.
    if (dmf.GetGeneratedData()->Get<GeneratedData<DMF>::kNoteOffUsed>().value())
    {
        auto& sample_mapper = sample_map.insert({SoundIndex<DMF>::None{}, {}}).first->second;
        mod_current_sound_index = sample_mapper.InitSilence();
    }

    // Map the DMF Square and WAVE sound indexes to MOD sample ids
    for (const auto& dmf_sound_index : dmf_sound_indexes)
    {
        auto& sample_mapper = sample_map.insert({dmf_sound_index, {}}).first->second;
        const auto& note_extremes = dmf_sound_index_note_extremes.at(dmf_sound_index);
        mod_current_sound_index = sample_mapper.Init(dmf_sound_index, mod_current_sound_index, note_extremes);

        if (sample_mapper.IsDownsamplingNeeded())
        {
            status_.AddWarning(GetWarningMessage(MOD::ConvertWarning::kWaveDownsample, std::to_string(std::get<SoundIndex<DMF>::Wave>(dmf_sound_index).id)));
        }
    }

    total_mod_samples_ = mod_current_sound_index - 1; // Set the number of MOD samples that will be needed. (minus sample #0 which is special)

    // TODO: Check if there are too many samples needed here, and throw exception if so

    DMFConvertSampleData(dmf, sample_map);
}

void MOD::DMFConvertSampleData(const DMF& dmf, const SampleMap& sample_map)
{
    // Fill out information needed to define a MOD sample
    samples_.clear();

    for (const auto& [dmf_sound_index, sample_mapper] : sample_map)
    {
        for (int note_range_int = 0; note_range_int < sample_mapper.GetNumMODSamples(); ++note_range_int)
        {
            auto note_range = static_cast<DMFSampleMapper::NoteRange>(note_range_int);

            Sample si;

            si.id = sample_mapper.GetMODSampleId(note_range);
            si.length = sample_mapper.GetMODSampleLength(note_range);
            si.repeat_length = si.length;
            si.repeat_offset = 0;
            si.finetune = 0;

            si.name = "";

            // Set sample data specific to the sample type:
            using SampleType = DMFSampleMapper::SampleType;
            switch (sample_mapper.GetSampleType())
            {
                case SampleType::kSilence:
                    si.name = "Silence";
                    si.volume = 0;
                    si.data = std::vector<int8_t>(si.length, 0);
                    break;

                case SampleType::kSquare:
                {
                    const uint8_t duty_cycle = std::get<SoundIndex<DMF>::Square>(dmf_sound_index).id;
                    si.name = "SQW, Duty ";
                    switch (duty_cycle)
                    {
                        case 0: si.name += "12.5%"; break;
                        case 1: si.name += "25%"; break;
                        case 2: si.name += "50%"; break;
                        case 3: si.name += "75%"; break;
                    }
                    si.volume = kVolumeMax; // TODO: Optimize this?
                    si.data = GenerateSquareWaveSample(duty_cycle, si.length);
                    break;
                }

                case SampleType::kWave:
                {
                    const uint8_t wavetable_index = std::get<SoundIndex<DMF>::Wave>(dmf_sound_index).id;

                    si.name = "Wavetable #";
                    si.name += std::to_string(wavetable_index);
                
                    si.volume = kVolumeMax; // TODO: Optimize this?

                    uint32_t* wavetable_data = dmf.GetWavetableValues()[wavetable_index];
                    si.data = GenerateWavetableSample(wavetable_data, si.length);
                    break;
                }
            }

            // Append note range text to the sample name:
            using NoteRangeName = DMFSampleMapper::NoteRangeName;
            switch (sample_mapper.GetMODNoteRangeName(note_range))
            {
                case NoteRangeName::kNone:
                    break;
                case NoteRangeName::kLow:
                    si.name += " (low)"; break;
                case NoteRangeName::kMiddle:
                    si.name += " (mid)"; break;
                case NoteRangeName::kHigh:
                    si.name += " (high)"; break;
            }

            if (si.id == 1) // #0 is a special value, not the 1st MOD sample
            {
                // Overwrite 1st sample's name with author's name
                si.name = GetAuthor();
            }

            if (si.name.size() > 22)
                throw std::invalid_argument("Sample name must be 22 characters or less");

            // Pad name with zeros
            while (si.name.size() < 22)
                si.name += " ";

            samples_[si.id] = si;
        }
    }
}

std::vector<int8_t> GenerateSquareWaveSample(unsigned duty_cycle, unsigned length)
{
    std::vector<int8_t> sample;
    sample.assign(length, 0);

    constexpr uint8_t duty[] = {1, 2, 4, 6};

    // This loop creates a square wave with the correct length and duty cycle:
    for (unsigned i = 1; i <= length; i++)
    {
        if ((i * 8.f) / length <= (float)duty[duty_cycle])
        {
            sample[i - 1] = 127; // high
        }
        else
        {
            sample[i - 1] = -10; // low
        }
    }

    return sample;
}

std::vector<int8_t> GenerateWavetableSample(uint32_t* wavetable_data, unsigned length)
{
    std::vector<int8_t> sample;
    sample.assign(length, 0);

    constexpr float max_vol_cap = 12.f / 15.f; // Set WAVE max volume to 12/15 of potential max volume to emulate DMF wave channel

    for (unsigned i = 0; i < length; i++)
    {
        // Note: For the Deflemask Game Boy system, all wavetable lengths are 32.
        // Converting from DMF sample values (0 to 15) to PT sample values (-128 to 127).
        switch (length)
        {
            case 512: // x16
                sample[i] = (int8_t)(((wavetable_data[i / 16] / 15.f * 255.f) - 128.f) * max_vol_cap); break;
            case 256: // x8
                sample[i] = (int8_t)(((wavetable_data[i / 8] / 15.f * 255.f) - 128.f) * max_vol_cap); break;
            case 128: // x4
                sample[i] = (int8_t)(((wavetable_data[i / 4] / 15.f * 255.f) - 128.f) * max_vol_cap); break;
            case 64:  // x2
                sample[i] = (int8_t)(((wavetable_data[i / 2] / 15.f * 255.f) - 128.f) * max_vol_cap); break;
            case 32:  // Original length
                sample[i] = (int8_t)(((wavetable_data[i] / 15.f * 255.f) - 128.f) * max_vol_cap); break;
            case 16:  // Half length (loss of information from downsampling)
            {
                // Take average of every two sample values to make new sample value
                const unsigned sum = wavetable_data[i * 2] + wavetable_data[(i * 2) + 1];
                sample[i] = (int8_t)(((sum / (15.f * 2) * 255.f) - 128.f) * max_vol_cap);
                break;
            }
            case 8:   // Quarter length (loss of information from downsampling)
            {
                // Take average of every four sample values to make new sample value
                const unsigned sum = wavetable_data[i * 4] + wavetable_data[(i * 4) + 1] + wavetable_data[(i * 4) + 2] + wavetable_data[(i * 4) + 3];
                sample[i] = (int8_t)(((sum / (15.f * 4) * 255.f) - 128.f) * max_vol_cap);
                break;
            }
            default:
                // ERROR: Invalid length
                throw std::invalid_argument("Invalid value for length in GenerateWavetableSample(): " + std::to_string(length));
        }
    }

    return sample;
}

void MOD::DMFConvertPatterns(const DMF& dmf, const SampleMap& sample_map)
{
    auto& mod_data = GetData();
    const auto options = GetOptions()->Cast<const MODConversionOptions>();

    unsigned initial_tempo, initial_speed; // Together these will set the initial BPM
    DMFConvertInitialBPM(dmf, initial_tempo, initial_speed);

    if (using_setup_pattern_)
    {
        // Set initial tempo
        Row<MOD> tempo_row;
        tempo_row.sample = 0;
        tempo_row.note = NoteTypes::Empty{};
        tempo_row.effect = { Effects::kTempo, static_cast<EffectValue>(initial_tempo) };
        mod_data.SetRow(0, 0, 0, tempo_row);

        // Set initial speed
        if (options->GetTempoType() != MODConversionOptions::TempoType::kEffectCompatibility)
        {
            Row<MOD> speed_row;
            speed_row.sample = 0;
            speed_row.note = NoteTypes::Empty{};
            speed_row.effect = { Effects::kSpeedA, static_cast<EffectValue>(initial_speed) };
            mod_data.SetRow(1, 0, 0, speed_row);
        }

        // Set Pattern Break to start of song
        Row<MOD> pat_break_row;
        pat_break_row.sample = 0;
        pat_break_row.note = NoteTypes::Empty{};
        pat_break_row.effect = { Effects::kPatBreak, 0 };
        mod_data.SetRow(2, 0, 0, pat_break_row);

        // Set Amiga Filter
        Row<MOD> amiga_filter_row;
        amiga_filter_row.sample = 0;
        amiga_filter_row.note = NoteTypes::Empty{};
        amiga_filter_row.effect = { mod::Effects::kSetFilter, !options->GetOption(MODOptionEnum::kAmigaFilter).GetValue<bool>() };
        mod_data.SetRow(3, 0, 0, amiga_filter_row);

        // All other channel rows in the pattern are already zeroed out so nothing needs to be done for them
    }

    const OrderIndex dmf_num_orders = dmf.GetGeneratedData()->GetNumOrders().value();
    const RowIndex dmf_num_rows = dmf.GetData().GetNumRows();

    auto state_readers = dmf.GetGeneratedData()->GetState().value().GetReaders();
    auto& global_reader = state_readers->global_reader;
    auto& channel_readers = state_readers->channel_readers;

    // Extra state info needed:
    std::array<DMFSampleMapper::NoteRange, 4> note_range;
    note_range.fill(DMFSampleMapper::NoteRange::kFirst); // ???
    std::vector<PriorityEffect> global_effects;
    std::array set_sample{ false, false, false, false };
    std::array set_volume_if_not{ -1, -1, -1, -1 }; // Signifies the channel volume needs to be set if the current channel volume is not this value. Volume in DMF units.

    // Main loop
    for (OrderIndex dmf_order = 0; dmf_order < dmf_num_orders; ++dmf_order)
    {
        // Loop through rows in a pattern:
        for (RowIndex dmf_row = 0; dmf_row < dmf_num_rows; ++dmf_row)
        {
            global_reader.SetReadPos(dmf_order, dmf_row);

            // Global effects, highest priority first:

            if (global_reader.GetOneShotDelta(GlobalState<DMF>::kPatBreak))
                global_effects.push_back({ kEffectPriorityStructureRelated, { Effects::kPatBreak, static_cast<EffectValue>(global_reader.GetOneShot<GlobalState<DMF>::kPatBreak>()) } });
            else if (dmf_num_rows < 64 && dmf_row + 1 == dmf_num_rows)
                global_effects.push_back({ kEffectPriorityStructureRelated, { Effects::kPatBreak, 0 } });  // Use PatBreak to allow patterns under 64 rows

            if (global_reader.GetOneShotDelta(GlobalState<DMF>::kPosJump))
                global_effects.push_back({ kEffectPriorityStructureRelated, { Effects::kPosJump, static_cast<EffectValue>(global_reader.GetOneShot<GlobalState<DMF>::kPosJump>() + using_setup_pattern_) } });

            std::array<Row<MOD>, 4> mod_row_data{};
            std::array<PriorityEffect, 4> mod_effects{};

            // Loop through channels:
            for (ChannelIndex channel = 0; channel < mod_data.GetNumChannels(); ++channel)
            {
                auto& channel_reader = channel_readers[channel];
                channel_reader.SetReadPos(dmf_order, dmf_row);

                if (channel != dmf::GameBoyChannel::kNoise)
                {
                    if (global_reader.GetOneShotDelta(GlobalState<DMF>::kLoopback))
                    {
                        // When looping back, the sound index and channel volume could be different before
                        // and after the PosJump, which would require them to be set at the next note played after the
                        // loopback point else the sound index and channel volume from before the PosJump would carry over.
                        // In addition, notes might carry over and need to be stopped with a Note OFF.
                        const OrderRowPosition looping_back_from = global_reader.GetOneShot<GlobalState<DMF>::kLoopback>();
                        const auto state_before_loop = channel_reader.ReadAt(looping_back_from);

                        // Set the volume if it changed
                        const auto volume_before = channel_reader.GetValue<ChannelState<DMF>::kVolume>(state_before_loop);
                        if (channel_reader.Get<ChannelState<DMF>::kVolume>() != volume_before)
                        {
                            set_volume_if_not[channel] = volume_before;
                            // set_volume_if_not will be reset to -1 if a volume change occurs after this row, or anything that would
                            // cause a volume change, such as a sample change.
                        }

                        // Explicitly set the sample if needed
                        const auto dmf_sound_index_before = channel_reader.GetValue<ChannelState<DMF>::kSoundIndex>(state_before_loop);
                        const NoteSlot dmf_noteslot_before = channel_reader.GetValue<ChannelState<DMF>::kNoteSlot>(state_before_loop);
                        const auto mod_sound_index_before = NoteHasPitch(dmf_noteslot_before) ? sample_map.at(dmf_sound_index_before).GetMODSampleId(GetNote(dmf_noteslot_before)) : 1;

                        const auto next_note = channel_reader.Find<ChannelState<DMF>::kNoteSlot>([](const NoteSlot& n) { return NoteHasPitch(n); });
                        if (next_note.has_value())
                        {
                            const auto state_at_next_note = channel_reader.ReadAt(next_note.value().first);
                            const auto dmf_sound_index_at_next_note = channel_reader.GetValue<ChannelState<DMF>::kSoundIndex>(state_at_next_note);
                            const auto mod_sound_index_at_next_note = sample_map.at(dmf_sound_index_at_next_note).GetMODSampleId(GetNote(next_note.value().second));
                            if (mod_sound_index_before != mod_sound_index_at_next_note)
                            {
                                // Tell DMFConvertNote to explicitly set the sample the next time a note is played
                                set_sample[channel] = true;
                            }
                        }
                    }

                    if (channel_reader.GetDelta(ChannelState<DMF>::kVolume))
                    {
                        // set_volume_if_not is essentially used to insert an extra volume change into the state
                        // because in Protracker, the channel volume at the end of a song can carry over when looping back
                        // and this behavior is not specified in the generated state data.
                        set_volume_if_not[channel] = -1; // New volume change encountered; set_volume_if_not is now irrelevant
                    }

                    mod_effects[channel] = DMFConvertEffects(channel_reader);
                    mod_row_data[channel] = DMFConvertNote(channel_reader, note_range[channel], set_sample[channel], set_volume_if_not[channel], sample_map, mod_effects[channel]);
                }
            }

            ApplyEffects(mod_row_data, mod_effects, global_effects);

            // Set the channel rows for the current pattern row all at once
            for (ChannelIndex channel = 0; channel < mod_data.GetNumChannels(); ++channel)
            {
                mod_data.SetRow(channel, dmf_order + using_setup_pattern_, dmf_row, mod_row_data[channel]);
            }
        }

        // If the DMF has less than 64 rows per pattern, there will be extra MOD rows which will need to be blank; TODO: May not be needed
        for (RowIndex dmf_row = dmf_num_rows; dmf_row < 64; ++dmf_row)
        {
            for (ChannelIndex channel = 0; channel < mod_data.GetNumChannels(); ++channel)
            {
                Row<MOD> temp_row_data{ 0, NoteTypes::Empty{}, { Effects::kNoEffect, 0 } };
                mod_data.SetRow(channel, dmf_order + (int)using_setup_pattern_, dmf_row, temp_row_data);
            }
        }
    }
}

mod::PriorityEffect MOD::DMFConvertEffects(ChannelStateReader<DMF>& state)
{
    const auto& options = *GetOptions()->Cast<const MODConversionOptions>();

    // Effects are listed here with highest priority first

    // Portamentos
    if (PortamentoStateData val = state.Get<ChannelState<DMF>::kPort>(); val.type != PortamentoStateData::kNone)
    {
        const EffectValue effect_value = static_cast<EffectValue>(val.value);
        switch (val.type)
        {
            case PortamentoStateData::kUp:
                if (options.AllowPortamento())
                    return { kEffectPriorityPortUp, { Effects::kPortUp, effect_value } };
                break;
            case PortamentoStateData::kDown:
                if (options.AllowPortamento())
                    return { kEffectPriorityPortDown, { Effects::kPortDown, effect_value } };
                break;
            case PortamentoStateData::kToNote:
                if (options.AllowPort2Note())
                    return { kEffectPriorityPort2Note, { Effects::kPort2Note, effect_value } };
                break;
            default:
                assert(0);
                break;
        }
    }

    // If volume changed, need to update it
    if (state.GetDelta(ChannelState<DMF>::kVolume))
    {
        VolumeStateData dmf_volume = state.Get<ChannelState<DMF>::kVolume>();
        uint8_t mod_volume = (uint8_t)std::round(dmf_volume / (double)dmf::kDMFGameBoyVolumeMax * (double)kVolumeMax); // Convert DMF volume to MOD volume

        return { kEffectPriorityVolumeChange, { mod::Effects::kSetVolume, mod_volume } };
    }

    // Arpeggios
    if (EffectValue val = state.Get<ChannelState<DMF>::kArp>(); val > 0 && options.AllowArpeggio())
        return { kEffectPriorityArp, { Effects::kArp, val } };

    // Vibrato
    if (EffectValue val = state.Get<ChannelState<DMF>::kVibrato>(); val > 0 && options.AllowVibrato())
        return { kEffectPriorityVibrato, { Effects::kVibrato, val } };

    return { kEffectPriorityUnsupportedEffect, { Effects::kNoEffect, 0 } };
}

Row<MOD> MOD::DMFConvertNote(ChannelStateReader<DMF>& state, mod::DMFSampleMapper::NoteRange& note_range, bool& set_sample, int& set_vol_if_not, const MOD::SampleMap& sample_map, mod::PriorityEffect& mod_effect)
{
    // Do not call this when on the noise channel

    // note_playing is the state from the previous row

    Row<MOD> row_data{};

    if (!state.GetDelta(ChannelState<DMF>::kNoteSlot))
    {
        // This is actually an Empty note slot
        row_data.sample = 0; // Keeps previous sample id
        row_data.note = NoteTypes::Empty{};
        return row_data;
    }

    NoteSlot dmf_note = state.Get<ChannelState<DMF>::kNoteSlot>();
    switch (dmf_note.index())
    {
        // Convert note - Empty
        case NoteTypes::kEmpty:
            row_data.sample = 0; // Keeps previous sample id
            row_data.note = dmf_note;
            return row_data;

        // Convert note - Note OFF
        case NoteTypes::kOff:
            row_data.sample = 1; // Use silent sample (always sample #1 if it is used)
            row_data.note = dmf_note; // Don't need a note for the silent sample to work
            set_sample = false;
            set_vol_if_not = -1; // On sample changes, Protracker resets the volume
            return row_data;

        // Convert note - Note with pitch
        case NoteTypes::kNote:
        {
            const SoundIndexType<DMF>& dmf_sound_index = state.Get<ChannelState<DMF>::kSoundIndex>();
            const DMFSampleMapper& sample_mapper = sample_map.at(dmf_sound_index);

            DMFSampleMapper::NoteRange new_note_range;
            row_data.note = sample_mapper.GetMODNote(GetNote(dmf_note), new_note_range);

            bool mod_sample_changed = state.GetDelta(ChannelState<DMF>::kSoundIndex);
            if (note_range != new_note_range)
            {
                // Switching to a different note range also requires the use of a different MOD sample
                mod_sample_changed = true;
                note_range = new_note_range;
            }

            const auto dmf_volume = state.Get<ChannelState<DMF>::kVolume>();
            const bool note_playing_rising_edge = state.GetImpulse<ChannelState<DMF>::kNotePlaying>().value_or(false);
            if (mod_sample_changed || note_playing_rising_edge || set_sample)
            {
                row_data.sample = sample_mapper.GetMODSampleId(new_note_range);
                mod_sample_changed = false; // Just changed the sample, so resetting this for next time

                // When you change ProTracker samples, the channel volume resets, so we need to check if
                //  a volume change effect is needed to get the volume back to where it was.

                set_vol_if_not = -1; // On sample changes, Protracker resets the volume

                if (dmf_volume != dmf::kDMFGameBoyVolumeMax) // Currently, the default volume for all MOD samples is the maximum. TODO: Can optimize
                {
                    const uint8_t mod_volume = (uint8_t)std::round(dmf_volume / (double)dmf::kDMFGameBoyVolumeMax * (double)kVolumeMax); // Convert DMF volume to MOD volume

                    // If this volume change effect has a higher priority, use it
                    if (mod_effect.first <= kEffectPriorityVolumeChange)
                        mod_effect = { kEffectPriorityVolumeChange, { mod::Effects::kSetVolume, mod_volume } };

                    // TODO: If the default volume of the sample is selected in a smart way, we could potentially skip having to use a volume effect sometimes
                }

                set_sample = false; // Sample was just explicitly set; can reset this
            }
            else if (set_vol_if_not >= 0 && dmf_volume != set_vol_if_not)
            {
                // Need to explicitly set volume for this note because of channel volume carrying over when looping back
                const uint8_t mod_volume = (uint8_t)std::round(dmf_volume / (double)dmf::kDMFGameBoyVolumeMax * (double)kVolumeMax); // Convert DMF volume to MOD volume

                // If this volume change effect has a higher priority, use it
                if (mod_effect.first <= kEffectPriorityVolumeChange)
                {
                    mod_effect = { kEffectPriorityVolumeChange, { mod::Effects::kSetVolume, mod_volume } };
                    set_vol_if_not = -1; // Volume was just set; can reset this
                }

                // Keeps the previous sample number and prevents channel volume from being reset
                row_data.sample = 0;
            }
            else
            {
                // Keeps the previous sample number and prevents channel volume from being reset
                row_data.sample = 0;
            }

            return row_data;
        }

        default:
            assert(false);
            return {};
    }
}

void MOD::ApplyEffects(std::array<Row<MOD>, 4>& row_data, const std::array<mod::PriorityEffect, 4>& mod_effects, std::vector<mod::PriorityEffect>& global_effects)
{
    // When there are no global (channel-independent) effects:
    if (global_effects.empty())
    {
        row_data[0].effect = mod_effects[0].second;
        row_data[1].effect = mod_effects[1].second;
        row_data[2].effect = mod_effects[2].second;
        row_data[3].effect = mod_effects[3].second;
        return;
    }

    using ChannelPriorityEffect = std::pair<ChannelIndex, mod::PriorityEffect>;
    std::array<ChannelPriorityEffect, 4> priority;
    for (ChannelIndex channel = 0; channel < 4; ++channel)
    {
        priority[channel] = {channel, mod_effects[channel]};
    }

    // Sort so that the lowest priority effects are first
    auto cmp = [](const ChannelPriorityEffect& l, const ChannelPriorityEffect& r) -> bool { return l.second.first < r.second.first; };
    std::sort(priority.begin(), priority.end(), cmp);

    int i2 = 0;
    for (int i = 0; i < static_cast<int>(global_effects.size()); ++i)
    {
        // If the priority of this global effect is higher than the lowest priority per-channel effect
        if (global_effects[i].first > priority[i2].second.first)
        {
            // Make that channel use the global effect instead
            row_data[priority[i2].first].effect = global_effects[i].second;

            // Erase the global effect
            global_effects.erase(global_effects.begin() + i);
            --i;
            ++i2;
        }
        else
        {
            // Warning: Failed to use a global effect.
        }
    }

    // Set the rest of the effects
    for (; i2 < 4; ++i2)
    {
        row_data[priority[i2].first].effect = priority[i2].second.second;
    }

    if (!global_effects.empty())
    {
        // Warning: Some global effects were not used
    }

}

void MOD::DMFConvertInitialBPM(const DMF& dmf, unsigned& tempo, unsigned& speed)
{
    // Brute force function to get the Tempo/Speed pair which produces a BPM as close as possible to the desired BPM (if accuracy is desired),
    //      or a Tempo/Speed pair which is as close to the desired BPM without breaking the behavior of effects

    static constexpr double kHighestBPM = 3.0 * 255.0 / 1.0; // 3 * tempo * speed
    static constexpr double kLowestBPM = 3.0 * 32.0 / 31.0;  // 3 * tempo * speed

    const double desired_bpm = dmf.GetBPM();

    const auto options = std::static_pointer_cast<MODConversionOptions>(GetOptions());
    if (options->GetTempoType() == MODConversionOptions::TempoType::kEffectCompatibility)
    {
        tempo = static_cast<unsigned>(desired_bpm * 2);
        speed = 6;

        if (tempo > 255)
        {
            tempo = 255;
            status_.AddWarning(GetWarningMessage(ConvertWarning::kTempoHighCompat));
        }
        else if (tempo < 32)
        {
            tempo = 32;
            status_.AddWarning(GetWarningMessage(ConvertWarning::kTempoLowCompat));
        }
        else if ((desired_bpm * 2.0) - static_cast<double>(tempo) > 1e-3)
        {
            status_.AddWarning(GetWarningMessage(ConvertWarning::kTempoAccuracy));
        }
        return;
    }

    if (desired_bpm > kHighestBPM)
    {
        tempo = 255;
        speed = 1;
        status_.AddWarning(GetWarningMessage(ConvertWarning::kTempoHigh));
        return;
    }

    if (desired_bpm < kLowestBPM)
    {
        tempo = 32;
        speed = 31;
        status_.AddWarning(GetWarningMessage(ConvertWarning::kTempoLow));
        return;
    }

    tempo = 0;
    speed = 0;
    double best_BPM_diff = 9999999.0;

    for (unsigned d = 1; d <= 31; d++)
    {
        if (3 * 32.0 / d > desired_bpm || desired_bpm > 3 * 255.0 / d)
            continue; // Not even possible with this speed value

        for (unsigned n = 32; n <= 255; n++)
        {
            const double bpm = 3.0 * (double)n / d;
            const double this_bpm_diff = std::abs(desired_bpm - bpm);
            if (this_bpm_diff < best_BPM_diff
                || (this_bpm_diff == best_BPM_diff && d <= 6)) // Choose speed values more compatible with effects w/o sacrificing accuracy
            {
                tempo = n;
                speed = d;
                best_BPM_diff = this_bpm_diff;
            }
        }
    }

    if (best_BPM_diff > 1e-3)
        status_.AddWarning(GetWarningMessage(ConvertWarning::kTempoAccuracy));
}

static inline uint8_t GetEffectCode(int effect_code)
{
    // Maps dmf2mod internal effect code to MOD effect code.
    switch (effect_code)
    {
        // Common effects:
        case d2m::Effects::kNoEffect:
            return mod::EffectCode::kNoEffect;
        case d2m::Effects::kArp:
            return mod::EffectCode::kArp;
        case d2m::Effects::kPortUp:
            return mod::EffectCode::kPortUp;
        case d2m::Effects::kPortDown:
            return mod::EffectCode::kPortDown;
        case d2m::Effects::kPort2Note:
            return mod::EffectCode::kPort2Note;
        case d2m::Effects::kVibrato:
            return mod::EffectCode::kVibrato;
        case d2m::Effects::kPort2NoteVolSlide:
            return mod::EffectCode::kPort2NoteVolSlide;
        case d2m::Effects::kVibratoVolSlide:
            return mod::EffectCode::kVibratoVolSlide;
        case d2m::Effects::kTremolo:
            return mod::EffectCode::kTremolo;
        case d2m::Effects::kPanning:
            return mod::EffectCode::kPanning;
        case d2m::Effects::kSpeedA:
            return mod::EffectCode::kSetSpeed;
        case d2m::Effects::kVolSlide:
            return mod::EffectCode::kVolSlide;
        case d2m::Effects::kPosJump:
            return mod::EffectCode::kPosJump;
        case d2m::Effects::kRetrigger:
            return mod::EffectCode::kRetriggerSample;
        case d2m::Effects::kPatBreak:
            return mod::EffectCode::kPatBreak;
        case d2m::Effects::kNoteCut:
            return mod::EffectCode::kCutSample;
        case d2m::Effects::kNoteDelay:
            return mod::EffectCode::kDelaySample;
        case d2m::Effects::kTempo:
            return mod::EffectCode::kSetSpeed; // Same as kSpeedA, but different effect values are used
        case d2m::Effects::kSpeedB:
            assert(false && "Unsupported effect");
            return mod::EffectCode::kNoEffect;

        // ProTracker-specific effects:
        case mod::Effects::kSetSampleOffset:
            return mod::EffectCode::kSetSampleOffset;
        case mod::Effects::kSetVolume:
            return mod::EffectCode::kSetVolume;
        case mod::Effects::kSetFilter:
            return mod::EffectCode::kSetFilter;
        case mod::Effects::kFineSlideUp:
            return mod::EffectCode::kFineSlideUp;
        case mod::Effects::kFineSlideDown:
            return mod::EffectCode::kFineSlideDown;
        case mod::Effects::kSetGlissando:
            return mod::EffectCode::kSetGlissando;
        case mod::Effects::kSetVibratoWaveform:
            return mod::EffectCode::kSetVibratoWaveform;
        case mod::Effects::kSetFinetune:
            return mod::EffectCode::kSetFinetune;
        case mod::Effects::kLoopPattern:
            return mod::EffectCode::kLoopPattern;
        case mod::Effects::kSetTremoloWaveform:
            return mod::EffectCode::kSetTremoloWaveform;
        case mod::Effects::kFineVolSlideUp:
            return mod::EffectCode::kFineVolSlideUp;
        case mod::Effects::kFineVolSlideDown:
            return mod::EffectCode::kFineVolSlideDown;
        case mod::Effects::kDelayPattern:
            return mod::EffectCode::kDelayPattern;
        case mod::Effects::kInvertLoop:
            return mod::EffectCode::kInvertLoop;

        default:
            assert(false && "Unknown effect");
            return mod::EffectCode::kNoEffect;
    }
}

///// DMF --> MOD Sample Mapper

DMFSampleMapper::DMFSampleMapper()
{
    dmf_sound_index_ = SoundIndex<DMF>::None{};
    mod_sound_indexes_.fill(0);
    mod_sample_lengths_.fill(0);
    range_start_.clear();
    num_mod_samples_ = 0;
    sample_type_ = SampleType::kSilence;
    downsampling_needed_ = false;
    mod_octave_shift_ = 0;
}

SoundIndexType<MOD> DMFSampleMapper::Init(SoundIndexType<DMF> dmf_sound_index, SoundIndexType<MOD> starting_sound_index, const std::pair<Note, Note>& dmf_note_range)
{
    // Determines how to split up a DMF sound index into MOD sample(s). Returns the next free MOD sample id.

    // It's a Square or WAVE sample
    switch (dmf_sound_index.index())
    {
        case SoundIndex<DMF>::kSquare:
            sample_type_ = SampleType::kSquare;
            break;
        case SoundIndex<DMF>::kWave:
            sample_type_ = SampleType::kWave;
            break;
        default:
            assert(0);
            break;
    }

    dmf_sound_index_ = dmf_sound_index;

    const Note& lowest_note = dmf_note_range.first;
    const Note& highest_note = dmf_note_range.second;

    // Note ranges always start on a C, so get nearest C note (in downwards direction):
    const Note lowest_note_nearest_c = Note{NotePitch::kC, lowest_note.octave}; // DMF note

    // Get the number of MOD samples needed
    const int range = GetNoteRange(lowest_note_nearest_c, highest_note);
    if (range <= 36) // Only one MOD note range needed
        num_mod_samples_ = 1;
    else if (range <= 72)
        num_mod_samples_ = 2;
    else
        num_mod_samples_ = 3;

    range_start_.clear();

    // Initializing for 3 MOD samples is always the same:
    if (num_mod_samples_ == 3)
    {
        range_start_.push_back(Note{NotePitch::kC, 0});
        range_start_.push_back(Note{NotePitch::kC, 2});
        range_start_.push_back(Note{NotePitch::kC, 5});
        mod_sample_lengths_[0] = 256;
        mod_sample_lengths_[1] = 64;
        mod_sample_lengths_[2] = 8;

        // For whatever reason, wave samples need to be transposed down one octave to match their sound in Deflemask
        if (sample_type_ == SampleType::kWave)
        {
            mod_sample_lengths_[0] *= 2;
            mod_sample_lengths_[1] *= 2;
            mod_sample_lengths_[2] *= 2;
        }

        downsampling_needed_ = sample_type_ == SampleType::kWave; // Only wavetables are downsampled
        mod_octave_shift_ = 0;
        mod_sound_indexes_[0] = starting_sound_index;
        mod_sound_indexes_[1] = starting_sound_index + 1;
        mod_sound_indexes_[2] = starting_sound_index + 2;
        return starting_sound_index + 3;
    }

    // From here on, 1 or 2 MOD samples are needed

    // If we can, shift RangeStart lower to possibly prevent the need for downsampling:
    Note lowest_possible_range_start = lowest_note_nearest_c; // DMF note
    int possible_shift_amount = 0;

    Note current_high_end = lowest_note_nearest_c; // DMF note
    current_high_end.octave += 3;
    if (num_mod_samples_ == 2)
        current_high_end.octave += 3;

    assert(current_high_end > highest_note);

    const int high_end_slack = GetNoteRange(highest_note, current_high_end) - 1;
    if (high_end_slack > 24 && lowest_note_nearest_c.octave >= 2) // 2 octaves of slack at upper end, plus room to shift at bottom
        possible_shift_amount = 2; // Can shift MOD notes down 2 octaves
    else if (high_end_slack > 12 && lowest_note_nearest_c.octave >= 1) // 1 octave of slack at upper end, plus room to shift at bottom
        possible_shift_amount = 1; // Can shift MOD notes down 1 octave

    // Apply octave shift
    lowest_possible_range_start.octave -= possible_shift_amount;
    mod_octave_shift_ = possible_shift_amount;

    /*
    TODO: Whenever shifting is possible and num_mod_samples_ > 1, overlapping
        note ranges are possible. This hasn't been implemented, but within the
        intersection of two note ranges, 2 different MOD samples would be valid.
        If they were chosen intelligently, the number of sample changes (and
        consequently channel volume resets) could be reduced, which could lead
        to fewer necessary volume effects.
    */

    // Map of range start to required sample length:
    // C-0 --> 256
    // C-1 --> 128
    // C-2 --> 64
    // C-3 --> 32
    // C-4 --> 16
    // C-5 --> 8
    // DMF wavetables are 32 samples long, so downsampling is needed for MOD sample lengths of 16 or 8.

    unsigned octave_to_sample_length_map[6] = 
    {
        256,
        128,
        64,
        32,
        16,
        8
    };

    // Set Range Start and Sample Length for 1st MOD sample:
    range_start_.push_back(lowest_possible_range_start);
    mod_sample_lengths_[0] = octave_to_sample_length_map[range_start_[0].octave];

    // For whatever reason, wave samples need to be transposed down one octave to match their sound in Deflemask
    if (sample_type_ == SampleType::kWave)
        mod_sample_lengths_[0] *= 2;

    downsampling_needed_ = mod_sample_lengths_[0] < 32 && sample_type_ == SampleType::kWave;
    mod_sound_indexes_[0] = starting_sound_index;

    // Set Range Start and Sample Length for 2nd MOD sample (if it exists):
    if (num_mod_samples_ == 2)
    {
        range_start_.push_back({NotePitch::kC, uint8_t(lowest_possible_range_start.octave + 3)});
        mod_sample_lengths_[1] = octave_to_sample_length_map[range_start_[1].octave];

        // For whatever reason, wave samples need to be transposed down one octave to match their sound in Deflemask
        if (sample_type_ == SampleType::kWave)
            mod_sample_lengths_[1] *= 2;

        if (mod_sample_lengths_[1] < 32 && sample_type_ == SampleType::kWave)
            downsampling_needed_ = true;
        
        mod_sound_indexes_[1] = starting_sound_index + 1;
        return starting_sound_index + 2; // Two MOD samples were needed
    }

    return starting_sound_index + 1; // Only 1 MOD sample was needed
}

SoundIndexType<MOD> DMFSampleMapper::InitSilence()
{
    // Set up for a silent sample
    sample_type_ = SampleType::kSilence;
    range_start_.clear();
    num_mod_samples_ = 1;
    mod_sample_lengths_ = {8, 0, 0};
    downsampling_needed_ = false;
    mod_octave_shift_ = 0;
    dmf_sound_index_ = SoundIndex<DMF>::None{};
    mod_sound_indexes_ = {1, 0, 0}; // Silent sample is always MOD sample #1
    return 2; // The next available MOD sample id
}

Note DMFSampleMapper::GetMODNote(const Note& dmf_note, NoteRange& mod_note_range) const
{
    // Returns the MOD note to use given a DMF note. Also returns which
    //      MOD sample the MOD note needs to use. The MOD note's octave
    //      and pitch should always be exactly what gets displayed in ProTracker.

    Note mod_note{NotePitch::kC, (uint16_t)1};
    mod_note_range = NoteRange::kFirst;

    if (sample_type_ == SampleType::kSilence)
        return mod_note;

    mod_note_range = GetMODNoteRange(dmf_note);
    const Note& range_start = range_start_[static_cast<int>(mod_note_range)];

    mod_note.pitch = dmf_note.pitch;
    mod_note.octave = dmf_note.octave - range_start.octave + 1;
    // NOTE: The octave shift is already factored into range_start.
    //          The "+ 1" is because MOD's range starts at C-1 not C-0.

    assert(mod_note.octave >= 1 && "Note octave is too low.");
    assert(mod_note.octave <= 3 && "Note octave is too high.");

    return mod_note;
}

DMFSampleMapper::NoteRange DMFSampleMapper::GetMODNoteRange(const Note& dmf_note) const
{
    // Returns which MOD sample in the collection (1st, 2nd, or 3rd) should be used for the given DMF note
    // Assumes dmf_note is a valid note for this MOD sample collection

    if (num_mod_samples_ == 1)
        return NoteRange::kFirst; // The only option

    const uint16_t& octave_of_nearest_c = dmf_note.octave;

    if (octave_of_nearest_c < range_start_[1].octave)
    {
        // It is the lowest MOD sample
        return NoteRange::kFirst;
    }
    else if (num_mod_samples_ == 2)
    {
        // The only other option when there are just two choices is the 2nd one
        return NoteRange::kSecond;
    }
    else if (octave_of_nearest_c < range_start_[2].octave)
    {
        // Must be the middle of the 3 MOD samples
        return NoteRange::kSecond;
    }
    else
    {
        // Last option
        return NoteRange::kThird;
    }
}

SoundIndexType<MOD> DMFSampleMapper::GetMODSampleId(const Note& dmf_note) const
{
    // Returns the MOD sample id that would be used for the given DMF note
    // Assumes dmf_note is a valid note for this MOD sample collection
    const NoteRange note_range = GetMODNoteRange(dmf_note);
    return GetMODSampleId(note_range);
}

SoundIndexType<MOD> DMFSampleMapper::GetMODSampleId(NoteRange mod_note_range) const
{
    // Returns the MOD sample id of the given MOD sample in the collection (1st, 2nd, or 3rd)
    const int mod_note_range_int = static_cast<int>(mod_note_range);
    assert(mod_note_range_int + 1 <= num_mod_samples_ && "In SampleMapper::GetMODSampleId: The provided MOD note range is invalid for this SampleMapper object.");
    return mod_sound_indexes_[mod_note_range_int];
}

unsigned DMFSampleMapper::GetMODSampleLength(NoteRange mod_note_range) const
{
    // Returns the sample length of the given MOD sample in the collection (1st, 2nd, or 3rd)
    const int mod_note_range_int = static_cast<int>(mod_note_range);
    assert(mod_note_range_int + 1 <= num_mod_samples_ && "In SampleMapper::GetMODSampleLength: The provided MOD note range is invalid for this SampleMapper object.");
    return mod_sample_lengths_[mod_note_range_int];
}

DMFSampleMapper::NoteRange DMFSampleMapper::GetMODNoteRange(SoundIndexType<MOD> mod_sound_index) const
{
    // Returns note range (1st, 2nd, or 3rd MOD sample in the collection) given the MOD sample id (sound index)
    switch (mod_sound_index - mod_sound_indexes_[0])
    {
        case 0:
            return NoteRange::kFirst;
        case 1:
            return NoteRange::kSecond;
        case 2:
            return NoteRange::kThird;
        default:
            throw std::range_error("In SampleMapper::GetMODNoteRange: The provided MOD sample id was invalid for this SampleMapper object.");
    }
}

DMFSampleMapper::NoteRangeName DMFSampleMapper::GetMODNoteRangeName(NoteRange mod_note_range) const
{
    // Gets NoteRange which can be used for printing purposes
    switch (mod_note_range)
    {
        case NoteRange::kFirst:
            if (num_mod_samples_ == 1)
                return NoteRangeName::kNone;
            return NoteRangeName::kLow;
        case NoteRange::kSecond:
            if (num_mod_samples_ == 2)
                return NoteRangeName::kHigh;
            return NoteRangeName::kMiddle;
        case NoteRange::kThird:
            return NoteRangeName::kHigh;
        default:
            throw std::range_error("In SampleMapper::GetMODNoteRangeName: The provided MOD note range is invalid for this SampleMapper object.");
    }
}

///////// EXPORT /////////

void MOD::ExportImpl(const std::string& filename)
{
    std::ofstream out_file(filename, std::ios::binary);
    if (!out_file.is_open())
    {
        throw MODException(ModuleException::Category::kExport, ModuleException::ExportError::kFileOpen);
    }

    ExportModuleName(out_file);
    ExportSampleInfo(out_file);
    ExportModuleInfo(out_file);
    ExportPatterns(out_file);
    ExportSampleData(out_file);

    out_file.close();

    const bool verbose = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kVerbose).GetValue<bool>();
    if (verbose)
        std::cout << "Saved MOD file to disk.\n\n";
}

void MOD::ExportModuleName(std::ofstream& fout) const
{
    // Print module name, truncating or padding with zeros as needed
    const std::string& title = GetTitle();
    for (unsigned i = 0; i < 20; i++)
    {
        if (i < title.size())
        {
            fout.put(title[i]);
        }
        else
        {
            fout.put(0);
        }
    }
}

void MOD::ExportSampleInfo(std::ofstream& fout) const
{
    for (const auto& [discard, sample] : samples_)
    {
        if (sample.name.size() > 22)
            throw std::length_error("Sample name must be 22 characters or less");

        // Pad name with spaces
        std::string name_copy = sample.name;
        while (name_copy.size() < 22)
            name_copy += " ";

        fout << name_copy;

        fout.put(sample.length >> 9);       // Length byte 0
        fout.put(sample.length >> 1);       // Length byte 1
        fout.put(sample.finetune);          // Finetune value !!!
        fout.put(sample.volume);            // Sample volume // TODO: Optimize this?
        fout.put(sample.repeat_offset >> 9); // Repeat offset byte 0
        fout.put(sample.repeat_offset >> 1); // Repeat offset byte 1
        fout.put(sample.repeat_length >> 9); // Sample repeat length byte 0
        fout.put(sample.repeat_length >> 1); // Sample repeat length byte 1
    }

    // The remaining samples are blank:
    for (int i = total_mod_samples_; i < 31; i++)
    {
        if (i != 30)
        {
            // According to real ProTracker files viewed in a hex viewer, the 30th and final byte
            //    of a blank sample is 0x01 and all 29 other bytes are 0x00.
            for (int j = 0; j < 29; j++)
            {
                fout.put(0);
            }
            fout.put(1);
        }
        else
        {
            // Print credits message in last sample's name
            std::string credits = "Made with dmf2mod";

            // Pad name with zeros
            while (credits.size() < 22)
                credits += " ";

            fout << credits;

            for (int j = 22; j < 29; j++)
            {
                fout.put(0);
            }
            fout.put(1);
        }
    }
}

void MOD::ExportModuleInfo(std::ofstream& fout) const
{
    const uint8_t num_orders = static_cast<uint8_t>(GetData().GetNumOrders());

    fout.put(num_orders); // Song length in patterns (not total number of patterns)
    fout.put(127);       // 0x7F - Useless byte that has to be here

    // Pattern matrix (Each ProTracker pattern number is the same as its pattern matrix row number)
    for (PatternIndex pattern_id : GetData().PatternMatrixRef())
    {
        fout.put(static_cast<uint8_t>(pattern_id));
    }
    for (uint8_t i = num_orders; i < 128; ++i)
    {
        fout.put(0);
    }

    fout << "M.K."; // ProTracker uses "M!K!" if there's more than 64 pattern matrix rows
}

void MOD::ExportPatterns(std::ofstream& fout) const
{
    const auto& mod_data = GetData();
    for (const auto& pattern : mod_data.PatternsRef())
    {
        for (RowIndex row = 0; row < mod_data.GetNumRows(); ++row)
        {
            for (ChannelIndex channel = 0; channel < mod_data.GetNumChannels(); ++channel)
            {
                const Row<MOD>& row_data = pattern[row][channel];
                uint16_t period = 0;
                if (NoteHasPitch(row_data.note))
                {
                    const uint8_t octave = GetNote(row_data.note).octave;
                    const uint8_t pitch = static_cast<uint8_t>(GetNote(row_data.note).pitch);
                    period = kProTrackerPeriodTable[octave][pitch];
                }

                // Sample number (upper 4b); sample period/effect param. (lower 4b)
                fout.put((row_data.sample & 0xF0) | ((period & 0x0F00) >> 8));

                // Sample period/effect param. (lower 8 bits)
                fout.put(period & 0x00FF);

                //const uint16_t effect = ((uint16_t)rowData.EffectCode << 4) | rowData.EffectValue;

                // Convert dmf2mod internal effect code to MOD effect code
                const uint8_t effect_code = GetEffectCode(row_data.effect.code);

                // Sample number (lower 4b); effect code (upper 4b)
                fout.put((row_data.sample << 4) | (effect_code >> 4));

                // Effect code (lower 8 bits)
                fout.put(((effect_code << 4) & 0x00FF) | static_cast<uint8_t>(row_data.effect.value));
            }
        }
    }
}

void MOD::ExportSampleData(std::ofstream& fout) const
{
    for (const auto& sample_info : samples_)
    {
        const auto& sample_data = sample_info.second.data;
        for (int8_t value : sample_data)
        {
            fout.put(value);
        }
    }
}

///////// OTHER /////////

static std::string GetWarningMessage(MOD::ConvertWarning warning, const std::string& info)
{
    switch (warning)
    {
        case MOD::ConvertWarning::kPitchHigh:
            return "Cannot use the highest Deflemask note (C-8) on some MOD players including ProTracker.";
        case MOD::ConvertWarning::kTempoLow:
            return std::string("Tempo is too low. Using ~3.1 BPM instead.\n")
                    + std::string("         ProTracker only supports tempos between ~3.1 and 765 BPM.");
        case MOD::ConvertWarning::kTempoHigh:
            return std::string("Tempo is too high for ProTracker. Using 127.5 BPM instead.\n")
                    + std::string("         ProTracker only supports tempos between ~3.1 and 765 BPM.");
        case MOD::ConvertWarning::kTempoLowCompat:
            return std::string("Tempo is too low. Using 16 BPM to retain effect compatibility.\n")
                    + std::string("         Use --tempo=accuracy for the full tempo range.");
        case MOD::ConvertWarning::kTempoHighCompat:
            return std::string("Tempo is too high. Using 127.5 BPM to retain effect compatibility.\n")
                    + std::string("         Use --tempo=accuracy for the full tempo range.");
        case MOD::ConvertWarning::kTempoAccuracy:
            return "Tempo does not exactly match, but a value close to it is being used.";
        case MOD::ConvertWarning::kEffectIgnored:
            return "A Deflemask effect was ignored due to limitations of the MOD format.";
        case MOD::ConvertWarning::kWaveDownsample:
            return "Wavetable instrument #" + info + " was downsampled in MOD to allow higher notes to be played.";
        case MOD::ConvertWarning::kMultipleEffects:
            return "No more than one volume change or effect can appear in the same row of the same channel. Important effects will be prioritized.";
        case MOD::ConvertWarning::kLoopbackInaccuracy:
            return "Notes from one or more channels may erroneously carry over when looping back.";
        default:
            return "";
    }
}

std::string MODException::CreateErrorMessage(Category category, int error_code, const std::string& arg)
{
    switch (category)
    {
        case Category::kNone:
            return "No error.";
        case Category::kImport:
            return "No error.";
        case Category::kExport:
            return "No error.";
        case Category::kConvert:
            switch (error_code)
            {
                case (int)MOD::ConvertError::kSuccess:
                    return "No error.";
                case (int)MOD::ConvertError::kNotGameBoy:
                    return "Only the Game Boy system is currently supported.";
                case (int)MOD::ConvertError::kTooManyPatternMatrixRows:
                    return "Too many rows of patterns in the pattern matrix. 64 is the maximum. (63 if using Setup Pattern.)";
                case (int)MOD::ConvertError::kOver64RowPattern:
                    return std::string("Patterns must have 64 or fewer rows.\n")
                            + std::string("       A workaround for this issue is planned for a future update to dmf2mod.");
                default:
                    return arg;
            }
            break;
    }
    return "";
}
