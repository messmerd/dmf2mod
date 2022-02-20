/*
    mod.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Implements the ModuleInterface-derived class for ProTracker's 
    MOD files.

    Several limitations apply in order to export. For example, 
    for DMF --> MOD, the DMF file must use the Game Boy system, 
    patterns must have 64 or fewer rows, only one effect column is
    allowed per channel, etc.
*/

// TODO: Right now, only effect column 0 is used. But two dmf effects 
//      could potentially be used if one of them is 10xx or 12xx. Need to allow for that. 

// TODO: Add '--effects=none' option, which does not use any ProTracker effects. 

// TODO: Delete output file if an error occurred while creating it?

#include "mod.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <cstdio>
#include <set>

#include <cassert>

// Finish setup
const std::vector<std::string> MODOptions = {"--downsample", "--effects=[min,max]"};
REGISTER_MODULE_CPP(MOD, MODConversionOptions, ModuleType::MOD, "mod", MODOptions)

static const unsigned int MOD_NOTE_VOLUMEMAX = 64u; // Yes, there are 65 different values for the volume
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

static std::vector<int8_t> GenerateSquareWaveSample(unsigned dutyCycle, unsigned length);
static std::vector<int8_t> GenerateWavetableSample(uint32_t* wavetableData, unsigned length);

static std::string GetWarningMessage(MOD::ConvertWarning warning);

static bool GetTempoAndSpeedFromBPM(double desiredBPM, unsigned& tempo, unsigned& speed);
static unsigned GCD(unsigned u, unsigned v);

// The current square wave duty cycle, note volume, and other information that the 
//      tracker stores for each channel while playing a tracker file.
struct MODChannelState
{
    uint8_t dutyCycle;
    uint8_t wavetable;
    bool sampleChanged; // True if dutyCycle or wavetable just changed
    int16_t volume;
    bool notePlaying;
    bool onHighNoteRange;
};

struct MODState
{
    struct global
    {
        bool suspended;      // true == currently in part that a Position Jump skips over
        int jumpDestination; // MOD pattern matrix row where you are jumping to. Not a loop.
        MODEffect channelIndependentEffect;
        unsigned channel;    // The current channel in DMF or MOD
        unsigned order;      // The current pattern matrix row in DMF
        unsigned patternRow; // The current pattern row in MOD?
    } global;
    
    MODChannelState channel[4];
    MODChannelState channelCopy[4];

    MODState()
    {
        Init();
    }

    void Init()
    {
        global.suspended = false;
        global.jumpDestination = -1; 
        global.channelIndependentEffect = {MODEffectCode::NoEffectCode, MODEffectCode::NoEffectVal};
        
        global.channel = 0;         
        global.order = 0;
        global.patternRow = 0;

        for (int i = 0; i < 4; i++)
        {
            channel[i].dutyCycle = 0; // Default is 0 or a 12.5% duty cycle square wave.
            channel[i].wavetable = 0; // Default is wavetable #0.
            channel[i].sampleChanged = true; // Whether dutyCycle or wavetable recently changed
            channel[i].volume = DMFVolumeMax; // The max volume for a channel (in DMF units)
            channel[i].notePlaying = false; // Whether a note is currently playing on a channel
            channel[i].onHighNoteRange = false; // Whether a note is using the PT sample for the high note range.

            channelCopy[i] = channel[i];
        }
    }

    void Save()
    {
        // Save copy of channel states
        for (int i = 0; i < 4; i++)
        {
            channelCopy[i] = channel[i];
        }
        global.suspended = true;
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

/*
    Game Boy's range is:  C-1 -> C-8 (though in testing this, the range seems to be C-2 -> C-8 in Deflemask GUI)
    ProTracker's range is:  C-1 -> B-3  (plus octaves 0 and 4 which are non-standard) 

    If I upsample current 32-length PT square wave samples to double length (64), then C-1 -> B-3 in PT will be C-2 -> B-4 in Deflemask GUI. ("low note range")
    If I downsample current 32-length PT square wave samples to quarter length (8), then C-1 -> B-3 in PT will be C-5 -> B-7 in Deflemask GUI. ("high note range")
            If finetune == 0, then it would cover Deflemask's lowest note for the GB (C-2), but not the highest (C-10). 
    I would have to downsample wavetables in order to achieve notes of C-6 or higher. 
      And in order to reach Deflemask's 2nd highest GB note (B-7), I would need to downsample the wavetables to 1/4 of the values it normally has.  
*/

static uint16_t proTrackerPeriodTable[5][12] = {
    {1712,1616,1525,1440,1357,1281,1209,1141,1077,1017, 961, 907},  /* C-0 to B-0 */
    {856,808,762,720,678,640,604,570,538,508,480,453},              /* C-1 to B-1 */
    {428,404,381,360,339,320,302,285,269,254,240,226},              /* C-2 to B-2 */
    {214,202,190,180,170,160,151,143,135,127,120,113},              /* C-3 to B-3 */
    {107,101, 95, 90, 85, 80, 76, 71, 67, 64, 60, 57}               /* C-4 to B-4 */
};

bool MODConversionOptions::ParseArgs(std::vector<std::string>& args)
{
    unsigned i = 0;
    while (i < args.size())
    {
        bool processedFlag = false;
        if (args[i] == "--downsample")
        {
            Downsample = true;
            args.erase(args.begin() + i);
            processedFlag = true;
        }
        else if (args[i].substr(0, 10) == "--effects=")
        {
            std::string val = args[i].substr(10);
            std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return std::tolower(c); });

            if (val == "min")
                Effects = EffectsEnum::Min;
            else if (val == "max")
                Effects = EffectsEnum::Max;
            else
            {
                std::cerr << "ERROR: For the option '--effects=', the acceptable values are: min, max.\n";
                return true;
            }
            args.erase(args.begin() + i);
            processedFlag = true;
        }
        else
        {
            std::cerr << "ERROR: Unrecognized option '" << args[i] << "'\n";
            return true;
        }
        
        if (!processedFlag)
            i++;
    }

    return false;
}

void MODConversionOptions::PrintHelp()
{
    std::cout << "MOD Options:\n";

    std::cout.setf(std::ios_base::left);
    std::cout << std::setw(30) << "  --downsample" << "Allow wavetables to lose information through downsampling if needed.\n";
    std::cout << std::setw(30) << "  --effects=[min,max]" << "The number of ProTracker effects to use. (Default: max)\n";
}

MOD::MOD() {}

void MOD::ConvertFrom(const Module* input, const ConversionOptionsPtr& options)
{
    m_Status.Clear();

    if (!input)
    {
        throw MODException(ModuleException::Category::Convert, ModuleException::ConvertError::InvalidArgument);
    }

    switch (input->GetType())
    {
        case ModuleType::DMF:
            ConvertFromDMF(*(input->Cast<DMF>()), options);
            break;
        // Add other input types here if support is added
        default:
            // Unsupported input type for conversion to MOD
            throw MODException(ModuleException::Category::Convert, ModuleException::ConvertError::UnsupportedInputType, input->GetFileExtension());
    }
}

///////// CONVERT FROM DMF /////////

void MOD::ConvertFromDMF(const DMF& dmf, const ConversionOptionsPtr& options)
{
    m_Options = reinterpret_cast<MODConversionOptions*>(options.get());

    const bool silent = ModuleUtils::GetCoreOptions().silent;
    
    if (!silent)
        std::cout << "Starting to convert to MOD...\n";
    
    if (dmf.GetSystem().id != DMF::Systems(DMF::SystemType::GameBoy).id) // If it's not a Game Boy
    {
        throw MODException(ModuleException::Category::Convert, MOD::ConvertError::NotGameBoy);
    }

    const DMFModuleInfo& moduleInfo = dmf.GetModuleInfo();
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

    std::map<dmf_sample_id_t, MODMappedDMFSample> sampleMap;
    DMFConvertSamples(dmf, sampleMap);

    ///////////////// CONVERT PATTERN DATA

    if (!silent)
        std::cout << "Converting pattern data...\n";

    DMFConvertPatterns(dmf, sampleMap);
    
    ///////////////// CLEAN UP

    if (!silent)
        std::cout << "Done converting to MOD.\n\n";
}

void MOD::DMFConvertSamples(const DMF& dmf, std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap)
{
    std::map<dmf_sample_id_t, std::pair<MODNote, MODNote>> sampleIdLowestHighestNotesMap;
    DMFCreateSampleMapping(dmf, sampleMap, sampleIdLowestHighestNotesMap);
    DMFSampleSplittingAndAssignment(sampleMap, sampleIdLowestHighestNotesMap);
    DMFConvertSampleData(dmf, sampleMap);
}

void MOD::DMFCreateSampleMapping(const DMF& dmf, std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap, std::map<dmf_sample_id_t, std::pair<MODNote, MODNote>>& sampleIdLowestHighestNotesMap)
{
    // This function loops through all DMF pattern contents to find the highest and lowest notes 
    //  for each square wave duty cycle and each wavetable. It also finds which SQW duty cycles are 
    //  used and which wavetables are used and stores this info in sampleMap.
    //  This extra processing is needed because Deflemask has roughly twice the pitch range that MOD has, so
    //  a pair of samples may be needed for MOD to achieve the same pitch range. These extra samples are only
    //  used when needed.

    sampleMap.clear();

    // Lowest/highest note for each square wave duty cycle or wavetable instrument
    sampleIdLowestHighestNotesMap.clear();

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.
    MODState state;
    state.Init();

    // The main MODChannelState structs should NOT update during patterns or parts of 
    //   patterns that the Position Jump (Bxx) effect skips over. (Ignore loops)
    //   Keep a copy of the main state for each channel, and once it reaches the 
    //   jump destination, overwrite the current state with the copied state.

    const DMFModuleInfo& moduleInfo = dmf.GetModuleInfo();
    
    // Most of the following nested for loop is copied from the export pattern data loop in ConvertPatterns.
    // I didn't want to do this, but I think having two of the same loop is the only simple way.
    // Loop through SQ1, SQ2, and WAVE channels:
    for (int chan = static_cast<int>(DMFGameBoyChannel::SQW1); chan <= static_cast<int>(DMFGameBoyChannel::WAVE); chan++)
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

                MODChannelState& chanState = state.channel[chan];

                DMFChannelRow chanRow = dmf.GetChannelRow(chan, patMatRow, patRow);

                // If just arrived at jump destination:
                if (patMatRow == state.global.jumpDestination && patRow == 0 && state.global.suspended)
                {
                    // Restore state copies
                    state.Restore();
                }

                PriorityEffectsMap modEffects;
                //mod_sample_id_t modSampleId = 0;
                //uint16_t period = 0;

                if (chan == DMFGameBoyChannel::NOISE)
                {
                    modEffects = DMFConvertEffects_NoiseChannel(chanRow);
                    UpdateStatePre(dmf, state, modEffects);
                    continue;
                }

                modEffects = DMFConvertEffects(chanRow);
                UpdateStatePre(dmf, state, modEffects);
                auto extraModEffects = DMFGetAdditionalEffects(state, chanRow);
                for (const auto& effectsPair : extraModEffects)
                {
                    modEffects.insert(effectsPair);
                }

                //DMFConvertNote(state, chanRow, sampleMap, modEffects, modSampleId, period);

                // TODO: More state-related stuff could be extracted from DMFConvertNote and put into separate 
                //  method so that I don't have to copy code from it to put here.
                
                // Convert note - Note cut effect
                auto sampleChangeEffects = modEffects.equal_range(EffectPrioritySampleChange);
                if (sampleChangeEffects.first != sampleChangeEffects.second) // If sample change occurred (duty cycle, wave, or note cut effect)
                {
                    for (auto& iter = sampleChangeEffects.first; iter != sampleChangeEffects.second; ++iter)
                    {
                        MODEffect& modEffect = iter->second;
                        if (modEffect.effect == MODEffectCode::CutSample) // Note cut
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
                if (chanRow.note.HasPitch() && chan != DMFGameBoyChannel::NOISE)
                {
                    chanState.notePlaying = true;

                    MODNote modNote(chanRow.note);

                    mod_sample_id_t sampleId = chan == DMFGameBoyChannel::WAVE ? chanState.wavetable + 4 : chanState.dutyCycle;

                    // Mark this square wave or wavetable as used
                    sampleMap[sampleId] = {};

                    // Get lowest/highest notes
                    if (sampleIdLowestHighestNotesMap.count(sampleId) == 0) // 1st time
                    {
                        sampleIdLowestHighestNotesMap[sampleId] = std::pair<MODNote, MODNote>(modNote, modNote);
                    }
                    else
                    {
                        auto& notePair = sampleIdLowestHighestNotesMap[sampleId];
                        if (modNote > notePair.second)
                        {
                            // Found a new highest note
                            notePair.second = chanRow.note;
                        }
                        if (modNote < notePair.first)
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

void MOD::DMFSampleSplittingAndAssignment(std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap, const std::map<dmf_sample_id_t, std::pair<MODNote, MODNote>>& sampleIdLowestHighestNotesMap)
{
    // This method determines whether a sample will need to be split into low and high ranges, then assigns
    //  MOD sample numbers
    
    mod_sample_id_t currentMODSampleId = 1; // Sample #0 is special in ProTracker

    // Only the samples we need will be in this map (+ the silent sample possibly)
    for (auto& mapPair : sampleMap)
    {
        dmf_sample_id_t sampleId = mapPair.first;
        MODMappedDMFSample& modSampleInfo = mapPair.second;

        // Special handling of silent sample
        if (sampleId == -1)
        {
            modSampleInfo.lowId = 1; // Silent sample is always sample #1 if it is used
            modSampleInfo.highId = -1;
            modSampleInfo.lowLength = 8;
            modSampleInfo.highLength = 0;
            modSampleInfo.splitPoint = MODNote();
            currentMODSampleId++;
            continue;
        }

        modSampleInfo.lowLength = 0;
        modSampleInfo.highLength = 0;

        const auto& lowHighNotes = sampleIdLowestHighestNotesMap.at(sampleId);
        const auto& lowestNote = lowHighNotes.first;
        const auto& highestNote = lowHighNotes.second;

        // Whether the DMF "sample" (square wave type or wavetable) will need two MOD samples to work given PT's limited range
        bool noSplittingIsNeeded = false;

        // If the range is 3 octaves or less:
        if (DMFGetNoteRange(lowestNote, highestNote) <= 36)
        {
            // (Note: The note C-n in the Deflemask tracker is called C-(n-1) in DMF format because the 1st note of an octave is C# in DMF files.)

            if (lowestNote < DMFNote(DMFNotePitch::C, 2))
                assert(false && "Lowest note is too low!");

            if (lowestNote >= DMFNote(DMFNotePitch::C, 2) && highestNote <= DMFNote(DMFNotePitch::B, 4))
            {
                noSplittingIsNeeded = true;

                // If between C-2 and B-4 (Deflemask tracker note format)
                modSampleInfo.splitPoint = DMFNote(DMFNotePitch::C, 2);
                modSampleInfo.lowLength = 64; // Use sample length: 64 (Double length)
            }

            if (lowestNote >= DMFNote(DMFNotePitch::C, 3) && highestNote <= DMFNote(DMFNotePitch::B, 5))
            {
                noSplittingIsNeeded = true;

                // If between C-3 and B-5 (Deflemask tracker note format)
                modSampleInfo.splitPoint = DMFNote(DMFNotePitch::C, 3);
                modSampleInfo.lowLength = 32; // Use sample length: 32 (regular wavetable length)
            }
            else if (lowestNote >= DMFNote(DMFNotePitch::C, 4) && highestNote <= DMFNote(DMFNotePitch::B, 6))
            {
                // If between C-4 and B-6 (Deflemask tracker note format) and none of the above options work
                if (sampleId >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    throw MODException(ModuleException::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                }

                noSplittingIsNeeded = true;

                modSampleInfo.splitPoint = DMFNote(DMFNotePitch::C, 4);
                modSampleInfo.lowLength = 16; // Use sample length: 16 (half length - downsampling needed)
            }
            else if (lowestNote >= DMFNote(DMFNotePitch::C, 5) && highestNote <= DMFNote(DMFNotePitch::B, 7))
            {
                // If between C-5 and B-7 (Deflemask tracker note format)
                if (sampleId >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    throw MODException(ModuleException::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                }
                
                noSplittingIsNeeded = true;

                modSampleInfo.splitPoint = DMFNote(DMFNotePitch::C, 5);
                modSampleInfo.lowLength = 8; // Use sample length: 8 (1/4 length - downsampling needed)
            }
            else if (highestNote == DMFNote(DMFNotePitch::C, 8))
            {
                // If between C#5 and C-8 (highest note) (Deflemask tracker note format):
                if (sampleId >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    throw MODException(ModuleException::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                }
                
                // TODO: This note is currently unsupported. Should fail here.
                assert(false && "C-8 note");

                noSplittingIsNeeded = true;
                //finetune = 0; // One semitone up from B = C- ??? was 7

                modSampleInfo.splitPoint = DMFNote(DMFNotePitch::C, 5);
                modSampleInfo.lowLength = 8; // Use sample length: 8 (1/4 length - downsampling needed)
            }

            if (noSplittingIsNeeded) // If one of the above options worked
            {
                if (sampleId >= 4) // If on a wavetable instrument
                {
                    // Double wavetable sample length.
                    // This will lower all wavetable instruments by one octave, which should make it
                    // match the octave for wavetable instruments in Deflemask
                    
                    // TODO: Is this correct?
                    //modSampleInfo.lowLength *= 2;
                }

                modSampleInfo.lowId = currentMODSampleId; // Assign MOD sample number to this square wave / WAVE sample
                modSampleInfo.highId = -1; // No MOD sample needed for this square wave / WAVE sample (high note range)

                currentMODSampleId++;
            }
        }

        // If none of the options above worked, both high note range and low note range are needed.
        if (!noSplittingIsNeeded) // If splitting is necessary
        {
            // Use sample length: 64 (Double length) for low range (C-2 to B-4)
            modSampleInfo.lowLength = 64;
            // Use sample length: 8 (Quarter length) for high range (C-5 to B-7)
            modSampleInfo.highLength = 8;

            // If on a wavetable sample
            if (sampleId >= 4)
            {
                // If it cannot be downsampled (square waves can always be downsampled w/o permission):
                if (!m_Options->GetDownsample())
                {
                    throw MODException(ModuleException::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                }
            }

            modSampleInfo.splitPoint = DMFNote(DMFNotePitch::C, 5);

            if (highestNote == DMFNote(DMFNotePitch::C, 8))
            {
                m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::PitchHigh));
            }

            /*
            // If lowest possible note is needed:
            if (lowestNote == (Note){DMF_NOTE_C, 1})
            {
                // If highest and lowest possible notes are both needed:
                if (highestNote == (Note){DMF_NOTE_C, 7})
                {
                    ///std::cout << "WARNING: Can't use the highest Deflemask note (C-8).\n";
                    //std::cout << "WARNING: Can't use both the highest note (C-8) and the lowest note (C-2).\n";
                }
                else // Only highest possible note is needed 
                {
                    finetune = 7; // One semitone up from B = C-  ???
                }
            }
            */

            modSampleInfo.lowId = currentMODSampleId++; // Assign MOD sample number to this square wave / WAVE sample
            modSampleInfo.highId = currentMODSampleId++; // Assign MOD sample number to this square wave / WAVE sample
        }
    }

    m_TotalMODSamples = currentMODSampleId - 1; // Set the number of MOD samples that will be needed. (minus sample #0 which is special)
    
    // TODO: Check if there are too many samples needed here
}

void MOD::DMFConvertSampleData(const DMF& dmf, const std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap)
{
    // Fill out information needed to define a MOD sample
    m_Samples.clear();
    for (const auto& mapPair : sampleMap)
    {
        dmf_sample_id_t modSampleId = mapPair.first;

        for (int noteRange = 0; noteRange < 2; noteRange++)
        {
            const MODMappedDMFSample& splitSample = mapPair.second;
            const bool highRangeExists = splitSample.highId != -1;
            if (noteRange == 1 && !highRangeExists)
                break;

            MODSample si;

            if (noteRange == 0)
            {
                si.id = splitSample.lowId;
                si.length = splitSample.lowLength;
                si.repeatLength = splitSample.lowLength;
            }
            else
            {
                si.id = splitSample.highId;
                si.length = splitSample.highLength;
                si.repeatLength = splitSample.highLength;
            }

            si.name = "";

            if (modSampleId == -1) // Silent sample
            {
                si.name = "Silence";
                si.volume = 0;
                si.data = std::vector<int8_t>(si.length, 0);
            }
            else if (modSampleId < 4) // Square wave
            {
                si.name = "SQW, Duty ";
                switch (modSampleId)
                {
                    case 0: si.name += "12.5%"; break;
                    case 1: si.name += "25%"; break;
                    case 2: si.name += "50%"; break;
                    case 3: si.name += "75%"; break;
                }

                if (highRangeExists)
                {
                    if (noteRange == 0)
                        si.name += " (low)";
                    else
                        si.name += " (high)";
                }

                si.volume = MOD_NOTE_VOLUMEMAX; // TODO: Optimize this?
                si.data = GenerateSquareWaveSample(modSampleId, si.length);
            }
            else // Wavetable
            {
                si.name = "Wavetable #";
                si.name += std::to_string(modSampleId - 4);
                
                if (highRangeExists)
                {
                    if (noteRange == 0)
                        si.name += " (low)";
                    else
                        si.name += " (high)";
                }

                si.volume = VolumeMax; // TODO: Optimize this?

                const int wavetableIndex = modSampleId - 4;
                uint32_t* wavetableData = dmf.GetWavetableValues()[wavetableIndex];
            
                si.data = GenerateWavetableSample(wavetableData, si.length);
            }

            if (si.name.size() > 22)
                throw std::invalid_argument("Sample name must be 22 characters or less");

            // Pad name with zeros
            while (si.name.size() < 22)
                si.name += " ";
            
            si.repeatOffset = 0;
            si.finetune = 0;
            
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

    for (unsigned i = 0; i < length; i++)
    {
        // Note: For the Deflemask Game Boy system, all wavetable lengths are 32.
        if (length == 128) // Quadruple length
        {
            // Convert from DMF sample values (0 to 15) to PT sample values (-128 to 127).
            sample[i] = (int8_t)((wavetableData[i / 4] / 15.f * 255.f) - 128.f);
        }
        else if (length == 64) // Double length
        {
            // Convert from DMF sample values (0 to 15) to PT sample values (-128 to 127).
            sample[i] = (int8_t)((wavetableData[i / 2] / 15.f * 255.f) - 128.f);
        }
        else if (length == 32) // Normal length 
        {
            // Convert from DMF sample values (0 to 15) to PT sample values (-128 to 127).
            sample[i] = (int8_t)((wavetableData[i] / 15.f * 255.f) - 128.f);
        }
        else if (length == 16) // Half length (loss of information)
        {
            // Take average of every two sample values to make new sample value
            int avg = (int8_t)((wavetableData[i * 2] / 15.f * 255.f) - 128.f);
            avg += (int8_t)((wavetableData[(i * 2) + 1] / 15.f * 255.f) - 128.f);
            avg /= 2;
            sample[i] = (int8_t)avg;
        }
        else if (length == 8) // Quarter length (loss of information)
        {
            // Take average of every four sample values to make new sample value
            int avg = (int8_t)((wavetableData[i * 4] / 15.f * 255.f) - 128.f);
            avg += (int8_t)((wavetableData[(i * 4) + 1] / 15.f * 255.f) - 128.f);
            avg += (int8_t)((wavetableData[(i * 4) + 2] / 15.f * 255.f) - 128.f);
            avg += (int8_t)((wavetableData[(i * 4) + 3] / 15.f * 255.f) - 128.f);
            avg /= 4;
            sample[i] = (int8_t)avg;
        }
        else
        {
            // ERROR: Invalid length
            throw std::invalid_argument("Invalid value for length in GenerateWavetableSample()");
        }
    }

    return sample;
}

void MOD::DMFConvertPatterns(const DMF& dmf, const std::map<dmf_sample_id_t, MODMappedDMFSample>& sampleMap)
{
    m_Patterns.assign(m_NumberOfRowsInPatternMatrix, {});

    unsigned initialTempo, initialSpeed; // Together these will set the initial BPM
    DMFConvertInitialBPM(dmf, initialTempo, initialSpeed);

    if (m_UsingSetupPattern)
    {
        m_Patterns[0].assign(64 * m_NumberOfChannels, {0, 0, 0, 0});

        // Set initial tempo
        MODChannelRow tempoRow;
        tempoRow.SampleNumber = 0;
        tempoRow.SamplePeriod = 0;
        tempoRow.EffectCode = MODEffectCode::SetSpeed;
        tempoRow.EffectValue = initialTempo;
        SetChannelRow(0, 0, 0, tempoRow);

        // Set initial speed
        MODChannelRow speedRow;
        speedRow.SampleNumber = 0;
        speedRow.SamplePeriod = 0;
        speedRow.EffectCode = MODEffectCode::SetSpeed;
        speedRow.EffectValue = initialSpeed;
        SetChannelRow(0, 0, 1, speedRow);

        // Set position jump (once the Pattern Break effect is implemented, it can be used instead.)
        MODChannelRow posJumpRow;
        posJumpRow.SampleNumber = 0;
        posJumpRow.SamplePeriod = 0;
        posJumpRow.EffectCode = MODEffectCode::PosJump;
        posJumpRow.EffectValue = 1;
        SetChannelRow(0, 0, 2, posJumpRow);

        // All other channel rows in the pattern are already zeroed out so nothing needs to be done for them
    }

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.

    MODState state;
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
            state.global.channelIndependentEffect = {MODEffectCode::NoEffectCode, MODEffectCode::NoEffectVal};

            // Use Pattern Break effect to allow for patterns that are less than 64 rows
            if (dmfTotalRowsPerPattern < 64 && patRow + 1 == dmfTotalRowsPerPattern /*&& !stateSuspended*/)
                state.global.channelIndependentEffect = {MODEffectCode::PatBreak, 0};

            //MODChannelRow tempChannelRows[4];

            // Loop through channels:
            for (unsigned chan = 0; chan < m_NumberOfChannels; chan++)
            {
                state.global.channel = chan;

                DMFChannelRow chanRow = dmf.GetChannelRow(chan, patMatRow, patRow);

                // If just arrived at jump destination:
                if (patMatRow == state.global.jumpDestination && patRow == 0 && state.global.suspended)
                {
                    // Restore state copies
                    state.Restore();
                }

                PriorityEffectsMap modEffects;
                mod_sample_id_t modSampleId = 0;
                uint16_t period = 0;

                if (chan == DMFGameBoyChannel::NOISE)
                {
                    modEffects = DMFConvertEffects_NoiseChannel(chanRow);
                    UpdateStatePre(dmf, state, modEffects);
                }
                else
                {
                    modEffects = DMFConvertEffects(chanRow);
                    UpdateStatePre(dmf, state, modEffects);
                    auto extraModEffects = DMFGetAdditionalEffects(state, chanRow);
                    for (const auto& effectsPair : extraModEffects)
                    {
                        modEffects.insert(effectsPair);
                    }

                    DMFConvertNote(state, chanRow, sampleMap, modEffects, modSampleId, period);
                }
                
                MODChannelRow tempChannelRow = ApplyNoteAndEffect(state, modEffects, modSampleId, period);
                SetChannelRow(patMatRow + (int)m_UsingSetupPattern, patRow, chan, tempChannelRow);
            }

            // TODO: Better channel independent effects implementation

        }

        // If the DMF has less than 64 rows per pattern, there will be extra MOD rows which will need to be blank
        for (unsigned patRow = dmfTotalRowsPerPattern; patRow < 64; patRow++)
        {
            // Loop through channels:
            for (unsigned chan = 0; chan < m_NumberOfChannels; chan++)
            {
                MODChannelRow tempChannelRow = {0, 0, 0, 0};
                SetChannelRow(patMatRow + (int)m_UsingSetupPattern, patRow, chan, tempChannelRow);
            }
        }
    }
}

// Updates Structure and Sample Change state info.
void MOD::UpdateStatePre(const DMF& dmf, MODState& state, const MOD::PriorityEffectsMap& modEffects)
{
    MODChannelState& chanState = state.channel[state.global.channel];

    // Update structure-related state info
    
    auto structureEffects = modEffects.equal_range(MODEffectPriority::EffectPriorityStructureRelated);
    if (structureEffects.first != structureEffects.second && !state.global.suspended) // If structure effects exist and state isn't suspended
    {
        for (auto& iter = structureEffects.first; iter != structureEffects.second; ++iter)
        {
            auto effectCode = iter->second.effect;
            auto effectValue = iter->second.value;

            // If a PosJump / PatBreak command was found and it's not in a section skipped by another PosJump / PatBreak:
            if (effectCode == DMFEffectCode::PosJump || (effectCode == DMFEffectCode::PatBreak && effectValue == 0))
            {
                const int dest = effectCode == DMFEffectCode::PosJump ? effectValue : state.global.order + 1;

                if (dest < 0 || dest >= dmf.GetModuleInfo().totalRowsInPatternMatrix)
                    throw std::invalid_argument("Invalid Position Jump or Pattern Break effect");
                
                if (dest >= (int)state.global.order) // If not a loop
                {
                    state.Save();
                    state.global.jumpDestination = dest;
                }
            }
        }
    }

    // Update duty cycle / wavetable state

    auto sampleChangeEffects = modEffects.equal_range(MODEffectPriority::EffectPrioritySampleChange);
    if (sampleChangeEffects.first != sampleChangeEffects.second)
    {
        for (auto& iter = sampleChangeEffects.first; iter != sampleChangeEffects.second; ++iter)
        {
            auto effectCode = iter->second.effect;
            auto effectValue = iter->second.value;

            if (effectCode == MODEffectCode::DutyCycleChange)
            {
                // Set Duty Cycle effect. Must be in square wave channel:
                if (state.global.channel == DMFGameBoyChannel::SQW1 || state.global.channel == DMFGameBoyChannel::SQW2)
                {
                    if (effectValue >= 0 && effectValue < 4)
                    {
                        // Update state
                        chanState.dutyCycle = effectValue;
                        chanState.sampleChanged = true;
                    }
                }
            }
            else if (effectCode == MODEffectCode::WavetableChange)
            {
                // Set Wave effect. Must be in wave channel:
                if (state.global.channel == DMFGameBoyChannel::WAVE)
                {
                    if (effectValue >= 0 && effectValue < dmf.GetTotalWavetables())
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

/* 
void MOD::UpdateStatePost(const DMF& dmf, MODState& state, const std::multimap<MODEffectPriority, MODEffect>& modEffects)
{
    MODChannelState& chanState = state.channel[state.global.channel];

}
*/

MOD::PriorityEffectsMap MOD::DMFConvertEffects(const DMFChannelRow& pat)
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

    using EffectPair = std::pair<MODEffectPriority, MODEffect>;
    std::multimap<MODEffectPriority, MODEffect> modEffects;

    // Convert DMF effects to MOD effects
    for (const auto& dmfEffect : pat.effect)
    {
        EffectPair effectPair {EffectPriorityUnsupportedEffect, {MODEffectCode::NoEffectCode, MODEffectCode::NoEffectVal}};

        if (dmfEffect.code == DMFEffectCode::NoEffect)
            continue;

        switch (DMFEffectCode(dmfEffect.code))
        {
        case DMFEffectCode::NoteCut:
            if (dmfEffect.value == 0)
            {
                effectPair.first = EffectPrioritySampleChange; // Can be implemented as silent sample
                effectPair.second = {MODEffectCode::CutSample, 0};
            }
            break;
        case DMFEffectCode::PatBreak:
            if (dmfEffect.value == 0)
            {
                effectPair.first = EffectPriorityStructureRelated; // Only D00 is supported at the moment
                effectPair.second = {MODEffectCode::PatBreak, 0};
            }
            break;
        case DMFEffectCode::PosJump:
        {
            effectPair.first = EffectPriorityStructureRelated;
            uint16_t dest = dmfEffect.value + (int)m_UsingSetupPattern;
            effectPair.second = {MODEffectCode::PosJump, dest};
            break;
        }
            
        default:
            break; // Unsupported. priority remains UnsupportedEffect
        }

        switch (DMFGameBoyEffectCode(dmfEffect.code))
        {
        case DMFGameBoyEffectCode::SetDutyCycle:
            effectPair.first = EffectPrioritySampleChange;
            effectPair.second = {MODEffectCode::DutyCycleChange, (uint16_t)dmfEffect.value};
            break;
        case DMFGameBoyEffectCode::SetWave:
            effectPair.first = EffectPrioritySampleChange;
            effectPair.second = {MODEffectCode::WavetableChange, (uint16_t)(dmfEffect.value)};
            break;
        default:
            break; // Unsupported. priority remains UnsupportedEffect
        }

        modEffects.insert(effectPair);
    }

    return modEffects;
}

MOD::PriorityEffectsMap MOD::DMFConvertEffects_NoiseChannel(const DMFChannelRow& pat)
{
    // Temporary method until the Noise channel is supported.
    // Only Pattern Break and Jump effects on the noise channel are converted for now.

    PriorityEffectsMap modEffects;

    for (auto& dmfEffect : pat.effect)
    {
        if (dmfEffect.code == (int)DMFEffectCode::PatBreak && dmfEffect.value == 0)
        {
            // Only D00 is supported at the moment
            modEffects.insert({EffectPriorityStructureRelated, {MODEffectCode::PatBreak, 0}});
            break;
        }
        else if (dmfEffect.code == (int)DMFEffectCode::PosJump)
        {
            uint16_t dest = dmfEffect.value + (int)m_UsingSetupPattern;
            modEffects.insert({EffectPriorityStructureRelated, {MODEffectCode::PosJump, dest}});
            break;
        }
    }
    return modEffects;
}

MOD::PriorityEffectsMap MOD::DMFGetAdditionalEffects(MODState& state, const DMFChannelRow& pat)
{
    PriorityEffectsMap modEffects;

    MODChannelState& chanState = state.channel[state.global.channel];
    
    // Add volume change effect if needed. TODO: Use volume optimization later
    if (chanState.sampleChanged || (pat.volume != chanState.volume && pat.volume != DMFNoVolume)) // If volume changed
    {
        if (pat.note.HasPitch() || chanState.notePlaying) // If a note is playing
        {
            uint8_t newVolume = std::round(pat.volume / (double)DMFVolumeMax * (double)VolumeMax); // Convert DMF volume to MOD volume

            modEffects.insert({EffectPriorityVolumeChange, {MODEffectCode::SetVolume, newVolume}});

            chanState.volume = pat.volume; // Update the state
        }
    }

    return modEffects;
}

MODNote MOD::DMFConvertNote(MODState& state, const DMFChannelRow& pat, const MOD::SampleMap& sampleMap, MOD::PriorityEffectsMap& modEffects, mod_sample_id_t& sampleId, uint16_t& period)
{
    MODNote modNote {0, 0};

    sampleId = 0;
    period = 0;

    // Noise channel is unsupported and should contain no notes
    if (state.global.channel == DMFGameBoyChannel::NOISE)
        return modNote;

    MODChannelState& chanState = state.channel[state.global.channel];

    // Convert note - Note cut effect // TODO: Move this to UpdateStatePre()?
    auto sampleChangeEffects = modEffects.equal_range(EffectPrioritySampleChange);
    if (sampleChangeEffects.first != sampleChangeEffects.second) // If sample change occurred (duty cycle, wave, or note cut effect)
    {
        for (auto& iter = sampleChangeEffects.first; iter != sampleChangeEffects.second; ++iter)
        {
            MODEffect& modEffect = iter->second;
            if (modEffect.effect == MODEffectCode::CutSample) // Note cut
            {
                sampleId = sampleMap.at(-1).lowId; // Use silent sample
                period = 0; // Don't need a note for the silent sample to work
                chanState.notePlaying = false;
                modEffects.erase(iter); // Consume the effect
                return modNote;
            }
        }
    }

    // Convert note - No note playing
    if (pat.note.IsEmpty()) // No note is playing. Only handle effects.
    {
        sampleId = 0; // Keeps previous sample id
        period = 0;
        return modNote;
    }
    
    // Convert note - Note OFF
    if (pat.note.IsOff()) // Note OFF. Use silent sample and handle effects.
    {
        sampleId = sampleMap.at(-1).lowId; // Use silent sample
        period = 0; // Don't need a note for the silent sample to work
        chanState.notePlaying = false;
        return modNote;
    }

    // Note is playing

    // The indices for this SQW / WAVE sample's low note range and high note range:
    dmf_sample_id_t dmfSampleId = state.global.channel == DMFGameBoyChannel::WAVE ? chanState.wavetable + 4 : chanState.dutyCycle;
    
    // If we are on the NOISE channel, dmfSampleId will go unused

    uint16_t modOctave = 0; // The note's octave to export to the MOD file.

    if (pat.note.HasPitch()) // A note
    {
        MODNote modNote(pat.note);
        if (state.global.channel != DMFGameBoyChannel::NOISE)
        {
            if (sampleMap.count(dmfSampleId) > 0)
            {
                const MODMappedDMFSample& modSampleInfo = sampleMap.at(dmfSampleId);
                
                if (state.global.channel == DMFGameBoyChannel::WAVE && modSampleInfo.splitPoint.octave != 5)
                    std::cout << "modNote.octave=" << modNote.octave << "; modSampleInfo.splitPoint.octave=" << modSampleInfo.splitPoint.octave << "\n";

                // If using high note range:
                if (modSampleInfo.highId != -1)
                {
                    // If the current note is in the high note range
                    if (modNote >= modSampleInfo.splitPoint)
                    {
                        // Find the note's octave in MOD format:
                        modOctave = pat.note.octave - modSampleInfo.splitPoint.octave + 1;
                        //std::cout << "   hi: pat.note.octave=" << pat.note.octave << " - modSampleInfo.splitPoint.octave=" << modSampleInfo.splitPoint.octave << "...modOctave=" << modOctave << "\n";
                        if (!chanState.onHighNoteRange)
                        {
                            // Switching to a different note range requires the use of a different MOD sample.
                            chanState.sampleChanged = true;
                        }
                        chanState.onHighNoteRange = true;
                    }
                    else // If current note is in the low note range
                    {
                        // Find the note's octave in MOD format:
                        modOctave = pat.note.octave - modSampleInfo.splitPoint.octave + 4;
                        //std::cout << "   lo: pat.note.octave=" << pat.note.octave << " - modSampleInfo.splitPoint.octave=" << modSampleInfo.splitPoint.octave << " + 3...modOctave=" << modOctave << "\n";
                        if (chanState.onHighNoteRange)
                        {
                            // Switching to a different note range requires the use of a different MOD sample.
                            chanState.sampleChanged = true;
                        }
                        chanState.onHighNoteRange = false;
                    }
                }
                else // Only one MOD note range for this DMF sample
                {
                    // Find the note's octave in MOD format:
                    modOctave = pat.note.octave - modSampleInfo.splitPoint.octave;
                }
            }
            else
            {
                // Throw error
            }
        }
    }

    // C# is the start of next octave in .dmf, not C- like in PT / .mod / Deflemask GUI, 
    //      so the note C- needs adjusted for exporting to mod:
    if (pat.note.pitch == DMFNotePitch::C_Alt)
    {
        assert(false && "DMF note pitch is 12 (C)");
        modOctave++;
    }

    modNote.octave = modOctave;
    modNote.pitch = pat.note.pitch % 12;

    period = proTrackerPeriodTable[modNote.octave][modNote.pitch];

    if (chanState.sampleChanged || !chanState.notePlaying)
    {
        const MODMappedDMFSample& modSampleInfo = sampleMap.at(dmfSampleId);
        sampleId = chanState.onHighNoteRange ? modSampleInfo.highId : modSampleInfo.lowId;

        chanState.sampleChanged = false; // Just changed the sample, so resetting this for next time.
        
        // When you change PT samples, the channel volume resets, so 
        //  if there are still no effects being used on this pattern row, 
        //  use a volume change effect to set the volume to where it needs to be.

        if (chanState.volume != DMFVolumeMax) // Currently, the default volume for all sample is the maximum. TODO: Can optimize
        {
            uint8_t newVolume = std::round(chanState.volume / (double)DMFVolumeMax * (double)VolumeMax); // Convert DMF volume to MOD volume

            modEffects.insert({EffectPriorityVolumeChange, {MODEffectCode::SetVolume, newVolume}});

            // TODO: If the default volume of the sample is selected in a smart way, we could potentially skip having to use a volume effect sometimes
        }
    }
    else
    {
        sampleId = 0; // Keeps the previous sample number and prevents channel volume from being reset.
    }

    chanState.notePlaying = true;
    return modNote;
}

MODChannelRow MOD::ApplyNoteAndEffect(MODState& state, const MOD::PriorityEffectsMap& modEffects, mod_sample_id_t modSampleId, uint16_t period)
{
    MODChannelRow modChannelRow;

    modChannelRow.SampleNumber = modSampleId;
    modChannelRow.SamplePeriod = period;
    modChannelRow.EffectCode = 0; // Will be set below
    modChannelRow.EffectValue = 0; // Will be set below

    MODEffect& channelIndependentEffect = state.global.channelIndependentEffect;

    // If no effects are being used here and a pat break or jump effect needs to be used, it can be done here
    if (modEffects.size() == 0)
    {
        if (channelIndependentEffect.effect != MODEffectCode::NoEffectCode)
        {
            // It's free real estate
            modChannelRow.EffectCode = channelIndependentEffect.effect;
            modChannelRow.EffectValue = channelIndependentEffect.value;

            channelIndependentEffect.effect = MODEffectCode::NoEffectCode;
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
            if (iter != modEffects.end() && channelIndependentEffect.effect == MODEffectCode::NoEffectCode)
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
            if (!effectUsed && channelIndependentEffect.effect != MODEffectCode::NoEffectCode)
            {
                //assert(false && "This code should never be reached now that DMFConvertNote() erases sample change effects.");

                // The only MOD effect was a sample change, which isn't implemented as an effect, so...
                // It's free real estate
                modChannelRow.EffectCode = channelIndependentEffect.effect;
                modChannelRow.EffectValue = channelIndependentEffect.value;

                channelIndependentEffect.effect = MODEffectCode::NoEffectCode;
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
            
            unsigned newN = n / divisor;
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
            unsigned newN = n / divisorD;

            // Make n the highest valid value.
            double divisorN = n / 255.0;
            unsigned newD = d / divisorN;

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

///////// EXPORT /////////

void MOD::Export(const std::string& filename)
{
    m_Status.Clear();
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

    if (!ModuleUtils::GetCoreOptions().silent)
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
            throw std::invalid_argument("Sample name must be 22 characters or less");

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
        return bpm * 2;
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

MODNote::MODNote(const DMFNote& dmfNote)
{
    this->pitch = dmfNote.pitch % 12;
    this->octave = dmfNote.octave;
}

MODNote& MODNote::operator=(const DMFNote& dmfNote)
{
    this->pitch = dmfNote.pitch % 12;
    this->octave = dmfNote.octave;
    return *this;
}

MODNote::operator DMFNote() const
{
    DMFNote n;
    n.pitch = pitch;
    n.octave = octave;
    return n;
}
