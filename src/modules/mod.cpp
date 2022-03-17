/*
    mod.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Implements a ModuleInterface-derived class for ProTracker's 
    MOD files.

    Several limitations apply in order to export. For example, 
    for DMF --> MOD, the DMF file must use the Game Boy system, 
    patterns must have 64 or fewer rows, only one effect column is
    allowed per channel, etc.
*/

// TODO: Add '--effects=none' option, which does not use any ProTracker effects. 

// TODO: Delete output file if an error occurred while creating it?

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
    /* Option id               / Full name     / Short / Default / Possib. vals / Description */
    {MODOptionEnum::Downsample, "downsample",   '\0',     false,                  "Allow wavetables to lose information through downsampling if needed.", true},
    {MODOptionEnum::Effects,    "effects",      '\0',     "max",  {"min", "max"}, "The number of ProTracker effects to use.", true}
});

// Register module info
MODULE_DEFINE(MOD, MODConversionOptions, ModuleType::MOD, "mod", MODOptions)

#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

static std::vector<int8_t> GenerateSquareWaveSample(unsigned dutyCycle, unsigned length);
static std::vector<int8_t> GenerateWavetableSample(uint32_t* wavetableData, unsigned length);
static int16_t GetNewDMFVolume(int16_t dmfRowVol, const ChannelState& state);

static std::string GetWarningMessage(MOD::ConvertWarning warning);

static bool GetTempoAndSpeedFromBPM(double desiredBPM, unsigned& tempo, unsigned& speed);
static unsigned GCD(unsigned u, unsigned v);

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

MODConversionOptions::MODConversionOptions() {}

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
            throw MODException(ModuleException::Category::Convert, ModuleException::ConvertError::UnsupportedInputType, input->GetFileExtension());
    }
}

///////// CONVERT FROM DMF /////////

void MOD::ConvertFromDMF(const DMF& dmf)
{
    const bool silent = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Silence).GetValue<bool>();

    if (!silent)
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

    if (!silent)
        std::cout << "Converting samples...\n";

    SampleMap sampleMap;
    DMFConvertSamples(dmf, sampleMap);

    ///////////////// CONVERT PATTERN DATA

    if (!silent)
        std::cout << "Converting pattern data...\n";

    DMFConvertPatterns(dmf, sampleMap);
    
    ///////////////// CLEAN UP

    if (!silent)
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
    State state;
    state.Init();

    // The main MODChannelState structs should NOT update during patterns or parts of 
    //   patterns that the Position Jump (Bxx) effect skips over. (Ignore loops)
    //   Keep a copy of the main state for each channel, and once it reaches the 
    //   jump destination, overwrite the current state with the copied state.

    const dmf::ModuleInfo& moduleInfo = dmf.GetModuleInfo();
    
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

                dmf::ChannelRow chanRow = dmf.GetChannelRow(chan, patMatRow, patRow);

                // If just arrived at jump destination:
                if (patMatRow == state.global.jumpDestination && patRow == 0 && state.global.suspended)
                {
                    // Restore state copies
                    state.Restore();
                }

                PriorityEffectsMap modEffects;
                //mod_sample_id_t modSampleId = 0;
                //uint16_t period = 0;

                if (chan == dmf::GameBoyChannel::NOISE)
                {
                    modEffects = DMFConvertEffects_NoiseChannel(chanRow);
                    DMFUpdateStatePre(dmf, state, modEffects);
                    continue;
                }

                modEffects = DMFConvertEffects(chanRow);
                DMFUpdateStatePre(dmf, state, modEffects);
                DMFGetAdditionalEffects(dmf, state, chanRow, modEffects);

                //DMFConvertNote(state, chanRow, sampleMap, modEffects, modSampleId, period);

                // TODO: More state-related stuff could be extracted from DMFConvertNote and put into separate 
                //  method so that I don't have to copy code from it to put here.
                
                // Convert note - Note cut effect
                auto sampleChangeEffects = modEffects.equal_range(EffectPrioritySampleChange);
                if (sampleChangeEffects.first != sampleChangeEffects.second) // If sample change occurred (duty cycle, wave, or note cut effect)
                {
                    for (auto& iter = sampleChangeEffects.first; iter != sampleChangeEffects.second; ++iter)
                    {
                        Effect& modEffect = iter->second;
                        if (modEffect.effect == EffectCode::CutSample) // Note cut
                        {
                            // Silent sample is needed
                            if (sampleMap.count(-1) == 0)
                                sampleMap[-1] = {};

                            chanState.notePlaying = false;
                            modEffects.erase(iter); // Consume the effect
                        }
                    }   
                }

                // Convert note - Note OFF
                if (chanRow.note.IsOff()) // Note OFF. Use silent sample and handle effects.
                {
                    // Silent sample is needed
                    if (sampleMap.count(-1) == 0)
                        sampleMap[-1] = {};
                    
                    chanState.notePlaying = false;
                }

                // A note on the SQ1, SQ2, or WAVE channels:
                if (chanRow.note.HasPitch() && chan != dmf::GameBoyChannel::NOISE)
                {
                    chanState.notePlaying = true;

                    const dmf::Note& dmfNote = chanRow.note;

                    mod_sample_id_t sampleId = chan == dmf::GameBoyChannel::WAVE ? chanState.wavetable + 4 : chanState.dutyCycle;

                    // Mark this square wave or wavetable as used
                    sampleMap[sampleId] = {};

                    // Get lowest/highest notes
                    if (sampleIdLowestHighestNotesMap.count(sampleId) == 0) // 1st time
                    {
                        sampleIdLowestHighestNotesMap[sampleId] = std::pair<dmf::Note, dmf::Note>(dmfNote, dmfNote);
                    }
                    else
                    {
                        auto& notePair = sampleIdLowestHighestNotesMap[sampleId];
                        if (dmfNote > notePair.second)
                        {
                            // Found a new highest note
                            notePair.second = chanRow.note;
                        }
                        if (dmfNote < notePair.first)
                        {
                            // Found a new lowest note
                            notePair.first = chanRow.note;
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
    for (auto& mapPair : sampleMap)
    {
        dmf_sample_id_t sampleId = mapPair.first;
        DMFSampleMapper& sampleMapper = mapPair.second;

        // Special handling of silent sample
        if (sampleId == -1)
        {
            // Silent sample is always sample #1 if it is used
            currentMODSampleId = sampleMapper.InitSilence();
            continue;
        }

        const auto& lowHighNotes = sampleIdLowestHighestNotesMap.at(sampleId);
        currentMODSampleId = sampleMapper.Init(sampleId, currentMODSampleId, lowHighNotes);

        if (sampleMapper.IsDownsamplingNeeded() && !GetOptions()->GetDownsample())
        {
            throw MODException(ModuleException::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
        }
    }

    m_TotalMODSamples = currentMODSampleId - 1; // Set the number of MOD samples that will be needed. (minus sample #0 which is special)
    
    // TODO: Check if there are too many samples needed here, and throw exception if so
}

void MOD::DMFConvertSampleData(const DMF& dmf, const SampleMap& sampleMap)
{
    // Fill out information needed to define a MOD sample
    m_Samples.clear();
    
    for (const auto& mapPair : sampleMap)
    {
        dmf_sample_id_t dmfSampleId = mapPair.first;
        const DMFSampleMapper& sampleMapper = mapPair.second;

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
        ChannelRow speedRow;
        speedRow.SampleNumber = 0;
        speedRow.SamplePeriod = 0;
        speedRow.EffectCode = EffectCode::SetSpeed;
        speedRow.EffectValue = initialSpeed;
        SetChannelRow(0, 0, 1, speedRow);

        // Set Pattern Break to start of song
        ChannelRow posJumpRow;
        posJumpRow.SampleNumber = 0;
        posJumpRow.SamplePeriod = 0;
        posJumpRow.EffectCode = EffectCode::PatBreak;
        posJumpRow.EffectValue = 0;
        SetChannelRow(0, 0, 2, posJumpRow);
        
        // All other channel rows in the pattern are already zeroed out so nothing needs to be done for them
    }

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.

    State state;
    state.Init();

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

                dmf::ChannelRow chanRow = dmf.GetChannelRow(chan, patMatRow, patRow);
                
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
                    modEffects = DMFConvertEffects(chanRow);
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

MOD::PriorityEffectsMap MOD::DMFConvertEffects(const dmf::ChannelRow& pat)
{
    /*
     * Higher in list = higher priority effect:
     * - Important song structure related effect such as Pattern break or Jump (Can always be performed)
     * - Sample change (Can always be performed; Doesn't need MOD effect)
     * - Tempo change (Not always usable)
     * - Volume change (Not always usable)
     * - Other effects
     * - Unsupported effect
     */

    using EffectPair = std::pair<EffectPriority, Effect>;
    MOD::PriorityEffectsMap modEffects;

    // Convert DMF effects to MOD effects
    for (const auto& dmfEffect : pat.effect)
    {
        EffectPair effectPair {EffectPriorityUnsupportedEffect, {EffectCode::NoEffectCode, EffectCode::NoEffectVal}};

        if (dmfEffect.code == dmf::EffectCode::NoEffect)
            continue;

        switch (dmf::EffectCode(dmfEffect.code))
        {
        case dmf::EffectCode::NoteCut:
            if (dmfEffect.value == 0)
            {
                effectPair.first = EffectPrioritySampleChange; // Can be implemented as silent sample
                effectPair.second = {EffectCode::CutSample, 0};
            }
            break;
        case dmf::EffectCode::PatBreak:
            if (dmfEffect.value == 0)
            {
                effectPair.first = EffectPriorityStructureRelated; // Only D00 is supported at the moment
                effectPair.second = {EffectCode::PatBreak, 0};
            }
            break;
        case dmf::EffectCode::PosJump:
        {
            effectPair.first = EffectPriorityStructureRelated;
            const int dest = dmfEffect.value + (int)m_UsingSetupPattern; // Into MOD order value, not DMF
            effectPair.second = {EffectCode::PosJump, (uint16_t)dest};
            break;
        }
        
        default:
            break; // Unsupported. priority remains UnsupportedEffect
        }

        switch (dmf::GameBoyEffectCode(dmfEffect.code))
        {
        case dmf::GameBoyEffectCode::SetDutyCycle:
            effectPair.first = EffectPrioritySampleChange;
            effectPair.second = {EffectCode::DutyCycleChange, (uint16_t)dmfEffect.value};
            break;
        case dmf::GameBoyEffectCode::SetWave:
            effectPair.first = EffectPrioritySampleChange;
            effectPair.second = {EffectCode::WavetableChange, (uint16_t)(dmfEffect.value)};
            break;
        default:
            break; // Unsupported. priority remains UnsupportedEffect
        }

        modEffects.insert(effectPair);
    }

    return modEffects;
}

MOD::PriorityEffectsMap MOD::DMFConvertEffects_NoiseChannel(const dmf::ChannelRow& pat)
{
    // Temporary method until the Noise channel is supported.
    // Only Pattern Break and Jump effects on the noise channel are converted for now.

    PriorityEffectsMap modEffects;

    for (auto& dmfEffect : pat.effect)
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

void MOD::DMFUpdateStatePre(const DMF& dmf, State& state, const MOD::PriorityEffectsMap& modEffects)
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

void MOD::DMFGetAdditionalEffects(const DMF& dmf, State& state, const dmf::ChannelRow& pat, PriorityEffectsMap& modEffects)
{
    ChannelState& chanState = state.channel[state.global.channel];
    
    // Determine what the volume should be for this channel
    const int16_t newChanVol = GetNewDMFVolume(pat.volume, chanState);

    // If the volume or sample changed. If the sample changed, the MOD volume resets, so volume needs to be set again. TODO: Can optimize later.
    if (newChanVol != chanState.volume)
    {
        // The WAVE channel volume changes whether a note is attached or not, but SQ1/SQ2 need a note
        if (chanState.channel == dmf::GameBoyChannel::WAVE || pat.note.HasPitch())
        {
            uint8_t newVolume = (uint8_t)std::round(newChanVol / (double)dmf::DMFVolumeMax * (double)VolumeMax); // Convert DMF volume to MOD volume

            modEffects.insert({EffectPriorityVolumeChange, {EffectCode::SetVolume, newVolume}});

            chanState.volume = newChanVol; // Update the state
        }
    }
    
    // The sample change case is handled in DMFConvertNote.

    // If we're at the very end of the song, in the 1st channel, and using the setup pattern
    if (state.global.patternRow + 1 == dmf.GetModuleInfo().totalRowsPerPattern
        && state.global.order + 1 == dmf.GetModuleInfo().totalRowsInPatternMatrix
        && state.global.channel == dmf::GameBoyChannel::SQW1
        && m_UsingSetupPattern)
    {
        // Check whether DMF pattern row has any Pos Jump effects that loop back to earlier in the song
        bool hasLoopback = false;
        for (int chan = 0; chan < (int)dmf::GameBoyChannel::WAVE; chan++)
        {
            dmf::ChannelRow tempChanRow = dmf.GetChannelRow(chan, state.global.order, state.global.patternRow);
            for (const auto& effect : tempChanRow.effect)
            {
                if (effect.code == dmf::EffectCode::PosJump && effect.value < (int)state.global.patternRow)
                {
                    hasLoopback = true;
                    break;
                }
            }
            if (hasLoopback)
                break;
        }

        if (!hasLoopback)
        {
            // Add loopback so that song doesn't restart on the setup pattern
            modEffects.insert({EffectPriorityStructureRelated, {EffectCode::PosJump, (uint16_t)1}});
        }
    }
}

/* 
void MOD::UpdateStatePost(const DMF& dmf, MODState& state, const MOD::PriorityEffectsMap& modEffects)
{
    // Update volume-related state info?
    MODChannelState& chanState = state.channel[state.global.channel];

}
*/

Note MOD::DMFConvertNote(State& state, const dmf::ChannelRow& pat, const MOD::SampleMap& sampleMap, MOD::PriorityEffectsMap& modEffects, mod_sample_id_t& sampleId, uint16_t& period)
{
    Note modNote(0, 0);

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
            if (modEffect.effect == EffectCode::CutSample) // Note cut
            {
                sampleId = sampleMap.at(-1).GetFirstMODSampleId(); // Use silent sample
                period = 0; // Don't need a note for the silent sample to work
                chanState.notePlaying = false;
                modEffects.erase(iter); // Consume the effect
                return modNote;
            }
        }
    }

    const dmf::Note& dmfNote = pat.note;

    // Convert note - No note playing
    if (dmfNote.IsEmpty()) // No note is playing. Only handle effects.
    {
        sampleId = 0; // Keeps previous sample id
        period = 0;
        return modNote;
    }
    
    // Convert note - Note OFF
    if (dmfNote.IsOff()) // Note OFF. Use silent sample and handle effects.
    {
        sampleId = sampleMap.at(-1).GetFirstMODSampleId(); // Use silent sample
        period = 0; // Don't need a note for the silent sample to work
        chanState.notePlaying = false;
        return modNote;
    }

    // Note is playing
    
    // If we are on the NOISE channel, dmfSampleId will go unused
    dmf_sample_id_t dmfSampleId = state.global.channel == dmf::GameBoyChannel::WAVE ? chanState.wavetable + 4 : chanState.dutyCycle;
    
    if (dmfNote.HasPitch() && state.global.channel != dmf::GameBoyChannel::NOISE) // A note not on Noise channel
    {
        if (sampleMap.count(dmfSampleId) > 0)
        {
            const DMFSampleMapper& sampleMapper = sampleMap.at(dmfSampleId);
            
            DMFSampleMapper::NoteRange noteRange;
            modNote = sampleMapper.GetMODNote(dmfNote, noteRange);

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

    if (dmfNote.pitch == dmf::NotePitch::C_Alt)
    {
        assert(false && "In MOD::DMFConvertNote: DMF note pitch is 12 (C) - This should never happen since we're converting these pitches of 12 to 0 when importing DMF");
        //modOctave++;
    }

    period = proTrackerPeriodTable[modNote.octave][modNote.pitch];

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
    return modNote;
}

ChannelRow MOD::DMFApplyNoteAndEffect(State& state, const MOD::PriorityEffectsMap& modEffects, mod_sample_id_t modSampleId, uint16_t period)
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
                ++iter;
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
    // Gets MOD tempo and speed values needed to set initial BPM to the DMF's BPM, or as close as possible.

    tempo = 0;
    speed = 0;

    unsigned bpmNumerator, bpmDenominator;
    dmf.GetBPM(bpmNumerator, bpmDenominator);

    unsigned n = bpmNumerator / 3; // Should always divide cleanly for DMF tempos. See DMF::GetBPM(...)
    unsigned d = bpmDenominator;

    // Simplify the fraction
    unsigned div;
    do
    {
        div = GCD(n, d);
        n /= div;
        d /= div;
    } while (div != 1 && n != 0 && d != 0);

    enum NUMDENSTATUS {OK=0, NUM_LOW=1, NUM_HIGH=2, DEN_LOW=4, DEN_HIGH=8};
    unsigned status = OK;

    if (n < 33)
        status |= NUM_LOW;
    if (n > 255)
        status |= NUM_HIGH;
    // Denominator cannot be too low (0 is lowest value)
    if (d > 32)
        status |= DEN_HIGH;

    // Adjust the numerator and denominator to get valid tempo and speed values which
    //  will produce a BPM in MOD as close to the BPM in Deflemask as possible.
    switch (status)
    {
        case OK:
        {
            // OK! BPM can exactly match the BPM in Deflemask.
            tempo = n;
            speed = d;
            break;
        }

        case NUM_LOW:
        {
            unsigned multiplier = 255 / n; // Integer division.
            
            // n * multiplier will be <= 255; If the multiplier lowers, n will still be good.
            // So now we must multiply d by multiplier and check that it will work too:
            while (multiplier > 1)
            {
                if (d * multiplier <= 32)
                    break;
                multiplier--;
            }

            if (multiplier == 1)
            {
                // Numerator is too low but cannot raise it without making denominator too high.
                // Set MOD BPM to lowest value possible, but even that will not be enough to match DMF BPM:
                tempo = 33;
                speed = 32;
                
                m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::TempoLow));
                break;
            }
            
            // Adjustment was possible!
            tempo = n * multiplier;
            speed = d * multiplier;
            break;
        }

        case NUM_HIGH:
        {
            // Matching the DMF BPM exactly will be impossible.
            // n and d were already divided by their GCD.

            // This might be temporary until I get a better, faster solution:
            if (GetTempoAndSpeedFromBPM(bpmNumerator * 1.0 / bpmDenominator, tempo, speed))
            {
                // If it failed, use highest possible BPM:
                tempo = 255;
                speed = 1;
                m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::TempoHigh));
                break;
            }

            m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::TempoPrecision));
            break;
            
            /*
            // TODO: This case is very poor at making the BPM match up closely.
            //          Small changes to the denominator cause large changes to BPM.

            // Make n the highest valid value. d will still 
            // be valid after adjustment because d has no minimum value.
            double divisor = n / 255.0;
            
            tempo = 255;
            speed = d / divisor;

            // TODO: Set warning here
            break;
            */
        }

        case DEN_HIGH:
        {
            // Matching the DMF BPM exactly will be impossible.
            // n and d were already divided by their GCD.
            
            // Make d the highest valid value.
            double divisor = d / 32.0;
            
            unsigned newN = static_cast<unsigned>(n / divisor);
            if (newN < 33)
            {
                // n can't handle the adjustment.
                // Will have to use lowest possible BPM, which is closest approx. to DMF's BPM:
                tempo = 33;
                speed = 32;
                m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::TempoLow));
                break;
            }
            else
            {
                tempo = newN;
                speed = 32;
                m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::TempoPrecision));
                break;
            }
        }

        case (NUM_LOW | DEN_HIGH):
        {
            // Numerator is too low but cannot raise it without making denominator too high.
            // Set MOD BPM to lowest value possible, but even that will not be enough to 
            // match DMF BPM exactly:
            tempo = 33;
            speed = 32;
            
            m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::TempoLow));
            break;
        }

        case (NUM_HIGH | DEN_HIGH):
        {
            // DMF BPM is probably within the min and max values allowed by MOD, but it cannot 
            //  be converted to MOD BPM without losing precision.

            // Make d the highest valid value.
            double divisorD = d / 32.0;
            unsigned newN = static_cast<unsigned>(n / divisorD);

            // Make n the highest valid value.
            double divisorN = n / 255.0;
            unsigned newD = static_cast<unsigned>(d / divisorN);

            // Check which option loses the least precision?
            if (32 <= newN && newN <= 255)
            {
                tempo = newN;
                speed = 32;
            }
            else if (newD <= 32)
            {
                tempo = 255;
                speed = newD;
            }
            else
            {
                // If d is big enough compared to n, this condition is possible.
                // Specifically, when: n * 1.0 / d < 33.0 / 32.0
                // The d value cannot be made small enough without making the n value too small.
                // Will have to use lowest possible BPM, which is closest approx. to DMF's BPM:
                tempo = 33;
                speed = 32;
                m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::TempoLow));
                break;
            }
            // TODO: Is there also a possibility for the tempo to be too high?

            m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::TempoPrecision));
            break;
        }

        default:
            // Error!
            throw std::runtime_error("Unknown error while converting Deflemask tempo to MOD.\n");
            break;
    }

    if (speed == 0)
        speed = 1; // Speed 0 and 1 are identical in ProTracker, but I prefer using 1.
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

mod_sample_id_t DMFSampleMapper::Init(dmf_sample_id_t dmfSampleId, mod_sample_id_t startingId, const std::pair<dmf::Note, dmf::Note>& dmfNoteRange)
{
    // Determines how to split up a DMF sample into MOD sample(s). Returns the next free MOD sample id.

    // If it's a silent sample, use the special Init method for that:
    if (dmfSampleId == -1)
        return InitSilence();

    // Else, it's a Square or WAVE sample
    m_SampleType = dmfSampleId < 4 ? SampleType::Square : SampleType::Wave;
    m_DmfId = dmfSampleId;

    const dmf::Note& lowestNote = dmfNoteRange.first;
    const dmf::Note& highestNote = dmfNoteRange.second;

    // Note ranges always start on a C, so get nearest C note (in downwards direction):
    const dmf::Note lowestNoteNearestC = dmf::Note(dmf::NotePitch::C, lowestNote.octave);

    // Get the number of MOD samples needed
    const int range = dmf::GetNoteRange(lowestNoteNearestC, highestNote);
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
        m_RangeStart.emplace_back(dmf::NotePitch::C, 0);
        m_RangeStart.emplace_back(dmf::NotePitch::C, 2);
        m_RangeStart.emplace_back(dmf::NotePitch::C, 5);
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
    dmf::Note lowestPossibleRangeStart = lowestNoteNearestC;
    int possibleShiftAmount = 0;

    dmf::Note currentHighEnd = lowestNoteNearestC;
    currentHighEnd.octave += 3;
    if (m_NumMODSamples == 2)
        currentHighEnd.octave += 3;

    assert(currentHighEnd > highestNote);

    const int highEndSlack = dmf::GetNoteRange(highestNote, currentHighEnd);
    if (highEndSlack > 12 && lowestNoteNearestC.octave >= 1) // 1 octave or more of slack at upper end, plus room to shift at bottom
        possibleShiftAmount = 1; // Can shift MOD notes down 1 octave
    if (highEndSlack > 24 && lowestNoteNearestC.octave >= 1) // 2 octaves of slack at upper end, plus room to shift at bottom
        possibleShiftAmount = 2; // Can shift MOD notes down 2 octaves

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
        m_RangeStart.emplace_back(dmf::NotePitch::C, lowestPossibleRangeStart.octave + 3);
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

Note DMFSampleMapper::GetMODNote(const dmf::Note& dmfNote, NoteRange& modNoteRange) const
{
    // Returns the MOD note to use given a DMF note. Also returns which
    //      MOD sample the MOD note needs to use. The MOD note's octave
    //      and pitch should always be exactly what gets displayed in ProTracker.

    Note modNote(dmf::NotePitch::C, 0);
    modNoteRange = NoteRange::First;

    if (m_SampleType == SampleType::Silence)
        return modNote;

    modNoteRange = GetMODNoteRange(dmfNote);
    const dmf::Note& rangeStart = m_RangeStart[static_cast<int>(modNoteRange)];

    modNote.pitch = dmfNote.pitch;
    modNote.octave = dmfNote.octave - rangeStart.octave + 1;
    // NOTE: The octave shift is already factored into rangeStart.
    //          The "+ 1" is because MOD's range starts at C-1 not C-0.

    assert(modNote.octave >= 1 && "Note octave is too low.");
    assert(modNote.octave <= 3 && "Note octave is too high.");

    return modNote;
}

DMFSampleMapper::NoteRange DMFSampleMapper::GetMODNoteRange(dmf::Note dmfNote) const
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

mod_sample_id_t DMFSampleMapper::GetMODSampleId(dmf::Note dmfNote) const
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

    const bool silent = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Silence).GetValue<bool>();

    if (!silent)
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
        // According to real ProTracker files viewed in a hex viewer, the 30th and final byte
        //    of a blank sample is 0x01 and all 29 other bytes are 0x00.
        for (int j = 0; j < 29; j++)
        {
            fout.put(0);
        }
        fout.put(1);
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

static std::string GetWarningMessage(MOD::ConvertWarning warning)
{
    switch (warning)
    {
        case MOD::ConvertWarning::PitchHigh:
            return "Cannot use the highest Deflemask note (C-8) on some MOD players including ProTracker.";
        case MOD::ConvertWarning::TempoLow:
            return std::string("Tempo is too low for ProTracker. Using 16 bpm instead.\n")
                    + std::string("         ProTracker only supports tempos between 16 and 127.5 bpm.");
        case MOD::ConvertWarning::TempoHigh:
            return std::string("Tempo is too high for ProTracker. Using 127.5 bpm instead.\n")
                    + std::string("         ProTracker only supports tempos between 16 and 127.5 bpm.");
        case MOD::ConvertWarning::TempoPrecision:
            return std::string("Tempo does not exactly match, but the closest possible value was used.\n");
        case MOD::ConvertWarning::EffectIgnored:
            return "A Deflemask effect was ignored due to limitations of the MOD format.";
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
                case (int)MOD::ConvertError::WaveDownsample:
                    return std::string("Cannot use wavetable instrument #") + arg + std::string(" without loss of information.\n")
                            + std::string("       Try using the '--downsample' option.");
                case (int)MOD::ConvertError::EffectVolume:
                    return std::string("An effect and a volume change cannot both appear in the same row of the same channel.\n")
                            + std::string("       Try fixing this issue in Deflemask or use the '--effects=min' option.");
                case (int)MOD::ConvertError::MultipleEffects:
                    return "No more than one Note OFF, Volume Change, or Position Jump can appear in the same row of the same channel.";
                default:
                    return "";
            }
            break;
    }
    return "";
}

uint8_t MOD::GetMODTempo(double bpm)
{
    // Get ProTracker tempo from Deflemask bpm.
    if (bpm * 2.0 > 255.0) // If tempo is too high (over 127.5 bpm)
    {
        m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::TempoHigh));
        return 255;
    }
    else if (bpm * 2.0 < 32.0) // If tempo is too low (under 16 bpm)
    {
        m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::TempoLow));
        return 32;
    }
    else // Tempo is okay for ProTracker
    {
        // ProTracker tempo is twice the Deflemask bpm.
        return static_cast<uint8_t>(bpm * 2);
    }
}

bool GetTempoAndSpeedFromBPM(double desiredBPM, unsigned& tempo, unsigned& speed)
{
    // Brute force function to get the Tempo/Speed pair which produces a BPM as close as possible to the desired BPM
    // Returns true if the desired BPM is outside the range of tempos playable by ProTracker.

    tempo = 0;
    speed = 0;
    double bestBPMDiff = 9999999.0;

    for (unsigned d = 1; d <= 32; d++)
    {
        if (3 * 33.0 / d > desiredBPM || desiredBPM > 3 * 255.0 / d)
            continue; // Not even possible with this speed value
        
        for (unsigned n = 33; n <= 255; n++)
        {
            const double bpm = 3.0 * (double)n / d;
            if (std::abs(desiredBPM - bpm) < bestBPMDiff)
            {
                tempo = n;
                speed = d;
                bestBPMDiff = desiredBPM - bpm;
            }
        }
    }

    if (tempo == 0) // Never found a tempo/speed pair that approximates the BPM
        return true;

    return false;
}

unsigned GCD(unsigned u, unsigned v)
{
    if (v == 0)
        return u;
    return GCD(v, u % v);
}

Note::Note(const dmf::Note& dmfNote)
{
    this->pitch = dmfNote.pitch % 12;
    this->octave = dmfNote.octave;
}

Note::Note(dmf::NotePitch p, uint16_t o)
{
    this->pitch = (uint16_t)p % 12;
    this->octave = o;
}

Note& Note::operator=(const dmf::Note& dmfNote)
{
    this->pitch = dmfNote.pitch % 12;
    this->octave = dmfNote.octave;
    return *this;
}

dmf::Note Note::ToDMFNote() const
{
    dmf::Note n;
    n.octave = octave;
    n.pitch = pitch;
    return n;
}
