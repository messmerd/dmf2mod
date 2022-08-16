/*
    mod.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Implements a ModuleInterface-derived class for ProTracker's
    MOD files.

    Several limitations apply in order to export. For example,
    for DMF-->MOD, the DMF file must use the Game Boy system,
    patterns must have 64 or fewer rows, etc.
*/

#include "mod.h"
#include "utils/utils.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <cstdio>
#include <set>
#include <cassert>

using namespace d2m;
using namespace d2m::mod;
// DO NOT use any module namespace other than d2m::mod

// Define the command-line options that MOD accepts:
using MODOptionEnum = MODConversionOptions::OptionEnum;
auto MODOptions = CreateOptionDefinitions(
{
    /* Type  / Option id                    / Full name    / Short / Default   / Possib. vals          / Description */
    {OPTION, MODOptionEnum::AmigaFilter,    "amiga",       '\0',   false,                              "Enables the Amiga filter"},
    {OPTION, MODOptionEnum::Arpeggio,       "arp",         '\0',   false,                              "Allow arpeggio effects"},
    {OPTION, MODOptionEnum::Portamento,     "port",        '\0',   false,                              "Allow portamento up/down effects"},
    {OPTION, MODOptionEnum::Port2Note,      "port2note",   '\0',   false,                              "Allow portamento to note effects"},
    {OPTION, MODOptionEnum::Vibrato,        "vib",         '\0',   false,                              "Allow vibrato effects"},
    {OPTION, MODOptionEnum::TempoType,      "tempo",       '\0',   "accuracy", {"accuracy", "compat"}, "Prioritize tempo accuracy or compatibility with effects"},
});

// Register module info
MODULE_DEFINE(MOD, MODConversionOptions, ModuleType::MOD, "ProTracker", "mod", MODOptions)

static std::vector<int8_t> GenerateSquareWaveSample(unsigned dutyCycle, unsigned length);
static std::vector<int8_t> GenerateWavetableSample(uint32_t* wavetableData, unsigned length);
static int16_t GetNewDMFVolume(int16_t dmfRowVol, const ChannelState& state);

static std::string GetWarningMessage(MOD::ConvertWarning warning, const std::string& info = "");

/*
    Game Boy's range is:  C-0 -> C-8 (though notes lower than C-2 just play as C-2)
    ProTracker's range is:  C-1 -> B-3  (plus octaves 0 and 4 which are non-standard) 
    See DMFSampleMapper for how this issue is resolved.
*/

static uint16_t proTrackerPeriodTable[5][12] = {
    {1712,1616,1525,1440,1357,1281,1209,1141,1077,1017, 961, 907},  /* C-0 to B-0 */
    {856,808,762,720,678,640,604,570,538,508,480,453},              /* C-1 to B-1 */
    {428,404,381,360,339,320,302,285,269,254,240,226},              /* C-2 to B-2 */
    {214,202,190,180,170,160,151,143,135,127,120,113},              /* C-3 to B-3 */
    {107,101, 95, 90, 85, 80, 76, 71, 67, 64, 60, 57}               /* C-4 to B-4 */
};

MOD::MOD() {}

void MOD::ImportRaw(const std::string& filename)
{
    // Not implemented
    throw NotImplementedException();
}

void MOD::ConvertRaw(const Module* input)
{
    if (!input)
    {
        throw MODException(ModuleException::Category::Convert, ModuleException::ConvertError::InvalidArgument);
    }

    switch (input->GetType())
    {
        case ModuleType::DMF:
            ConvertFromDMF(*(input->Cast<DMF>()));
            break;
        // Add other input types here if support is added
        default:
            // Unsupported input type for conversion to MOD
            throw MODException(ModuleException::Category::Convert, ModuleException::ConvertError::UnsupportedInputType, input->GetModuleInfo()->GetFileExtension());
    }
}

///////// CONVERT FROM DMF /////////

void MOD::ConvertFromDMF(const DMF& dmf)
{
    const bool verbose = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Verbose).GetValue<bool>();

    if (verbose)
        std::cout << "Starting to convert to MOD...\n";

    if (dmf.GetSystem().type != DMF::SystemType::GameBoy) // If it's not a Game Boy
    {
        throw MODException(ModuleException::Category::Convert, MOD::ConvertError::NotGameBoy);
    }

    const dmf::ModuleInfo& moduleInfo = dmf.GetModuleInfo();
    m_NumberOfRowsInPatternMatrix = moduleInfo.totalRowsInPatternMatrix + (int)m_UsingSetupPattern;
    if (m_NumberOfRowsInPatternMatrix > 64) // totalRowsInPatternMatrix is 1 more than it actually is
    {
        throw MODException(ModuleException::Category::Convert, MOD::ConvertError::TooManyPatternMatrixRows);
    }

    if (moduleInfo.totalRowsPerPattern > 64)
    {
        throw MODException(ModuleException::Category::Convert, MOD::ConvertError::Over64RowPattern);
    }

    m_NumberOfChannels = DMF::Systems(DMF::SystemType::GameBoy).channels;
    m_NumberOfChannelsPowOfTwo = 0;
    unsigned channels = m_NumberOfChannels;
    while (channels > 1)
    {
        channels >>= 1;
        m_NumberOfChannelsPowOfTwo++;
    }

    ///////////////// CONVERT SONG NAME

    m_ModuleName.clear();
    for (int i = 0; i < std::min((int)dmf.GetVisualInfo().songNameLength, 20); i++)
    {
        m_ModuleName += dmf.GetVisualInfo().songName[i];
    }

    ///////////////// CONVERT SAMPLE INFO

    if (verbose)
        std::cout << "Converting samples...\n";

    SampleMap sampleMap;
    DMFConvertSamples(dmf, sampleMap);

    ///////////////// CONVERT PATTERN DATA

    if (verbose)
        std::cout << "Converting pattern data...\n";

    DMFConvertPatterns(dmf, sampleMap);
    
    ///////////////// CLEAN UP

    if (verbose)
        std::cout << "Done converting to MOD.\n\n";
}

void MOD::DMFConvertSamples(const DMF& dmf, SampleMap& sampleMap)
{
    DMFSampleNoteRangeMap sampleIdLowestHighestNotesMap;
    DMFCreateSampleMapping(dmf, sampleMap, sampleIdLowestHighestNotesMap);
    DMFSampleSplittingAndAssignment(sampleMap, sampleIdLowestHighestNotesMap);
    DMFConvertSampleData(dmf, sampleMap);
}

void MOD::DMFCreateSampleMapping(const DMF& dmf, SampleMap& sampleMap, DMFSampleNoteRangeMap& sampleIdLowestHighestNotesMap)
{
    /*
     * This function loops through all DMF pattern contents to find the highest and lowest notes 
     *  for each square wave duty cycle and each wavetable. It also finds which SQW duty cycles are 
     *  used and which wavetables are used and stores this info in sampleMap.
     *  This extra processing is needed because Deflemask has over twice the note range that MOD has, so
     *  up to 3 MOD samples may be needed for MOD to achieve the same pitch range. These extra samples are 
     *  only used when needed.
     */

    sampleMap.clear();

    // Lowest/highest note for each square wave duty cycle or wavetable instrument
    sampleIdLowestHighestNotesMap.clear();

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.

    State state{dmf.GetTicksPerRowPair()};

    // The main MODChannelState structs should NOT update during patterns or parts of 
    //   patterns that the Position Jump (Bxx) effect skips over. (Ignore loops)
    //   Keep a copy of the main state for each channel, and once it reaches the 
    //   jump destination, overwrite the current state with the copied state.

    const dmf::ModuleInfo& moduleInfo = dmf.GetModuleInfo();

    // Assume that the Silent sample is always needed
    //  This is the easiest way to do things at the moment because a Note OFF may need to
    //  be inserted into the MOD file at the loopback point once the end of the song is
    //  reached to prevent notes from carrying over, but whether this is needed is not known
    //  until later.
    //  TODO: After a generated data system is added, it will be easier to check whether a silent sample is actually needed.

    sampleMap[-1] = {};

    // Most of the following nested for loop is copied from the export pattern data loop in DMFConvertPatterns.
    // I didn't want to do this, but I think having two of the same loop is the only simple way.
    // Loop through SQ1, SQ2, and WAVE channels:
    for (int chan = static_cast<int>(dmf::GameBoyChannel::SQW1); chan <= static_cast<int>(dmf::GameBoyChannel::WAVE); chan++)
    {
        state.global.channel = chan;

        // Loop through Deflemask patterns
        for (int patMatRow = 0; patMatRow < moduleInfo.totalRowsInPatternMatrix; patMatRow++)
        {
            state.global.order = patMatRow;

            // Row within pattern
            for (unsigned patRow = 0; patRow < moduleInfo.totalRowsPerPattern; patRow++)
            {
                state.global.patternRow = patRow;

                ChannelState& chanState = state.channel[chan];

                const auto& chanRow = dmf.GetData().GetRow(chan, patMatRow, patRow);

                // If just arrived at jump destination:
                if (patMatRow == state.global.jumpDestination && patRow == 0 && state.global.suspended)
                {
                    // Restore state copies
                    state.Restore();
                }

                PriorityEffectsMap modEffects;
                //mod_sample_id_t modSampleId = 0;
                //uint16_t period = 0;

                if (chan == static_cast<int>(dmf::GameBoyChannel::NOISE))
                {
                    modEffects = DMFConvertEffects_NoiseChannel(chanRow);
                    DMFUpdateStatePre(dmf, state, modEffects);
                    continue;
                }

                modEffects = DMFConvertEffects(chanRow, state);
                DMFUpdateStatePre(dmf, state, modEffects);
                DMFGetAdditionalEffects(dmf, state, chanRow, modEffects);

                //DMFConvertNote(state, chanRow, sampleMap, modEffects, modSampleId, period);

                // TODO: More state-related stuff could be extracted from DMFConvertNote and put into separate 
                //  method so that I don't have to copy code from it to put here.
                
                // Convert note - Note cut effect
                auto sampleChangeEffects = modEffects.equal_range(EffectPrioritySampleChange);
                if (sampleChangeEffects.first != sampleChangeEffects.second) // If sample change occurred (duty cycle, wave, or note cut effect)
                {
                    for (auto& iter = sampleChangeEffects.first; iter != sampleChangeEffects.second; )
                    {
                        Effect& modEffect = iter->second;
                        if (modEffect.effect == EffectCode::CutSample && modEffect.value == 0) // Note cut
                        {
                            // Silent sample is needed
                            if (sampleMap.count(-1) == 0)
                                sampleMap[-1] = {};

                            chanState.notePlaying = false;
                            chanState.currentNote = {};
                            iter = modEffects.erase(iter); // Consume the effect
                        }
                        else
                        {
                            // Only increment if an element wasn't erased
                            ++iter;
                        }
                    }
                }

                // Convert note - Note OFF
                if (NoteIsOff(chanRow.note)) // Note OFF. Use silent sample and handle effects.
                {
                    chanState.notePlaying = false;
                    chanState.currentNote = NoteTypes::Off{};
                }

                // A note on the SQ1, SQ2, or WAVE channels:
                if (NoteHasPitch(chanRow.note) && chan != dmf::GameBoyChannel::NOISE)
                {
                    const Note& dmfNote = GetNote(chanRow.note);
                    chanState.notePlaying = true;
                    chanState.currentNote = chanRow.note;

                    mod_sample_id_t sampleId = chan == dmf::GameBoyChannel::WAVE ? chanState.wavetable + 4 : chanState.dutyCycle;

                    // Mark this square wave or wavetable as used
                    sampleMap[sampleId] = {};

                    // Get lowest/highest notes
                    if (sampleIdLowestHighestNotesMap.count(sampleId) == 0) // 1st time
                    {
                        sampleIdLowestHighestNotesMap[sampleId] = std::pair<Note, Note>(dmfNote, dmfNote);
                    }
                    else
                    {
                        auto& notePair = sampleIdLowestHighestNotesMap[sampleId];
                        if (dmfNote > notePair.second)
                        {
                            // Found a new highest note
                            notePair.second = dmfNote;
                        }
                        if (dmfNote < notePair.first)
                        {
                            // Found a new lowest note
                            notePair.first = dmfNote;
                        }
                    }
                }
            }
        }
    }
}

void MOD::DMFSampleSplittingAndAssignment(SampleMap& sampleMap, const DMFSampleNoteRangeMap& sampleIdLowestHighestNotesMap)
{
    // This method determines whether a DMF sample will need to be split into low, middle, or high ranges in MOD, 
    //  then assigns MOD sample numbers, sample lengths, etc.
    
    mod_sample_id_t currentMODSampleId = 1; // Sample #0 is special in ProTracker

    // Only the samples we need will be in this map (+ the silent sample possibly)
    for (auto& [sampleId, sampleMapper] : sampleMap)
    {
        // Special handling of silent sample
        if (sampleId == -1)
        {
            // Silent sample is always sample #1 if it is used
            currentMODSampleId = sampleMapper.InitSilence();
            continue;
        }

        const auto& lowHighNotes = sampleIdLowestHighestNotesMap.at(sampleId);
        currentMODSampleId = sampleMapper.Init(sampleId, currentMODSampleId, lowHighNotes);

        if (sampleMapper.IsDownsamplingNeeded())
        {
            m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::WaveDownsample, std::to_string(sampleId - 4)));
        }
    }

    m_TotalMODSamples = currentMODSampleId - 1; // Set the number of MOD samples that will be needed. (minus sample #0 which is special)
    
    // TODO: Check if there are too many samples needed here, and throw exception if so
}

void MOD::DMFConvertSampleData(const DMF& dmf, const SampleMap& sampleMap)
{
    // Fill out information needed to define a MOD sample
    m_Samples.clear();
    
    for (const auto& [dmfSampleId, sampleMapper] : sampleMap)
    {
        const int totalNoteRanges = sampleMapper.GetNumMODSamples();

        for (int noteRangeInt = 0; noteRangeInt < totalNoteRanges; noteRangeInt++)
        {
            auto noteRange = static_cast<DMFSampleMapper::NoteRange>(noteRangeInt);

            Sample si;

            si.id = sampleMapper.GetMODSampleId(noteRange);
            si.length = sampleMapper.GetMODSampleLength(noteRange);
            si.repeatLength = si.length;
            si.repeatOffset = 0;
            si.finetune = 0;

            si.name = "";

            // Set sample data specific to the sample type:
            using SampleType = DMFSampleMapper::SampleType;
            switch (sampleMapper.GetSampleType())
            {
                case SampleType::Silence:
                    si.name = "Silence";
                    si.volume = 0;
                    si.data = std::vector<int8_t>(si.length, 0);
                    break;

                case SampleType::Square:
                    si.name = "SQW, Duty ";
                    switch (dmfSampleId)
                    {
                        case 0: si.name += "12.5%"; break;
                        case 1: si.name += "25%"; break;
                        case 2: si.name += "50%"; break;
                        case 3: si.name += "75%"; break;
                    }
                    si.volume = VolumeMax; // TODO: Optimize this?
                    si.data = GenerateSquareWaveSample(dmfSampleId, si.length);
                    break;

                case SampleType::Wave:
                    const int wavetableIndex = dmfSampleId - 4;

                    si.name = "Wavetable #";
                    si.name += std::to_string(wavetableIndex);
                
                    si.volume = VolumeMax; // TODO: Optimize this?

                    uint32_t* wavetableData = dmf.GetWavetableValues()[wavetableIndex];
                    si.data = GenerateWavetableSample(wavetableData, si.length);
                    break;
            }

            // Append note range text to the sample name:
            using NoteRangeName = DMFSampleMapper::NoteRangeName;
            switch (sampleMapper.GetMODNoteRangeName(noteRange))
            {
                case NoteRangeName::None:
                    break;
                case NoteRangeName::Low:
                    si.name += " (low)"; break;
                case NoteRangeName::Middle:
                    si.name += " (mid)"; break;
                case NoteRangeName::High:
                    si.name += " (high)"; break;
            }

            if (si.name.size() > 22)
                throw std::invalid_argument("Sample name must be 22 characters or less");

            // Pad name with zeros
            while (si.name.size() < 22)
                si.name += " ";
            
            m_Samples[si.id] = si;
        }
    }
}

std::vector<int8_t> GenerateSquareWaveSample(unsigned dutyCycle, unsigned length)
{
    std::vector<int8_t> sample;
    sample.assign(length, 0);

    uint8_t duty[] = {1, 2, 4, 6};
    
    // This loop creates a square wave with the correct length and duty cycle:
    for (unsigned i = 1; i <= length; i++)
    {
        if ((i * 8.f) / length <= (float)duty[dutyCycle])
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

std::vector<int8_t> GenerateWavetableSample(uint32_t* wavetableData, unsigned length)
{
    std::vector<int8_t> sample;
    sample.assign(length, 0);

    const float maxVolCap = 12.f / 15.f; // Set WAVE max volume to 12/15 of potential max volume to emulate DMF wave channel

    for (unsigned i = 0; i < length; i++)
    {
        // Note: For the Deflemask Game Boy system, all wavetable lengths are 32.
        // Converting from DMF sample values (0 to 15) to PT sample values (-128 to 127).
        switch (length)
        {
            case 512: // x16
                sample[i] = (int8_t)(((wavetableData[i / 16] / 15.f * 255.f) - 128.f) * maxVolCap); break;
            case 256: // x8
                sample[i] = (int8_t)(((wavetableData[i / 8] / 15.f * 255.f) - 128.f) * maxVolCap); break;
            case 128: // x4
                sample[i] = (int8_t)(((wavetableData[i / 4] / 15.f * 255.f) - 128.f) * maxVolCap); break;
            case 64:  // x2
                sample[i] = (int8_t)(((wavetableData[i / 2] / 15.f * 255.f) - 128.f) * maxVolCap); break;
            case 32:  // Original length
                sample[i] = (int8_t)(((wavetableData[i] / 15.f * 255.f) - 128.f) * maxVolCap); break;
            case 16:  // Half length (loss of information from downsampling)
            {
                // Take average of every two sample values to make new sample value
                const unsigned sum = wavetableData[i * 2] + wavetableData[(i * 2) + 1];
                sample[i] = (int8_t)(((sum / (15.f * 2) * 255.f) - 128.f) * maxVolCap);
                break;
            }
            case 8:   // Quarter length (loss of information from downsampling)
            {
                // Take average of every four sample values to make new sample value
                const unsigned sum = wavetableData[i * 4] + wavetableData[(i * 4) + 1] + wavetableData[(i * 4) + 2] + wavetableData[(i * 4) + 3];
                sample[i] = (int8_t)(((sum / (15.f * 4) * 255.f) - 128.f) * maxVolCap);
                break;
            }
            default:
                // ERROR: Invalid length
                throw std::invalid_argument("Invalid value for length in GenerateWavetableSample(): " + std::to_string(length));
        }
    }

    return sample;
}

void MOD::DMFConvertPatterns(const DMF& dmf, const SampleMap& sampleMap)
{
    m_Patterns.assign(m_NumberOfRowsInPatternMatrix, {});

    unsigned initialTempo, initialSpeed; // Together these will set the initial BPM
    DMFConvertInitialBPM(dmf, initialTempo, initialSpeed);

    if (m_UsingSetupPattern)
    {
        m_Patterns[0].assign(64 * m_NumberOfChannels, {0, 0, 0, 0});

        // Set initial tempo
        ChannelRow tempoRow;
        tempoRow.SampleNumber = 0;
        tempoRow.SamplePeriod = 0;
        tempoRow.EffectCode = EffectCode::SetSpeed;
        tempoRow.EffectValue = initialTempo;
        SetChannelRow(0, 0, 0, tempoRow);

        // Set initial speed
        if (GetOptions()->GetTempoType() != MODConversionOptions::TempoType::EffectCompatibility)
        {
            ChannelRow speedRow;
            speedRow.SampleNumber = 0;
            speedRow.SamplePeriod = 0;
            speedRow.EffectCode = EffectCode::SetSpeed;
            speedRow.EffectValue = initialSpeed;
            SetChannelRow(0, 0, 1, speedRow);
        }

        // Set Pattern Break to start of song
        ChannelRow posJumpRow;
        posJumpRow.SampleNumber = 0;
        posJumpRow.SamplePeriod = 0;
        posJumpRow.EffectCode = EffectCode::PatBreak;
        posJumpRow.EffectValue = 0;
        SetChannelRow(0, 0, 2, posJumpRow);

        // Set Amiga Filter
        ChannelRow amigaFilterRow;
        amigaFilterRow.SampleNumber = 0;
        amigaFilterRow.SamplePeriod = 0;
        amigaFilterRow.EffectCode = EffectCode::SetFilter;
        amigaFilterRow.EffectValue = !GetOptions()->GetOption(MODOptionEnum::AmigaFilter).GetValue<bool>();
        SetChannelRow(0, 0, 3, amigaFilterRow);

        // All other channel rows in the pattern are already zeroed out so nothing needs to be done for them
    }

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.

    State state{dmf.GetTicksPerRowPair()};

    // The main MODChannelState structs should NOT update during patterns or parts of 
    //   patterns that the Position Jump (Bxx) effect skips over. (Ignore loops)
    //   Keep a copy of the main state for each channel, and once it reaches the 
    //   jump destination, overwrite the current state with the copied state.

    const int dmfTotalRowsInPatternMatrix = dmf.GetModuleInfo().totalRowsInPatternMatrix;
    const unsigned dmfTotalRowsPerPattern = dmf.GetModuleInfo().totalRowsPerPattern;

    // Loop through ProTracker pattern matrix rows (corresponds to DMF pattern numbers):
    for (int patMatRow = 0; patMatRow < dmfTotalRowsInPatternMatrix; patMatRow++)
    {
        state.global.order = patMatRow;

        // patMatRow is in DMF pattern matrix rows, not MOD
        m_Patterns[patMatRow + (int)m_UsingSetupPattern].assign(64 * m_NumberOfChannels, {});

        // Loop through rows in a pattern:
        for (unsigned patRow = 0; patRow < dmfTotalRowsPerPattern; patRow++)
        {
            state.global.patternRow = patRow;
            state.global.channelIndependentEffect = {EffectCode::NoEffectCode, EffectCode::NoEffectVal};

            // Use Pattern Break effect to allow for patterns that are less than 64 rows
            if (dmfTotalRowsPerPattern < 64 && patRow + 1 == dmfTotalRowsPerPattern /*&& !stateSuspended*/)
                state.global.channelIndependentEffect = {EffectCode::PatBreak, 0};

            // Clear channel rows
            for (unsigned chan = 0; chan < m_NumberOfChannels; chan++)
            {
                state.channelRows[chan] = {};
            }

            // Loop through channels:
            for (unsigned chan = 0; chan < m_NumberOfChannels; chan++)
            {
                state.global.channel = chan;

                const auto& chanRow = dmf.GetData().GetRow(chan, patMatRow, patRow);

                // If just arrived at jump destination:
                if (patMatRow == state.global.jumpDestination && patRow == 0 && state.global.suspended)
                {
                    // Restore state copies
                    state.Restore();
                }

                PriorityEffectsMap modEffects;
                mod_sample_id_t modSampleId = 0;
                uint16_t period = 0;

                if (chan == dmf::GameBoyChannel::NOISE)
                {
                    modEffects = DMFConvertEffects_NoiseChannel(chanRow);
                    DMFUpdateStatePre(dmf, state, modEffects);
                }
                else
                {
                    modEffects = DMFConvertEffects(chanRow, state);
                    DMFUpdateStatePre(dmf, state, modEffects);
                    DMFGetAdditionalEffects(dmf, state, chanRow, modEffects);
                    DMFConvertNote(state, chanRow, sampleMap, modEffects, modSampleId, period);
                }

                state.channelRows[chan] = DMFApplyNoteAndEffect(state, modEffects, modSampleId, period);
            }

            // Set the channel rows for the current pattern row all at once
            for (unsigned chan = 0; chan < m_NumberOfChannels; chan++)
            {
                SetChannelRow(patMatRow + (int)m_UsingSetupPattern, patRow, chan, state.channelRows[chan]);
            }

            // TODO: Better channel independent effects implementation

        }

        // If the DMF has less than 64 rows per pattern, there will be extra MOD rows which will need to be blank
        for (unsigned patRow = dmfTotalRowsPerPattern; patRow < 64; patRow++)
        {
            // Loop through channels:
            for (unsigned chan = 0; chan < m_NumberOfChannels; chan++)
            {
                ChannelRow tempChannelRow = {0, 0, 0, 0};
                SetChannelRow(patMatRow + (int)m_UsingSetupPattern, patRow, chan, tempChannelRow);
            }
        }
    }
}

PriorityEffectsMap MOD::DMFConvertEffects(const Row<DMF>& row, State& state)
{
    const auto& options = GetOptions();
    auto& channelState = state.channel[state.global.channel];
    auto& persistentEffects = channelState.persistentEffects;

    PriorityEffectsMap modEffects;

    if (options->AllowEffects())
    {
        if (!NoteIsEmpty(row.note))
        {
            // Portamento to note stops when next note is reached or on Note OFF
            persistentEffects.erase(EffectPriorityPort2Note);
        }

        /*
        * In spite of what the Deflemask manual says, portamento effects are automatically turned off if they
        *  stay on long enough without a new note being played. I believe it's until C-2 is reached for port down.
        * See order 0x14 in the "i wanna eat my ice cream alone (reprise)" demo song for an example of this behavior.
        * In that order, for the F-2 note on SQ2, the port down effect turns off automatically if the next note
        *   comes 21 or more rows later. The number of rows it takes depends on the note pitch, port effect parameter,
        * and the tempo denominator (Speed A/B and the base time).
        * See DMF::GetRowsUntilPortDownAutoOff(...) for an experimentally determined approximation for port down.
        * TODO: Create approximation for port up.
        */
        if (channelState.rowsUntilPortAutoOff >= 0)
        {
            if (channelState.rowsUntilPortAutoOff == 0)
            {
                // Automatically turn port effects off
                persistentEffects.erase(EffectPriorityPortUp);
                persistentEffects.erase(EffectPriorityPortDown);
                channelState.rowsUntilPortAutoOff = -1;
            }
            else if (NoteHasPitch(row.note))
            {
                // Reset the time until port effects automatically turn off
                if (channelState.portDirection == ChannelState::PORT_UP)
                    channelState.rowsUntilPortAutoOff = DMF::GetRowsUntilPortUpAutoOff(state.global.ticksPerRowPair, row.note, channelState.portParam);
                else
                    channelState.rowsUntilPortAutoOff = DMF::GetRowsUntilPortDownAutoOff(state.global.ticksPerRowPair, row.note, channelState.portParam);
            }
            else
            {
                channelState.rowsUntilPortAutoOff--;
            }
        }
    }

    // Convert DMF effects to MOD effects
    for (const auto& dmfEffect : row.effect)
    {
        if (dmfEffect.code == dmf::EffectCode::NoEffect)
            continue;

        switch (dmfEffect.code)
        {
            case dmf::EffectCode::Arp:
            {
                if (!options->AllowArpeggio()) break;
                persistentEffects.erase(EffectPriorityArp);
                if (dmfEffect.value <= 0)
                    break;
                persistentEffects.emplace(EffectPriorityArp, Effect{EffectCode::Arp, (uint16_t)dmfEffect.value});
                break;
            }
            case dmf::EffectCode::PortUp:
            {
                if (!options->AllowPortamento()) break;
                persistentEffects.erase(EffectPriorityPortUp);
                persistentEffects.erase(EffectPriorityPortDown);
                persistentEffects.erase(EffectPriorityPort2Note);
                if (dmfEffect.value <= 0)
                {
                    channelState.rowsUntilPortAutoOff = -1;
                    break;
                }
                persistentEffects.emplace(EffectPriorityPortUp, Effect{EffectCode::PortUp, (uint16_t)dmfEffect.value});
                if (channelState.rowsUntilPortAutoOff == -1)
                    channelState.rowsUntilPortAutoOff = DMF::GetRowsUntilPortUpAutoOff(state.global.ticksPerRowPair, NoteHasPitch(row.note) ? row.note : channelState.currentNote, dmfEffect.value);
                channelState.portDirection = ChannelState::PORT_UP;
                channelState.portParam = dmfEffect.value;
                break;
            }
            case dmf::EffectCode::PortDown:
            {
                if (!options->AllowPortamento()) break;
                persistentEffects.erase(EffectPriorityPortUp);
                persistentEffects.erase(EffectPriorityPortDown);
                persistentEffects.erase(EffectPriorityPort2Note);
                if (dmfEffect.value <= 0) // TODO: What is the behavior if there are two DMF port effects - one up and the other down?
                {
                    channelState.rowsUntilPortAutoOff = -1;
                    break;
                }
                persistentEffects.emplace(EffectPriorityPortDown, Effect{EffectCode::PortDown, (uint16_t)dmfEffect.value});
                if (channelState.rowsUntilPortAutoOff == -1)
                    channelState.rowsUntilPortAutoOff = DMF::GetRowsUntilPortDownAutoOff(state.global.ticksPerRowPair, NoteHasPitch(row.note) ? row.note : channelState.currentNote, dmfEffect.value);
                channelState.portDirection = ChannelState::PORT_DOWN;
                channelState.portParam = dmfEffect.value;
                break;
            }
            case dmf::EffectCode::Port2Note:
            {
                if (!options->AllowPort2Note()) break;
                persistentEffects.erase(EffectPriorityPortUp);
                persistentEffects.erase(EffectPriorityPortDown);
                persistentEffects.erase(EffectPriorityPort2Note);
                if (dmfEffect.value <= 0)
                {
                    channelState.rowsUntilPortAutoOff = -1;
                    break;
                }
                persistentEffects.emplace(EffectPriorityPort2Note, Effect{EffectCode::Port2Note, (uint16_t)dmfEffect.value});
                break;
            }
            case dmf::EffectCode::Vibrato:
            {
                if (!options->AllowVibrato()) break;
                persistentEffects.erase(EffectPriorityVibrato);
                if (dmfEffect.value <= 0)
                    break;
                persistentEffects.emplace(EffectPriorityVibrato, Effect{EffectCode::Vibrato, (uint16_t)dmfEffect.value});
                break;
            }
            case dmf::EffectCode::NoteCut:
            {
                if (dmfEffect.value == 0)
                {
                    // Can be implemented as silent sample
                    modEffects.emplace(EffectPrioritySampleChange, Effect{EffectCode::CutSample, 0});
                }
                break;
            }
            case dmf::EffectCode::PatBreak:
            {
                if (dmfEffect.value == 0)
                {
                    // Only D00 is supported at the moment
                    modEffects.emplace(EffectPriorityStructureRelated, Effect{EffectCode::PatBreak, 0});
                }
                break;
            }
            case dmf::EffectCode::PosJump:
            {
                const int dest = dmfEffect.value + (int)m_UsingSetupPattern; // Into MOD order value, not DMF
                modEffects.emplace(EffectPriorityStructureRelated, Effect{EffectCode::PosJump, (uint16_t)dest});
                break;
            }

            default:
                break; // Unsupported effect
        }

        switch (dmfEffect.code)
        {
            case dmf::GameBoyEffectCode::SetDutyCycle:
                modEffects.emplace(EffectPrioritySampleChange, Effect{EffectCode::DutyCycleChange, (uint16_t)dmfEffect.value});
                break;
            case dmf::GameBoyEffectCode::SetWave:
                modEffects.emplace(EffectPrioritySampleChange, Effect{EffectCode::WavetableChange, (uint16_t)(dmfEffect.value)});
                break;
            default:
                break; // Unsupported effect
        }
    }

    for (auto& mapPair : persistentEffects)
    {
        modEffects.insert(mapPair);
    }

    return modEffects;
}

PriorityEffectsMap MOD::DMFConvertEffects_NoiseChannel(const Row<DMF>& row)
{
    // Temporary method until the Noise channel is supported.
    // Only Pattern Break and Jump effects on the noise channel are converted for now.

    PriorityEffectsMap modEffects;

    for (auto& dmfEffect : row.effect)
    {
        if (dmfEffect.code == dmf::EffectCode::PatBreak && dmfEffect.value == 0)
        {
            // Only D00 is supported at the moment
            modEffects.insert({EffectPriorityStructureRelated, {EffectCode::PatBreak, 0}});
            break;
        }
        else if (dmfEffect.code == dmf::EffectCode::PosJump)
        {
            const int dest = dmfEffect.value + (int)m_UsingSetupPattern; // Into MOD order value, not DMF
            modEffects.insert({EffectPriorityStructureRelated, {EffectCode::PosJump, (uint16_t)dest}});
            break;
        }
    }
    return modEffects;
}

void MOD::DMFUpdateStatePre(const DMF& dmf, State& state, const PriorityEffectsMap& modEffects)
{
    // This method updates Structure and Sample Change state info.

    ChannelState& chanState = state.channel[state.global.channel];

    // Update structure-related state info

    auto structureEffects = modEffects.equal_range(EffectPriority::EffectPriorityStructureRelated);
    if (structureEffects.first != structureEffects.second && !state.global.suspended) // If structure effects exist and state isn't suspended
    {
        for (auto& iter = structureEffects.first; iter != structureEffects.second; ++iter)
        {
            auto effectCode = iter->second.effect;
            auto effectValue = iter->second.value;

            // If a PosJump / PatBreak command was found and it's not in a section skipped by another PosJump / PatBreak:
            if (effectCode == dmf::EffectCode::PosJump || (effectCode == dmf::EffectCode::PatBreak && effectValue == 0))
            {
                // Convert MOD destination value to DMF destination value:
                const int dest = effectCode == dmf::EffectCode::PosJump ? effectValue - (int)m_UsingSetupPattern : state.global.order + 1;

                if (dest < 0 || dest >= dmf.GetModuleInfo().totalRowsInPatternMatrix)
                    throw std::invalid_argument("Invalid Position Jump or Pattern Break effect");
                
                if (dest >= (int)state.global.order) // If not a loop
                {
                    state.Save(dest);
                }
            }
        }
    }

    // Update duty cycle / wavetable state

    auto sampleChangeEffects = modEffects.equal_range(EffectPriority::EffectPrioritySampleChange);
    if (sampleChangeEffects.first != sampleChangeEffects.second)
    {
        for (auto& iter = sampleChangeEffects.first; iter != sampleChangeEffects.second; ++iter)
        {
            auto effectCode = iter->second.effect;
            auto effectValue = iter->second.value;

            if (effectCode == EffectCode::DutyCycleChange)
            {
                // Set Duty Cycle effect. Must be in square wave channel:
                if (state.global.channel == dmf::GameBoyChannel::SQW1 || state.global.channel == dmf::GameBoyChannel::SQW2)
                {
                    if (effectValue >= 0 && effectValue < 4 && chanState.dutyCycle != effectValue)
                    {
                        // Update state
                        chanState.dutyCycle = effectValue;
                        chanState.sampleChanged = true;
                    }
                }
            }
            else if (effectCode == EffectCode::WavetableChange)
            {
                // Set Wave effect. Must be in wave channel:
                if (state.global.channel == dmf::GameBoyChannel::WAVE)
                {
                    if (effectValue >= 0 && effectValue < dmf.GetTotalWavetables() && chanState.wavetable != effectValue)
                    {
                        // Update state
                        chanState.wavetable = effectValue;
                        chanState.sampleChanged = true;
                    }
                }
            }
        }
    }
}

static inline int16_t GetNewDMFVolume(int16_t dmfRowVol, const ChannelState& state)
{
    if (state.channel != dmf::GameBoyChannel::WAVE)
        return dmfRowVol == dmf::DMFNoVolume ? state.volume : dmfRowVol;

    switch (dmfRowVol)
    {
        case dmf::DMFNoVolume: // Volume isn't set for this channel row
            return state.volume; // channel's volume
        case 0: case 1: case 2: case 3:
            return 0;
        case 4: case 5: case 6: case 7:
            return 5;
        case 8: case 9: case 10: case 11:
            return 10;
        case 12: case 13: case 14: case 15:
            return 15;
        default:
            assert(false && "Invalid DMF volume");
            return -1;
    }
}

void MOD::DMFGetAdditionalEffects(const DMF& dmf, State& state, const Row<DMF>& row, PriorityEffectsMap& modEffects)
{
    ChannelState& chanState = state.channel[state.global.channel];
    
    // Determine what the volume should be for this channel
    const int16_t newChanVol = GetNewDMFVolume(row.volume, chanState);

    // If the volume or sample changed. If the sample changed, the MOD volume resets, so volume needs to be set again. TODO: Can optimize later.
    if (newChanVol != chanState.volume)
    {
        // The WAVE channel volume changes whether a note is attached or not, but SQ1/SQ2 need a note
        if (chanState.channel == dmf::GameBoyChannel::WAVE || NoteHasPitch(row.note))
        {
            uint8_t newVolume = (uint8_t)std::round(newChanVol / (double)dmf::DMFVolumeMax * (double)VolumeMax); // Convert DMF volume to MOD volume

            modEffects.insert({EffectPriorityVolumeChange, {EffectCode::SetVolume, newVolume}});

            chanState.volume = newChanVol; // Update the state
        }
    }

    // The sample change case is handled in DMFConvertNote.

    // TODO: Handle this outside of main loop in DMFConvertPatterns() for better performance
    // If we're at the very end of the song, in the 1st channel, and using the setup pattern
    if (state.global.patternRow + 1 == dmf.GetModuleInfo().totalRowsPerPattern
        && state.global.order + 1 == dmf.GetModuleInfo().totalRowsInPatternMatrix
        && state.global.channel == dmf::GameBoyChannel::SQW1)
    {
        // Check whether DMF pattern row has any Pos Jump effects that loop back to earlier in the song
        bool hasLoopback = false;
        unsigned loopbackToPattern = 0; // DMF pattern matrix row
        for (int chan = 0; chan <= (int)dmf::GameBoyChannel::NOISE; chan++)
        {
            const Row<DMF>& tempChanRow = dmf.GetData().GetRow(chan, state.global.order, state.global.patternRow);
            for (const auto& effect : tempChanRow.effect)
            {
                if (effect.code == dmf::EffectCode::PosJump && effect.value >= 0 && effect.value < (int)state.global.patternRow)
                {
                    hasLoopback = true;
                    loopbackToPattern = effect.value;
                    break;
                }
            }
            if (hasLoopback)
                break;
        }

        // Make sure this function isn't being called from DMFCreateSampleMapping (this is a cludgy solution, but all this code will be refactored later)
        if (!m_Patterns.empty())
        {
            // Add Note OFF to the loopback point for any channels where the sound would carry over
            for (int chan = 0; chan <= (int)dmf::GameBoyChannel::NOISE; chan++)
            {
                // If a note is playing on the last row before it loops, and the loopback point does not have a note playing:
                if (chanState.notePlaying && !NoteHasPitch(dmf.GetData().GetRow(chan, loopbackToPattern, 0).note))
                {
                    // TODO: A note could already be playing at the loopback point (a note which carried over from the previous pattern), but I am ignoring that case for now
                    // TODO: Would this mess up the MOD state in any way?
                    // Get 1st row of the pattern we're looping back to and modify the note to use Note OFF (NOTE: This assumes silent sample exists)
                    auto& modChanRow = GetChannelRow(loopbackToPattern + (m_UsingSetupPattern ? 1 : 0), 0, chan);
                    modChanRow.SampleNumber = 1; // Use silent sample
                    modChanRow.SamplePeriod = 0; // Don't need a note for the silent sample to work
                }
            }
        }

        if (!hasLoopback && m_UsingSetupPattern)
        {
            // Add loopback so that song doesn't restart on the setup pattern
            modEffects.insert({EffectPriorityStructureRelated, {EffectCode::PosJump, (uint16_t)1}});
        }
    }
}

/* 
void MOD::DMFUpdateStatePost(const DMF& dmf, MODState& state, const MOD::PriorityEffectsMap& modEffects)
{
    // Update volume-related state info?
    MODChannelState& chanState = state.channel[state.global.channel];

}
*/

Note MOD::DMFConvertNote(State& state, const Row<DMF>& row, const MOD::SampleMap& sampleMap, PriorityEffectsMap& modEffects, mod_sample_id_t& sampleId, uint16_t& period)
{
    Note modNote{NotePitch::C, 0};

    sampleId = 0;
    period = 0;

    // Noise channel is unsupported and should contain no notes
    if (state.global.channel == dmf::GameBoyChannel::NOISE)
        return modNote;

    ChannelState& chanState = state.channel[state.global.channel];

    // Convert note - Note cut effect // TODO: Move this to UpdateStatePre()?
    auto sampleChangeEffects = modEffects.equal_range(EffectPrioritySampleChange);
    if (sampleChangeEffects.first != sampleChangeEffects.second) // If sample change occurred (duty cycle, wave, or note cut effect)
    {
        for (auto& iter = sampleChangeEffects.first; iter != sampleChangeEffects.second; ++iter)
        {
            Effect& modEffect = iter->second;
            if (modEffect.effect == EffectCode::CutSample && modEffect.value == 0) // Note cut
            {
                sampleId = sampleMap.at(-1).GetFirstMODSampleId(); // Use silent sample
                period = 0; // Don't need a note for the silent sample to work
                chanState.notePlaying = false;
                chanState.currentNote = {};
                iter = modEffects.erase(iter); // Consume the effect
                return modNote; // NOTE: If this didn't return, would need to prevent iter from incrementing after erase
            }
        }
    }

    const NoteSlot& dmfNote = row.note;

    // Convert note - No note playing
    if (NoteIsEmpty(dmfNote)) // No note is playing. Only handle effects.
    {
        sampleId = 0; // Keeps previous sample id
        period = 0;
        return modNote;
    }
    
    // Convert note - Note OFF
    if (NoteIsOff(dmfNote)) // Note OFF. Use silent sample and handle effects.
    {
        sampleId = sampleMap.at(-1).GetFirstMODSampleId(); // Use silent sample
        period = 0; // Don't need a note for the silent sample to work
        chanState.notePlaying = false;
        chanState.currentNote = {};
        return modNote;
    }

    // Note is playing
    
    // If we are on the NOISE channel, dmfSampleId will go unused
    dmf_sample_id_t dmfSampleId = state.global.channel == dmf::GameBoyChannel::WAVE ? chanState.wavetable + 4 : chanState.dutyCycle;
    
    if (NoteHasPitch(dmfNote) && state.global.channel != dmf::GameBoyChannel::NOISE) // A note not on Noise channel
    {
        if (sampleMap.count(dmfSampleId) > 0)
        {
            const DMFSampleMapper& sampleMapper = sampleMap.at(dmfSampleId);
            
            DMFSampleMapper::NoteRange noteRange;
            modNote = sampleMapper.GetMODNote(GetNote(dmfNote), noteRange);

            if (chanState.noteRange != noteRange)
            {
                // Switching to a different note range requires the use of a different MOD sample.
                chanState.sampleChanged = true;
                chanState.noteRange = noteRange;
            }
        }
        else
        {
            throw std::runtime_error("In MOD::DMFConvertNote: A necessary DMF sample was not in the sample map.");
        }
    }

    period = proTrackerPeriodTable[modNote.octave][static_cast<uint16_t>(modNote.pitch)];

    if (chanState.sampleChanged || !chanState.notePlaying)
    {
        const DMFSampleMapper& samplerMapper = sampleMap.at(dmfSampleId);
        sampleId = samplerMapper.GetMODSampleId(chanState.noteRange);

        chanState.sampleChanged = false; // Just changed the sample, so resetting this for next time.
        
        // If a volume change effect isn't already present
        if (modEffects.find(EffectPriority::EffectPriorityVolumeChange) == modEffects.end())
        {
            // When you change ProTracker samples, the channel volume resets, so
            //  we need to check if a volume change effect is needed to get the volume back to where it was.

            if (chanState.volume != dmf::DMFVolumeMax) // Currently, the default volume for all samples is the maximum. TODO: Can optimize
            {
                uint8_t newVolume = (uint8_t)std::round(chanState.volume / (double)dmf::DMFVolumeMax * (double)VolumeMax); // Convert DMF volume to MOD volume

                modEffects.insert({EffectPriorityVolumeChange, {EffectCode::SetVolume, newVolume}});

                // TODO: If the default volume of the sample is selected in a smart way, we could potentially skip having to use a volume effect sometimes
            }
        }
    }
    else
    {
        sampleId = 0; // Keeps the previous sample number and prevents channel volume from being reset.
    }

    chanState.notePlaying = true;
    chanState.currentNote = row.note;
    return modNote;
}

ChannelRow MOD::DMFApplyNoteAndEffect(State& state, const PriorityEffectsMap& modEffects, mod_sample_id_t modSampleId, uint16_t period)
{
    ChannelRow modChannelRow;

    modChannelRow.SampleNumber = modSampleId;
    modChannelRow.SamplePeriod = period;
    modChannelRow.EffectCode = 0; // Will be set below
    modChannelRow.EffectValue = 0; // Will be set below

    Effect& channelIndependentEffect = state.global.channelIndependentEffect;

    // If no effects are being used here and a pat break or jump effect needs to be used, it can be done here
    if (modEffects.size() == 0)
    {
        if (channelIndependentEffect.effect != EffectCode::NoEffectCode)
        {
            // It's free real estate
            modChannelRow.EffectCode = channelIndependentEffect.effect;
            modChannelRow.EffectValue = channelIndependentEffect.value;

            channelIndependentEffect.effect = EffectCode::NoEffectCode;
            channelIndependentEffect.value = 0;
        }
    }
    else // There are effect(s) which need to be applied
    {
        bool effectUsed = false; // Whether an effect has been chosen to be used for this channel's effect slot
    
        // If a pat break or jump is used
        if (modEffects.find(EffectPriorityStructureRelated) != modEffects.end())
        {
            // Get iterator to effect(s) after the pat break or jump effects
            auto iter = modEffects.upper_bound(EffectPriorityStructureRelated);

            // If there are effects besides pat break or jump, and the noise channel effect slot is free
            if (iter != modEffects.end() && channelIndependentEffect.effect == EffectCode::NoEffectCode)
            {
                --iter; // To last pat break or jump effect

                assert(iter->first == EffectPriorityStructureRelated && "Must be pat break or jump effect");

                // Use the pat break / jump effect later
                channelIndependentEffect.effect = iter->second.effect;
                channelIndependentEffect.value = iter->second.value;
            }
            else // Use the pat break or jump effect now
            {
                --iter; // To last pat break or jump effect

                assert(iter->first == EffectPriorityStructureRelated && "Must be pat break or jump effect");

                modChannelRow.EffectCode = iter->second.effect;
                modChannelRow.EffectValue = iter->second.value;
                effectUsed = true;
            }
            ++iter; // Back to effect(s) after the pat break or jump effects
        }

        // Skip to effect(s) after sample change, since sample changes do not need MOD effect
        auto iter = modEffects.upper_bound(EffectPrioritySampleChange);
        const auto unsupportedEffects = modEffects.lower_bound(EffectPriorityUnsupportedEffect); 

        // If there are more effects that need to be used
        if (iter != modEffects.end() && iter != unsupportedEffects)
        {
            if (!effectUsed)
            {
                // MOD effect slot is available, so we'll use it
                modChannelRow.EffectCode = iter->second.effect;
                modChannelRow.EffectValue = iter->second.value;
                effectUsed = true;
                ++iter; // Go to next effect
            }
            else
            {
                // No more room for effects even though one is needed
                // ERROR: If max effects were desired, it cannot be done. At least one effect cannot be used.
            }
        }
        else
        {
            // No more effects
            if (!effectUsed && channelIndependentEffect.effect != EffectCode::NoEffectCode)
            {
                //assert(false && "This code should never be reached now that DMFConvertNote() erases sample change effects.");

                // The only MOD effect was a sample change, which isn't implemented as an effect, so...
                // It's free real estate
                modChannelRow.EffectCode = channelIndependentEffect.effect;
                modChannelRow.EffectValue = channelIndependentEffect.value;

                channelIndependentEffect.effect = EffectCode::NoEffectCode;
                channelIndependentEffect.value = 0;
            }
        }
    }

    return modChannelRow;
}

void MOD::DMFConvertInitialBPM(const DMF& dmf, unsigned& tempo, unsigned& speed)
{
    // Brute force function to get the Tempo/Speed pair which produces a BPM as close as possible to the desired BPM (if accuracy is desired),
    //      or a Tempo/Speed pair which is as close to the desired BPM without breaking the behavior of effects

    static constexpr double highestBPM = 3.0 * 255.0 / 1.0; // 3 * tempo * speed
    static constexpr double lowestBPM = 3.0 * 32.0 / 31.0;  // 3 * tempo * speed

    const double desiredBPM = dmf.GetBPM();

    if (GetOptions()->GetTempoType() == MODConversionOptions::TempoType::EffectCompatibility)
    {
        tempo = desiredBPM * 2;
        speed = 6;

        if (tempo > 255)
        {
            tempo = 255;
            m_Status.AddWarning(GetWarningMessage(ConvertWarning::TempoHighCompat));
        }
        else if (tempo < 32)
        {
            tempo = 32;
            m_Status.AddWarning(GetWarningMessage(ConvertWarning::TempoLowCompat));
        }
        else if ((desiredBPM * 2.0) - static_cast<double>(tempo) > 1e-3)
        {
            m_Status.AddWarning(GetWarningMessage(ConvertWarning::TempoAccuracy));
        }
        return;
    }

    if (desiredBPM > highestBPM)
    {
        tempo = 255;
        speed = 1;
        m_Status.AddWarning(GetWarningMessage(ConvertWarning::TempoHigh));
        return;
    }

    if (desiredBPM < lowestBPM)
    {
        tempo = 32;
        speed = 31;
        m_Status.AddWarning(GetWarningMessage(ConvertWarning::TempoLow));
        return;
    }

    tempo = 0;
    speed = 0;
    double bestBPMDiff = 9999999.0;

    for (unsigned d = 1; d <= 31; d++)
    {
        if (3 * 32.0 / d > desiredBPM || desiredBPM > 3 * 255.0 / d)
            continue; // Not even possible with this speed value
        
        for (unsigned n = 32; n <= 255; n++)
        {
            const double bpm = 3.0 * (double)n / d;
            const double thisBPMDiff = std::abs(desiredBPM - bpm);
            if (thisBPMDiff < bestBPMDiff
                || (thisBPMDiff == bestBPMDiff && d <= 6)) // Choose speed values more compatible with effects w/o sacrificing accuracy
            {
                tempo = n;
                speed = d;
                bestBPMDiff = thisBPMDiff;
            }
        }
    }

    if (bestBPMDiff > 1e-3)
        m_Status.AddWarning(GetWarningMessage(ConvertWarning::TempoAccuracy));
}

///// DMF --> MOD Sample Mapper

DMFSampleMapper::DMFSampleMapper()
{
    m_DmfId = -1;
    m_ModIds[0] = 0;
    m_ModIds[1] = 0;
    m_ModIds[2] = 0;
    m_ModSampleLengths[0] = 0;
    m_ModSampleLengths[1] = 0;
    m_ModSampleLengths[2] = 0;
    m_RangeStart.clear();
    m_NumMODSamples = 0;
    m_SampleType = SampleType::Silence;
    m_DownsamplingNeeded = false;
    m_ModOctaveShift = 0;
}

mod_sample_id_t DMFSampleMapper::Init(dmf_sample_id_t dmfSampleId, mod_sample_id_t startingId, const std::pair<Note, Note>& dmfNoteRange)
{
    // Determines how to split up a DMF sample into MOD sample(s). Returns the next free MOD sample id.

    // If it's a silent sample, use the special Init method for that:
    if (dmfSampleId == -1)
        return InitSilence();

    // Else, it's a Square or WAVE sample
    m_SampleType = dmfSampleId < 4 ? SampleType::Square : SampleType::Wave;
    m_DmfId = dmfSampleId;

    const Note& lowestNote = dmfNoteRange.first;
    const Note& highestNote = dmfNoteRange.second;

    // Note ranges always start on a C, so get nearest C note (in downwards direction):
    const Note lowestNoteNearestC = Note{NotePitch::C, (uint16_t)lowestNote.octave}; // DMF note

    // Get the number of MOD samples needed
    const int range = GetNoteRange(lowestNoteNearestC, highestNote);
    if (range <= 36) // Only one MOD note range needed
        m_NumMODSamples = 1;
    else if (range <= 72)
        m_NumMODSamples = 2;
    else
        m_NumMODSamples = 3;

    m_RangeStart.clear();

    // Initializing for 3 MOD samples is always the same:
    if (m_NumMODSamples == 3)
    {
        m_RangeStart.push_back({NotePitch::C, (uint16_t)0});
        m_RangeStart.push_back({NotePitch::C, (uint16_t)2});
        m_RangeStart.push_back({NotePitch::C, (uint16_t)5});
        m_ModSampleLengths[0] = 256;
        m_ModSampleLengths[1] = 64;
        m_ModSampleLengths[2] = 8;

        // For whatever reason, wave samples need to be transposed down one octave to match their sound in Deflemask
        if (m_SampleType == SampleType::Wave)
        {
            m_ModSampleLengths[0] *= 2;
            m_ModSampleLengths[1] *= 2;
            m_ModSampleLengths[2] *= 2;
        }

        m_DownsamplingNeeded = m_SampleType == SampleType::Wave; // Only wavetables are downsampled
        m_ModOctaveShift = 0;
        m_ModIds[0] = startingId;
        m_ModIds[1] = startingId + 1;
        m_ModIds[2] = startingId + 2;
        return startingId + 3;
    }

    // From here on, 1 or 2 MOD samples are needed

    // If we can, shift RangeStart lower to possibly prevent the need for downsampling:
    Note lowestPossibleRangeStart = lowestNoteNearestC; // DMF note
    int possibleShiftAmount = 0;

    Note currentHighEnd = lowestNoteNearestC; // DMF note
    currentHighEnd.octave += 3;
    if (m_NumMODSamples == 2)
        currentHighEnd.octave += 3;

    assert(currentHighEnd > highestNote);

    const int highEndSlack = GetNoteRange(highestNote, currentHighEnd) - 1;
    if (highEndSlack > 24 && lowestNoteNearestC.octave >= 2) // 2 octaves of slack at upper end, plus room to shift at bottom
        possibleShiftAmount = 2; // Can shift MOD notes down 2 octaves
    else if (highEndSlack > 12 && lowestNoteNearestC.octave >= 1) // 1 octave of slack at upper end, plus room to shift at bottom
        possibleShiftAmount = 1; // Can shift MOD notes down 1 octave

    // Apply octave shift
    lowestPossibleRangeStart.octave -= possibleShiftAmount;
    m_ModOctaveShift = possibleShiftAmount;

    /*
    TODO: Whenever shifting is possible and m_NumMODSamples > 1, overlapping
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

    unsigned rangeStartOctaveToSampleLengthMap[6] = 
    {
        256,
        128,
        64,
        32,
        16,
        8
    };

    // Set Range Start and Sample Length for 1st MOD sample:
    m_RangeStart.push_back(lowestPossibleRangeStart);
    m_ModSampleLengths[0] = rangeStartOctaveToSampleLengthMap[m_RangeStart[0].octave];

    // For whatever reason, wave samples need to be transposed down one octave to match their sound in Deflemask
    if (m_SampleType == SampleType::Wave)
        m_ModSampleLengths[0] *= 2;

    m_DownsamplingNeeded = m_ModSampleLengths[0] < 32 && m_SampleType == SampleType::Wave;
    m_ModIds[0] = startingId;

    // Set Range Start and Sample Length for 2nd MOD sample (if it exists):
    if (m_NumMODSamples == 2)
    {
        m_RangeStart.push_back({NotePitch::C, uint16_t(lowestPossibleRangeStart.octave + 3)});
        m_ModSampleLengths[1] = rangeStartOctaveToSampleLengthMap[m_RangeStart[1].octave];

        // For whatever reason, wave samples need to be transposed down one octave to match their sound in Deflemask
        if (m_SampleType == SampleType::Wave)
            m_ModSampleLengths[1] *= 2;

        if (m_ModSampleLengths[1] < 32 && m_SampleType == SampleType::Wave)
            m_DownsamplingNeeded = true;
        
        m_ModIds[1] = startingId + 1;
        return startingId + 2; // Two MOD samples were needed
    }

    return startingId + 1; // Only 1 MOD sample was needed
}

mod_sample_id_t DMFSampleMapper::InitSilence()
{
    // Set up for a silent sample
    m_SampleType = SampleType::Silence;
    m_RangeStart.clear();
    m_NumMODSamples = 1;
    m_ModSampleLengths[0] = 8;
    m_DownsamplingNeeded = false;
    m_ModOctaveShift = 0;
    m_DmfId = -1;
    m_ModIds[0] = 1; // Silent sample is always MOD sample #1
    return 2; // The next available MOD sample id
}

Note DMFSampleMapper::GetMODNote(const Note& dmfNote, NoteRange& modNoteRange) const
{
    // Returns the MOD note to use given a DMF note. Also returns which
    //      MOD sample the MOD note needs to use. The MOD note's octave
    //      and pitch should always be exactly what gets displayed in ProTracker.

    Note modNote{NotePitch::C, (uint16_t)1};
    modNoteRange = NoteRange::First;

    if (m_SampleType == SampleType::Silence)
        return modNote;

    modNoteRange = GetMODNoteRange(dmfNote);
    const Note& rangeStart = m_RangeStart[static_cast<int>(modNoteRange)];

    modNote.pitch = dmfNote.pitch;
    modNote.octave = dmfNote.octave - rangeStart.octave + 1;
    // NOTE: The octave shift is already factored into rangeStart.
    //          The "+ 1" is because MOD's range starts at C-1 not C-0.

    assert(modNote.octave >= 1 && "Note octave is too low.");
    assert(modNote.octave <= 3 && "Note octave is too high.");

    return modNote;
}

DMFSampleMapper::NoteRange DMFSampleMapper::GetMODNoteRange(const Note& dmfNote) const
{
    // Returns which MOD sample in the collection (1st, 2nd, or 3rd) should be used for the given DMF note
    // Assumes dmfNote is a valid note for this MOD sample collection

    if (m_NumMODSamples == 1)
        return NoteRange::First; // The only option

    const uint16_t& octaveOfNearestC = dmfNote.octave;

    if (octaveOfNearestC < m_RangeStart[1].octave)
    {
        // It is the lowest MOD sample
        return NoteRange::First;
    }
    else if (m_NumMODSamples == 2)
    {
        // The only other option when there are just two choices is the 2nd one
        return NoteRange::Second;
    }
    else if (octaveOfNearestC < m_RangeStart[2].octave)
    {
        // Must be the middle of the 3 MOD samples
        return NoteRange::Second;
    }
    else
    {
        // Last option
        return NoteRange::Third;
    }
}

mod_sample_id_t DMFSampleMapper::GetMODSampleId(const Note& dmfNote) const
{
    // Returns the MOD sample id that would be used for the given DMF note
    // Assumes dmfNote is a valid note for this MOD sample collection
    const NoteRange noteRange = GetMODNoteRange(dmfNote);
    return GetMODSampleId(noteRange);
}

mod_sample_id_t DMFSampleMapper::GetMODSampleId(NoteRange modNoteRange) const
{
    // Returns the MOD sample id of the given MOD sample in the collection (1st, 2nd, or 3rd)
    const int modNoteRangeInt = static_cast<int>(modNoteRange);
    if (modNoteRangeInt + 1 > m_NumMODSamples)
        throw std::range_error("In SampleMapper::GetMODSampleId: The provided MOD note range is invalid for this SampleMapper object.");

    return m_ModIds[modNoteRangeInt];
}

unsigned DMFSampleMapper::GetMODSampleLength(NoteRange modNoteRange) const
{
    // Returns the sample length of the given MOD sample in the collection (1st, 2nd, or 3rd)
    const int modNoteRangeInt = static_cast<int>(modNoteRange);
    if (modNoteRangeInt + 1 > m_NumMODSamples)
        throw std::range_error("In SampleMapper::GetMODSampleLength: The provided MOD note range is invalid for this SampleMapper object.");

    return m_ModSampleLengths[modNoteRangeInt];
}

DMFSampleMapper::NoteRange DMFSampleMapper::GetMODNoteRange(mod_sample_id_t modSampleId) const
{
    // Returns note range (1st, 2nd, or 3rd MOD sample in the collection) given the MOD sample id
    switch (modSampleId - m_ModIds[0])
    {
        case 0:
            return NoteRange::First;
        case 1:
            return NoteRange::Second;
        case 2:
            return NoteRange::Third;
        default:
            throw std::range_error("In SampleMapper::GetMODNoteRange: The provided MOD sample id was invalid for this SampleMapper object.");
    }
}

DMFSampleMapper::NoteRangeName DMFSampleMapper::GetMODNoteRangeName(NoteRange modNoteRange) const
{
    // Gets NoteRange which can be used for printing purposes
    switch (modNoteRange)
    {
        case NoteRange::First:
            if (m_NumMODSamples == 1)
                return NoteRangeName::None;
            return NoteRangeName::Low;
        case NoteRange::Second:
            if (m_NumMODSamples == 2)
                return NoteRangeName::High;
            return NoteRangeName::Middle;
        case NoteRange::Third:
            return NoteRangeName::High;
        default:
            throw std::range_error("In SampleMapper::GetMODNoteRangeName: The provided MOD note range is invalid for this SampleMapper object.");
    }
}


///////// EXPORT /////////

void MOD::ExportRaw(const std::string& filename)
{
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile.is_open())
    {
        throw MODException(ModuleException::Category::Export, ModuleException::ExportError::FileOpen);
    }

    ExportModuleName(outFile);
    ExportSampleInfo(outFile);
    ExportModuleInfo(outFile);
    ExportPatterns(outFile);
    ExportSampleData(outFile);

    outFile.close();

    const bool verbose = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Verbose).GetValue<bool>();

    if (verbose)
        std::cout << "Saved MOD file to disk.\n\n";
}

void MOD::ExportModuleName(std::ofstream& fout) const
{
    // Print module name, truncating or padding with zeros as needed
    for (unsigned i = 0; i < 20; i++)
    {
        if (i < m_ModuleName.size())
        {
            fout.put(m_ModuleName[i]);
        }
        else
        {
            fout.put(0);
        }
    }
}

void MOD::ExportSampleInfo(std::ofstream& fout) const
{
    for (const auto& mapPair : m_Samples)
    {
        const auto& sample = mapPair.second;
        
        if (sample.name.size() > 22)
            throw std::length_error("Sample name must be 22 characters or less");

        // Pad name with zeros
        std::string nameCopy = sample.name;
        while (nameCopy.size() < 22)
            nameCopy += " ";

        fout << nameCopy;

        fout.put(sample.length >> 9);       // Length byte 0
        fout.put(sample.length >> 1);       // Length byte 1
        fout.put(sample.finetune);          // Finetune value !!!
        fout.put(sample.volume);            // Sample volume // TODO: Optimize this?
        fout.put(sample.repeatOffset >> 9); // Repeat offset byte 0
        fout.put(sample.repeatOffset >> 1); // Repeat offset byte 1
        fout.put(sample.repeatLength >> 9); // Sample repeat length byte 0
        fout.put(sample.repeatLength >> 1); // Sample repeat length byte 1
    }
    
    // The remaining samples are blank:
    for (int i = m_TotalMODSamples; i < 31; i++)
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
    fout.put(m_NumberOfRowsInPatternMatrix);   // Song length in patterns (not total number of patterns) 
    fout.put(127);                        // 0x7F - Useless byte that has to be here

    // Pattern matrix (Each ProTracker pattern number is the same as its pattern matrix row number)
    for (uint8_t i = 0; i < m_NumberOfRowsInPatternMatrix; i++)
    {
        fout.put(i);
    }
    for (uint8_t i = m_NumberOfRowsInPatternMatrix; i < 128; i++)
    {
        fout.put(0);
    }
    
    fout << "M.K."; // ProTracker uses "M!K!" if there's more than 64 pattern matrix rows
}

void MOD::ExportPatterns(std::ofstream& fout) const
{
    for (const auto& pattern : m_Patterns)
    {
        for (const auto& channelRow : pattern)
        {
            // Sample number (upper 4b); sample period/effect param. (upper 4b)
            fout.put((channelRow.SampleNumber & 0xF0) | ((channelRow.SamplePeriod & 0x0F00) >> 8));

            // Sample period/effect param. (lower 8 bits)
            fout.put(channelRow.SamplePeriod & 0x00FF);
        
            //const uint16_t effect = ((uint16_t)channelRow.EffectCode << 4) | channelRow.EffectValue;

            // Sample number (lower 4b); effect code (upper 4b)
            fout.put((channelRow.SampleNumber << 4) | (channelRow.EffectCode >> 4));
            
            // Effect code (lower 8 bits)
            fout.put(((channelRow.EffectCode << 4) & 0x00FF) | channelRow.EffectValue);
        }
    }
}

void MOD::ExportSampleData(std::ofstream& fout) const
{
    for (const auto& sampleInfo : m_Samples)
    {
        const auto& sampleData = sampleInfo.second.data;
        for (int8_t value : sampleData)
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
        case MOD::ConvertWarning::PitchHigh:
            return "Cannot use the highest Deflemask note (C-8) on some MOD players including ProTracker.";
        case MOD::ConvertWarning::TempoLow:
            return std::string("Tempo is too low. Using ~3.1 BPM instead.\n")
                    + std::string("         ProTracker only supports tempos between ~3.1 and 765 BPM.");
        case MOD::ConvertWarning::TempoHigh:
            return std::string("Tempo is too high for ProTracker. Using 127.5 BPM instead.\n")
                    + std::string("         ProTracker only supports tempos between ~3.1 and 765 BPM.");
        case MOD::ConvertWarning::TempoLowCompat:
            return std::string("Tempo is too low. Using 16 BPM to retain effect compatibility.\n")
                    + std::string("         Use --tempo=accuracy for the full tempo range.");
        case MOD::ConvertWarning::TempoHighCompat:
            return std::string("Tempo is too high. Using 127.5 BPM to retain effect compatibility.\n")
                    + std::string("         Use --tempo=accuracy for the full tempo range.");
        case MOD::ConvertWarning::TempoAccuracy:
            return "Tempo does not exactly match, but a value close to it is being used.";
        case MOD::ConvertWarning::EffectIgnored:
            return "A Deflemask effect was ignored due to limitations of the MOD format.";
        case MOD::ConvertWarning::WaveDownsample:
            return "Wavetable instrument #" + info + " was downsampled in MOD to allow higher notes to be played.";
        case MOD::ConvertWarning::MultipleEffects:
            return "No more than one volume change or effect can appear in the same row of the same channel. Important effects will be prioritized.";
        default:
            return "";
    }
}

std::string MODException::CreateErrorMessage(Category category, int errorCode, const std::string& arg)
{
    switch (category)
    {
        case Category::None:
            return "No error.";
        case Category::Import:
            return "No error.";
        case Category::Export:
            return "No error.";
        case Category::Convert:
            switch (errorCode)
            {
                case (int)MOD::ConvertError::Success:
                    return "No error.";
                case (int)MOD::ConvertError::NotGameBoy:
                    return "Only the Game Boy system is currently supported.";
                case (int)MOD::ConvertError::TooManyPatternMatrixRows:
                    return "Too many rows of patterns in the pattern matrix. 64 is the maximum. (63 if using Setup Pattern.)";
                case (int)MOD::ConvertError::Over64RowPattern:
                    return std::string("Patterns must have 64 or fewer rows.\n")
                            + std::string("       A workaround for this issue is planned for a future update to dmf2mod.");
                default:
                    return "";
            }
            break;
    }
    return "";
}
