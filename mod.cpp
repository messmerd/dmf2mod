/*
    mod.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Implements the Module-derived class for ProTracker's MOD files.

    Several limitations apply in order to export. For example, 
    for DMF --> MOD, the DMF file must use the Game Boy system, 
    patterns must have 64 rows, only one effect column is allowed 
    per channel, etc.
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

// Finish setup
const std::vector<std::string> MODOptions = {"--downsample", "--effects=[min,max]"};
REGISTER_MODULE_CPP(MOD, MODConversionOptions, ModuleType::MOD, "mod", MODOptions)

static const unsigned int MOD_NOTE_VOLUMEMAX = 64u; // Yes, there are 65 different values for the volume
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

static std::vector<int8_t> GenerateSquareWaveSample(unsigned dutyCycle, unsigned length);
static std::vector<int8_t> GenerateWavetableSample(uint32_t* wavetableData, unsigned length);

static std::string ErrorMessageCreator(Status::Category category, int errorCode, const std::string& arg);
static std::string GetWarningMessage(MOD::ConvertWarning warning);

// The current square wave duty cycle, note volume, and other information that the 
//      tracker stores for each channel while playing a tracker file.
typedef struct MODChannelState
{
    DMF_GAMEBOY_CHANNEL channel;
    uint8_t dutyCycle;
    uint8_t wavetable;
    bool sampleChanged; // True if dutyCycle or wavetable just changed
    int16_t volume;
    bool notePlaying;
    bool onHighNoteRange;
    bool needToSetVolume;
} MODChannelState;


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

MOD::MOD()
{
    m_Status.SetErrorMessageCreator(&ErrorMessageCreator);
}

bool MOD::Export(const std::string& filename)
{
    m_Status.Clear();
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile.is_open())
    {
        m_Status.SetError(Status::Category::Export, Status::ExportError::FileOpen);
        return true;
    }

    ExportModuleName(outFile);
    ExportSampleInfo(outFile);
    ExportModuleInfo(outFile);
    ExportPatterns(outFile);
    ExportSampleData(outFile);

    outFile.close();

    if (!ModuleUtils::GetCoreOptions().silent)
        std::cout << "Saved MOD file to disk.\n\n";

    return false;
}

bool MOD::ConvertFrom(const Module* input, const ConversionOptionsPtr& options)
{
    const std::set<std::string> supportedInputTypes = {"dmf"};

    m_Status.Clear();

    if (!input)
    {
        m_Status.SetError(Status::Category::Convert, Status::ConvertError::InvalidArgument);
        return true;
    }
    
    if (supportedInputTypes.count(input->GetFileExtension()) == 0)
    {
        m_Status.SetError(Status::Category::Convert, Status::ConvertError::UnsupportedInputType, input->GetFileExtension());
        return true;
    }

    const bool silent = ModuleUtils::GetCoreOptions().silent;
    
    m_Options = reinterpret_cast<MODConversionOptions*>(options.get());

    // Convert from DMF here. Other module types can be added in later.
    
    const DMF& dmf = *(input->Cast<DMF>());

    if (!silent)
        std::cout << "Starting to convert to MOD...\n";
    
    if (dmf.GetSystem().id != DMF::SYSTEMS(SYS_GAMEBOY).id) // If it's not a Game Boy
    {
        m_Status.SetError(Status::Category::Convert, MOD::ConvertError::NotGameBoy);
        return true;
    }

    const ModuleInfo& moduleInfo = dmf.GetModuleInfo();
    m_NumberOfRowsInPatternMatrix = moduleInfo.totalRowsInPatternMatrix + (int)m_UsingSetupPattern;
    if (m_NumberOfRowsInPatternMatrix > 64) // totalRowsInPatternMatrix is 1 more than it actually is 
    {
        m_Status.SetError(Status::Category::Convert, MOD::ConvertError::TooManyPatternMatrixRows);
        return true;
    }

    if (moduleInfo.totalRowsPerPattern != 64)
    { 
        m_Status.SetError(Status::Category::Convert, MOD::ConvertError::Not64RowPattern);
        return true;
    }

    m_NumberOfChannels = DMF::SYSTEMS(SYS_GAMEBOY).channels;
    m_NumberOfChannelsPowOfTwo = 0;
    unsigned channels = m_NumberOfChannels;
    while (channels > 1)
    {
        channels >>= 1;
        m_NumberOfChannelsPowOfTwo++;
    }

    ///////////////// CONVERT SONG NAME

    m_ModuleName.clear();
    for (int i = 0; i < dmf.GetVisualInfo().songNameLength; i++)
    {
        m_ModuleName += (char)tolower(dmf.GetVisualInfo().songName[i]);
    }

    ///////////////// CONVERT SAMPLE INFO

    if (!silent)
        std::cout << "Converting sample info...\n";

    if (CreateSampleMapping(dmf))
    {
        // An error occurred
        return true;
    }

    // Not needed anymore:
    m_SampleIdLowestHighestNotesMap.clear();
    
    ///////////////// CONVERT SAMPLE DATA

    if (!silent)
        std::cout << "Converting samples...\n";

    if (ConvertSampleData(dmf))
    {
        return true;
    }

    ///////////////// CONVERT PATTERN DATA

    if (!silent)
        std::cout << "Converting pattern data...\n";

    if (ConvertPatterns(dmf))
        return true;
    
    ///////////////// CLEAN UP

    if (!silent)
        std::cout << "Done converting to MOD.\n\n";

    // Success
    return false;
}

///////// CONVERT FROM DMF /////////

bool MOD::CreateSampleMapping(const DMF& dmf)
{
    // This function loops through all DMF pattern contents to find the highest and lowest notes 
    //  for each square wave duty cycle and each wavetable. It also finds which SQW duty cycles are 
    //  used and which wavetables are used and stores this info in m_SampleMap.
    //  Then it calls the function finalizeSampMap.

    m_SampleMap2.clear();
    m_SampleIdLowestHighestNotesMap.clear();
    m_SilentSampleNeeded = false;

    m_DMFTotalWavetables = dmf.GetTotalWavetables();

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.
    MODChannelState state[m_NumberOfChannels], stateJumpCopy[m_NumberOfChannels];
    for (unsigned i = 0; i < m_NumberOfChannels; i++)
    {
        state[i].channel = (DMF_GAMEBOY_CHANNEL)i;   // Set channel types: SQ1, SQ2, WAVE, NOISE.
        state[i].dutyCycle = 0; // Default is 0 or a 12.5% duty cycle square wave.
        state[i].wavetable = 0; // Default is wavetable #0.
        state[i].sampleChanged = true; // Whether dutyCycle or wavetable recently changed
        state[i].volume = DMF_NOTE_VOLUMEMAX; // The max volume for a channel (in DMF units)
        state[i].notePlaying = false; // Whether a note is currently playing on a channel
        state[i].onHighNoteRange = false; // Whether a note is using the PT sample for the high note range.
        state[i].needToSetVolume = false; // Whether the volume needs to be set (can happen after sample changes).

        stateJumpCopy[i] = state[i]; // Shallow copy, but it's ok since there are no pointers to anything.
    }

    // The main MODChannelState structs should NOT update during patterns or parts of 
    //   patterns that the Position Jump (Bxx) effect skips over. (Ignore loops)
    //   Keep a copy of the main state for each channel, and once it reaches the 
    //   jump destination, overwrite the current state with the copied state.
    bool stateSuspended = false; // true == currently in part that a Position Jump skips over
    int8_t jumpDestination = -1; // Pattern matrix row where you are jumping to. Not a loop.

    int16_t effectCode, effectValue;

    const ModuleInfo& moduleInfo = dmf.GetModuleInfo();
    PatternRow*** const patternValues = dmf.GetPatternValues();
    uint8_t** const patternMatrixValues = dmf.GetPatternMatrixValues();
    
    // Most of the following nested for loop is copied from the export pattern data loop in ConvertPatterns.
    // I didn't want to do this, but I think having two of the same loop is the only simple way.
    // Loop through SQ1, SQ2, and WAVE channels:
    for (int chan = DMF_GAMEBOY_SQW1; chan <= DMF_GAMEBOY_WAVE; chan++)
    {
        // Loop through Deflemask patterns
        for (int patMatRow = 0; patMatRow < moduleInfo.totalRowsInPatternMatrix; patMatRow++)
        {
            // Row within pattern
            for (int patRow = 0; patRow < 64; patRow++)
            {
                PatternRow pat = patternValues[chan][patternMatrixValues[chan][patMatRow]][patRow];
                effectCode = pat.effectCode[0];
                effectValue = pat.effectValue[0];

                // If just arrived at jump destination:
                if (patMatRow == jumpDestination && patRow == 0 && stateSuspended)
                { 
                    // Restore state copies
                    for (unsigned v = 0; v < m_NumberOfChannels; v++)
                    {
                        state[v] = stateJumpCopy[v];
                    }
                    stateSuspended = false;
                    jumpDestination = -1;
                }
                
                // If a Position Jump command was found and it's not in a section skipped by another Position Jump:
                if (effectCode == DMF_POSJUMP && !stateSuspended)
                {
                    if (effectValue >= patMatRow) // If not a loop
                    {
                        // Save copies of states
                        for (unsigned v = 0; v < m_NumberOfChannels; v++)
                        {
                            stateJumpCopy[v] = state[v];
                        }
                        stateSuspended = true;
                        jumpDestination = effectValue;
                    }
                }
                else if (effectCode == DMF_SETDUTYCYCLE && state[chan].dutyCycle != effectValue && chan <= DMF_GAMEBOY_SQW2) // If sqw channel duty cycle needs to change 
                {
                    if (effectValue >= 0 && effectValue <= 3)
                    {
                        state[chan].dutyCycle = effectValue;
                        state[chan].sampleChanged = true;
                    }
                }
                else if (effectCode == DMF_SETWAVE && state[chan].wavetable != effectValue && chan == DMF_GAMEBOY_WAVE) // If wave channel wavetable needs to change 
                {
                    if (effectValue >= 0 && effectValue < m_DMFTotalWavetables)
                    {
                        state[chan].wavetable = effectValue;
                        state[chan].sampleChanged = true;
                    }
                }
                
                if (pat.note.pitch == DMF_NOTE_OFF && state[chan].notePlaying)
                {
                    m_SilentSampleNeeded = true;
                    state[chan].notePlaying = false;
                }

                // A note on the SQ1, SQ2, or WAVE channels:
                if (pat.note.pitch >= 1 && pat.note.pitch <= 12 && chan != DMF_GAMEBOY_NOISE)
                {
                    state[chan].notePlaying = true;

                    mod_sample_id_t sampleId = chan == DMF_GAMEBOY_WAVE ? state[chan].wavetable + 4 : state[chan].dutyCycle;

                    // Mark this square wave or wavetable as used
                    m_SampleMap2[sampleId] = {};

                    // Get lowest/highest notes
                    if (m_SampleIdLowestHighestNotesMap.count(sampleId) == 0) // 1st time
                    {
                        MODNote n = { .pitch = pat.note.pitch, .octave = pat.note.octave };
                        m_SampleIdLowestHighestNotesMap[sampleId] = std::pair<MODNote, MODNote>(n, n);
                    }
                    else
                    {
                        auto& notePair = m_SampleIdLowestHighestNotesMap[sampleId];
                        if (pat.note > notePair.second)
                        {
                            // Found a new highest note
                            notePair.second.pitch = pat.note.pitch;
                            notePair.second.octave = pat.note.octave;
                        }
                        if (pat.note < notePair.first)
                        {
                            // Found a new lowest note
                            notePair.first.pitch = pat.note.pitch;
                            notePair.first.octave = pat.note.octave;
                        }
                    }
                }
            }
        }
    }

    return SampleSplittingAndAssignment();
}

bool MOD::SampleSplittingAndAssignment()
{
    // This method determines whether a sample will need to be split into low and high ranges, then assigns
    //  MOD sample numbers
    
    mod_sample_id_t currentMODSampleId = 1; // Sample #0 is special in ProTracker

    if (m_SilentSampleNeeded)
    {
        // The silent sample will be sample #1
        // It will be added to the sample map AFTER the loop below, since the loop's logic doesn't pertain to it
        currentMODSampleId = 2;
    }

    // Only the samples we need will be in this map (+ the silent sample possibly)
    for (auto& mapPair : m_SampleMap2)
    {
        dmf_sample_id_t sampleId = mapPair.first;
        MODSampleInfo& modSampleInfo = mapPair.second;

        modSampleInfo.lowLength = 0;
        modSampleInfo.highLength = 0;

        const auto& lowHighNotes = m_SampleIdLowestHighestNotesMap[sampleId];
        const auto& lowestNote = lowHighNotes.first;
        const auto& highestNote = lowHighNotes.second;

        // Whether the DMF "sample" (square wave type or wavetable) will need two MOD samples to work given PT's limited range
        bool noSplittingIsNeeded = false;

        // If the range is 3 octaves or less:
        if (highestNote.octave*12 + highestNote.pitch - lowestNote.octave*12 - lowestNote.pitch <= 36)
        {
            // (Note: The note C-n in the Deflemask tracker is called C-(n-1) in DMF format because the 1st note of an octave is C# in DMF files.)

            if (lowestNote >= (Note){DMF_NOTE_C, 1} && highestNote <= (Note){DMF_NOTE_B, 4})
            {
                noSplittingIsNeeded = true;

                // If between C-2 and B-4 (Deflemask tracker note format)
                modSampleInfo.splitPoint = { DMF_NOTE_C, 1 };
                modSampleInfo.lowLength = 64; // Use sample length: 64 (Double length)
            }

            if (lowestNote >= (Note){DMF_NOTE_C, 2} && highestNote <= (Note){DMF_NOTE_B, 5})
            {
                noSplittingIsNeeded = true;

                // If between C-3 and B-5 (Deflemask tracker note format)
                modSampleInfo.splitPoint = { DMF_NOTE_C, 2 };
                modSampleInfo.lowLength = 32; // Use sample length: 32 (regular wavetable length)
            }
            else if (lowestNote >= (Note){DMF_NOTE_C, 3} && highestNote <= (Note){DMF_NOTE_B, 6})
            {
                // If between C-4 and B-6 (Deflemask tracker note format) and none of the above options work
                if (sampleId >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                    return true;
                }

                noSplittingIsNeeded = true;

                modSampleInfo.splitPoint = { DMF_NOTE_C, 3 };
                modSampleInfo.lowLength = 16; // Use sample length: 16 (half length - downsampling needed)
            }
            else if (lowestNote >= (Note){DMF_NOTE_C, 4} && highestNote <= (Note){DMF_NOTE_B, 7})
            {
                // If between C-5 and B-7 (Deflemask tracker note format)
                if (sampleId >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                    return true;
                }
                
                noSplittingIsNeeded = true;

                modSampleInfo.splitPoint = { DMF_NOTE_C, 4 };
                modSampleInfo.lowLength = 8; // Use sample length: 8 (1/4 length - downsampling needed)
            }
            else if (highestNote == (Note){DMF_NOTE_C, 7})
            {
                // If between C#5 and C-8 (highest note) (Deflemask tracker note format):
                if (sampleId >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                    return true;
                }
                
                // TODO: This note is currently unsupported. Should fail here.

                noSplittingIsNeeded = true;
                //finetune = 0; // One semitone up from B = C- ??? was 7

                modSampleInfo.splitPoint = { DMF_NOTE_C, 4 };
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
                    modSampleInfo.lowLength *= 2;
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
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                    return true;
                }
            }

            modSampleInfo.splitPoint = {DMF_NOTE_C, 4};

            if (highestNote == (Note){DMF_NOTE_C, 7})
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

    // Now add the silent sample to the sample map
    if (m_SilentSampleNeeded)
    {
        m_SampleMap2[-1] = {};
        m_SampleMap2[-1].lowId = 1; // Silent sample is always sample #1 if it is used
        m_SampleMap2[-1].highId = -1;
        m_SampleMap2[-1].lowLength = 8;
        m_SampleMap2[-1].highLength = 0;
        m_SampleMap2[-1].splitPoint = {};
    }

    m_TotalMODSamples = currentMODSampleId - 1; // Set the number of MOD samples that will be needed. (minus sample #0 which is special)
    
    // TODO: Check if there are too many samples needed here

    return 0; // Success
}

bool MOD::ConvertSampleData(const DMF& dmf)
{
    // Convert samples
    for (const auto& mapPair : m_SampleMap2)
    {
        dmf_sample_id_t sampleId = mapPair.first;
        const MODSampleInfo& modSampleInfo = mapPair.second;

        mod_sample_id_t lowId = modSampleInfo.lowId;
        mod_sample_id_t highId = modSampleInfo.highId;
        unsigned lowSampleLength = modSampleInfo.lowLength;
        unsigned highSampleLength = modSampleInfo.highLength;

        bool isSplitSample = highId != -1;

        m_Samples[lowId] = {};
        if (isSplitSample)
            m_Samples[highId] = {};

        if (sampleId == -1)
        {
            // Silence sample
            m_Samples[lowId] = std::vector<int8_t>(modSampleInfo.lowLength, 0);
        }
        else if (sampleId <= 3)
        {
            // Square wave sample
            m_Samples[lowId] = GenerateSquareWaveSample(sampleId, lowSampleLength);
            if (isSplitSample)
                m_Samples[highId] = GenerateSquareWaveSample(sampleId, highSampleLength);
        }
        else
        {
            // Wavetable sample
            const int wavetableIndex = sampleId - 4;
            uint32_t* wavetableData = dmf.GetWavetableValues()[wavetableIndex];
            
            m_Samples[lowId] = GenerateWavetableSample(wavetableData, lowSampleLength);
            if (isSplitSample)
                m_Samples[highId] = GenerateWavetableSample(wavetableData, highSampleLength);
        }
    }

    return false;
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

bool MOD::ConvertPatterns(const DMF& dmf)
{
    m_Patterns.assign(m_NumberOfRowsInPatternMatrix, {});

    m_InitialTempo = GetMODTempo(dmf.GetBPM());

    if (m_UsingSetupPattern)
    {
        m_Patterns[0].assign(64 * m_NumberOfChannels, {0, 0, 0, 0});

        // Set initial tempo (An approximation. ProTracker can only support Deflemask tempo between 16 and 127.5 bpm.)
        ChannelRow tempoRow;
        tempoRow.SampleNumber = 0;
        tempoRow.SamplePeriod = 0;
        tempoRow.EffectCode = MOD_SETSPEED;
        tempoRow.EffectValue = m_InitialTempo;
        SetChannelRow(0, 0, 0, tempoRow);

        // Set position jump (once the Pattern Break effect is implemented, it can be used instead.)
        ChannelRow posJumpRow;
        posJumpRow.SampleNumber = 0;
        posJumpRow.SamplePeriod = 0;
        posJumpRow.EffectCode = MOD_POSJUMP;
        posJumpRow.EffectValue = 1;
        SetChannelRow(0, 0, 1, posJumpRow);

        // All other channel rows in the pattern are already zeroed out so nothing needs to be done for them
    }

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.
    MODChannelState state[m_NumberOfChannels], stateJumpCopy[m_NumberOfChannels];
    for (unsigned i = 0; i < m_NumberOfChannels; i++)
    {
        state[i].channel = (DMF_GAMEBOY_CHANNEL)i;   // Set channel types: SQ1, SQ2, WAVE, NOISE.
        state[i].dutyCycle = 0; // Default is 0 or a 12.5% duty cycle square wave.
        state[i].wavetable = 0; // Default is wavetable #0.
        state[i].sampleChanged = true; // Whether dutyCycle or wavetable recently changed
        state[i].volume = DMF_NOTE_VOLUMEMAX; // The max volume for a channel (in DMF units)
        state[i].notePlaying = false; // Whether a note is currently playing on a channel
        state[i].onHighNoteRange = false; // Whether a note is using the PT sample for the high note range.
        state[i].needToSetVolume = false; // Whether the volume needs to be set (can happen after sample changes).

        stateJumpCopy[i] = state[i]; // Shallow copy, but it's ok since there are no pointers to anything.
    }
    
    PatternRow pat;
    int16_t effectCode, effectValue;

    uint8_t** const patternMatrixValues = dmf.GetPatternMatrixValues();
    PatternRow*** const patternValues = dmf.GetPatternValues();

    // The main MODChannelState structs should NOT update during patterns or parts of 
    //   patterns that the Position Jump (Bxx) effect skips over. (Ignore loops)
    //   Keep a copy of the main state for each channel, and once it reaches the 
    //   jump destination, overwrite the current state with the copied state.
    bool stateSuspended = false; // true == currently in part that a Position Jump skips over
    int8_t jumpDestination = -1; // Pattern matrix row where you are jumping to. Not a loop.

    const int dmfTotalRowsInPatternMatrix = dmf.GetModuleInfo().totalRowsInPatternMatrix;

    // Loop through ProTracker pattern matrix rows (corresponds to pattern numbers):
    for (int patMatRow = 0; patMatRow < dmfTotalRowsInPatternMatrix; patMatRow++)
    {
        // patMatRow is in MOD pattern matrix rows, not DMF
        m_Patterns[patMatRow + (int)m_UsingSetupPattern].assign(64 * m_NumberOfChannels, {});

        // Loop through rows in a pattern:
        for (unsigned patRow = 0; patRow < 64; patRow++)
        {
            // Loop through channels:
            for (unsigned chan = 0; chan < m_NumberOfChannels; chan++)
            {
                pat = patternValues[chan][patternMatrixValues[chan][patMatRow]][patRow];
                effectCode = pat.effectCode[0];
                effectValue = pat.effectValue[0];

                // If just arrived at jump destination:
                if (patMatRow == jumpDestination && patRow == 0 && stateSuspended)
                {
                    // Restore state copies
                    for (unsigned v = 0; v < m_NumberOfChannels; v++)
                    {
                        state[v] = stateJumpCopy[v];
                    }
                    stateSuspended = false;
                    jumpDestination = -1;
                }

                #pragma region UPDATE_STATE
                // If a Position Jump command was found and it's not in a section skipped by another Position Jump:
                if (effectCode == DMF_POSJUMP && !stateSuspended)
                {
                    if (effectValue >= patMatRow) // If not a loop
                    {
                        // Save copies of states
                        for (unsigned v = 0; v < m_NumberOfChannels; v++)
                        {
                            stateJumpCopy[v] = state[v];
                        }
                        stateSuspended = true;
                        jumpDestination = effectValue;
                    }
                }
                else if (effectCode == DMF_SETDUTYCYCLE && state[chan].dutyCycle != effectValue && chan <= DMF_GAMEBOY_SQW2) // If sqw channel duty cycle needs to change 
                {
                    if (effectValue >= 0 && effectValue <= 3)
                    {
                        state[chan].dutyCycle = effectValue;
                        state[chan].sampleChanged = true;
                    }
                }
                else if (effectCode == DMF_SETWAVE && state[chan].wavetable != effectValue && chan == DMF_GAMEBOY_WAVE) // If wave channel wavetable needs to change 
                {
                    if (effectValue >= 0 && effectValue < m_DMFTotalWavetables)
                    {
                        state[chan].wavetable = effectValue;
                        state[chan].sampleChanged = true;
                    }
                }
                #pragma endregion

                ChannelRow tempChannelRow;

                if (ConvertChannelRow(dmf, pat, state[chan], tempChannelRow))
                {
                    // Error occurred while writing the pattern row
                    m_SampleIdLowestHighestNotesMap.clear();
                    m_SampleMap2.clear();
                    return true;
                }

                SetChannelRow(patMatRow + (int)m_UsingSetupPattern, patRow, chan, tempChannelRow);
            }
        }
    }

    return false;
}

bool MOD::ConvertChannelRow(const DMF& dmf, const PatternRow& pat, MODChannelState& state, ChannelRow& modChannelRow)
{
    // Writes 4 bytes of pattern row information to the .mod file
    uint16_t effectCode, effectValue;
    if (ConvertEffect(pat, state, effectCode, effectValue))
    {
        return true; // An error occurred
    }

    if (pat.note.pitch == DMF_NOTE_EMPTY)  // No note is playing. Only handle effects.
    {
        modChannelRow.SampleNumber = 0;
        modChannelRow.SamplePeriod = 0;
        modChannelRow.EffectCode = effectCode;
        modChannelRow.EffectValue = effectValue;
    }
    else if (pat.note.pitch == DMF_NOTE_OFF) // Note OFF. Only handle note OFF effect.
    {
        state.notePlaying = false;

        modChannelRow.SampleNumber = GetMODSampleId(-1, {}); // Use silent sample
        modChannelRow.SamplePeriod = 214; // C-3; Doesn't really matter
        modChannelRow.EffectCode = effectCode;
        modChannelRow.EffectValue = effectValue;
    }
    else  // A note is playing
    {
        uint8_t dmfSampleId;
        uint16_t period = 0;

        uint16_t modOctave = 0; // The note's octave to export to the MOD file.

        if (pat.note.pitch >= 1 && pat.note.pitch <= 12) // A note
        {
            if (state.channel != DMF_GAMEBOY_NOISE)
            {
                // The indices for this SQW / WAVE sample's low note range and high note range:
                dmfSampleId = state.channel == DMF_GAMEBOY_WAVE ? state.wavetable + 4 : state.dutyCycle;

                if (m_SampleMap2.count(dmfSampleId) > 0)
                {
                    MODSampleInfo& modSampleInfo = m_SampleMap2[dmfSampleId];
                    Note dmfSplitPoint;
                    dmfSplitPoint.octave = modSampleInfo.splitPoint.octave;
                    dmfSplitPoint.pitch = modSampleInfo.splitPoint.pitch;

                    // If using high note range:
                    if (modSampleInfo.highId != -1)
                    {
                        // If the current note is in the high note range
                        if (pat.note >= dmfSplitPoint)
                        {
                            // Find the note's octave in MOD format:
                            modOctave = pat.note.octave - dmfSplitPoint.octave;
                            if (!state.onHighNoteRange)
                            {
                                // Switching to a different note range requires the use of a different MOD sample.
                                state.sampleChanged = true;
                            }
                            state.onHighNoteRange = true;
                        }
                        else // If current note is in the low note range
                        {
                            // Find the note's octave in MOD format:
                            modOctave = pat.note.octave - dmfSplitPoint.octave + 3;
                            if (state.onHighNoteRange)
                            {
                                // Switching to a different note range requires the use of a different MOD sample.
                                state.sampleChanged = true;
                            }
                            state.onHighNoteRange = false;
                        }
                    }
                    else // Only one MOD note range for this DMF sample
                    {
                        // Find the note's octave in MOD format:
                        modOctave = pat.note.octave - dmfSplitPoint.octave;
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
        if (pat.note.pitch == DMF_NOTE_C)
        {
            modOctave++;
        }

        period = proTrackerPeriodTable[modOctave][pat.note.pitch % 12];

        uint8_t sampleNumber;
        if (!state.sampleChanged && state.notePlaying) // Sample hasn't changed (same duty cycle or wavetable as before) and a note was playing 
        {
            sampleNumber = 0; // No sample. Keeps the previous sample number and prevents channel volume from being reset.  
        }
        else if (state.channel != DMF_GAMEBOY_NOISE) // A MOD sample needs to change
        {
            MODSampleInfo* modSampleInfo = GetMODSampleInfo(dmfSampleId);
            sampleNumber = state.onHighNoteRange ? modSampleInfo->highId : modSampleInfo->lowId;

            state.sampleChanged = false; // Just changed the sample, so resetting this for next time.
            
            if (effectCode == MOD_NOEFFECT_CODE)
            {
                // When you change PT samples, the channel volume resets, so 
                //  if there are still no effects being used on this pattern row, 
                //  use a volume change effect to set the volume to where it needs to be.
                uint8_t newVolume = std::round(state.volume / (double)DMF_NOTE_VOLUMEMAX * (double)MOD_NOTE_VOLUMEMAX); // Convert DMF volume to MOD volume

                effectCode = MOD_SETVOLUME;
                effectValue = newVolume;
                
                // TODO: If the default volume of the sample is selected in a smart way, we could potentially skip having to use a volume effect sometimes
            }
        }
        else // Noise channel
        {
            sampleNumber = 0; // Keep noise channel quiet since it isn't implemented yet.
        }

        modChannelRow.SampleNumber = sampleNumber;
        modChannelRow.SamplePeriod = period;
        modChannelRow.EffectCode = effectCode;
        modChannelRow.EffectValue = effectValue;
        
        state.notePlaying = true;
    }

   return 0; // Success
}

bool MOD::ConvertEffect(const PatternRow& pat, MODChannelState& state, uint16_t& effectCode, uint16_t& effectValue)
{
    if (m_Options->GetEffects() == MODConversionOptions::EffectsEnum::Max) // If using maximum amount of effects
    {
        ConvertEffectCodeAndValue(pat.effectCode[0], pat.effectValue[0], effectCode, effectValue); // Effects must be in first row
        if (pat.volume != state.volume && pat.volume != DMF_NOTE_NOVOLUME && (pat.note.pitch >= 1 && pat.note.pitch <= 12)) // If the note volume changes, and the volume is connected to a note
        {
            if (effectCode != MOD_NOEFFECT_CODE) // If an effect is already being used
            {
                /* Unlike Deflemask, setting the volume in ProTracker requires the use of 
                    an effect, and only one effect can be used at a time per channel.
                    Same with turning a note off, which requires the EC0 command as far as I know.
                    Note that the set duty cycle effect in Deflemask is not implemented as an effect in PT, 
                    so it does not count.
                */
                m_Status.SetError(Status::Category::Convert, MOD::ConvertError::EffectVolume);
                return true;
            }
            else // Only the volume changed
            {
                uint8_t newVolume = std::round(pat.volume / (double)DMF_NOTE_VOLUMEMAX * (double)MOD_NOTE_VOLUMEMAX); // Convert DMF volume to MOD volume
                effectCode = MOD_SETVOLUME;
                effectValue = newVolume;

                state.volume = pat.volume; // Update the state
            }
        }
    }
    else // Don't use effects (except for Volume change, Note OFF, or Position Jump)
    {
        uint8_t total_effects = 0;
        int16_t volumeCopy = state.volume; // Save copy in case the volume change is canceled later 
        if (pat.volume != state.volume && pat.volume != DMF_NOTE_NOVOLUME && (pat.note.pitch >= 1 && pat.note.pitch <= 12)) // If the volume changed, we still want to handle that. Volume must be connected to a note.  
        {
            uint8_t newVolume = round(pat.volume / (double)DMF_NOTE_VOLUMEMAX * (double)MOD_NOTE_VOLUMEMAX); // Convert DMF volume to MOD volume
            
            effectCode = MOD_SETVOLUME;
            effectValue = newVolume;

            state.volume = pat.volume; // Update the state
            total_effects++;
        }

        if (pat.effectCode[0] == DMF_POSJUMP) // Position Jump. Has priority over volume changes and note cuts.
        {
            effectCode = MOD_POSJUMP;
            effectValue = pat.effectValue[0] + (int)m_UsingSetupPattern;
            
            state.volume = volumeCopy; // Cancel volume change if it occurred above.
            total_effects++;
        }

        if (total_effects > 1)
        {
            /* Unlike Deflemask, setting the volume in ProTracker requires the use of 
                an effect, and only one effect can be used at a time per channel.
                Same with turning a note off, which requires the EC0 command as far as I know.
                Note that the set duty cycle effect in Deflemask is not implemented as an effect in PT, 
                so it does not count.
            */
            m_Status.SetError(Status::Category::Convert, MOD::ConvertError::MultipleEffects);
            return true;
        }
        else if (total_effects == 0)
        {
            effectCode = MOD_NOEFFECT_CODE; // No effect
            effectValue = 0;
        }
    }
    return false; // Success
}

void MOD::ConvertEffectCodeAndValue(int16_t dmfEffectCode, int16_t dmfEffectValue, uint16_t& modEffectCode, uint16_t& modEffectValue)
{
    // An effect is represented with 12 bits, which is 3 groups of 4 bits: [e][x][y].
    // The effect code is [e] or [e][x], and the effect value is [x][y] or [y].
    // Effect codes of the form [e] are stored as [e][0x0].
    uint8_t ptEff = MOD_NOEFFECT;
    uint8_t ptEffVal = MOD_NOEFFECTVAL;

    switch (dmfEffectCode)
    {
        case DMF_NOEFFECT:
            ptEff = MOD_NOEFFECT; break; // ?
        case DMF_ARP:
            ptEff = MOD_ARP;
            ptEffVal = dmfEffectValue;
            break;
        case DMF_PORTUP:
            ptEff = MOD_PORTUP;
            ptEffVal = dmfEffectValue;
            break;
        case DMF_PORTDOWN:
            ptEff = MOD_PORTDOWN;
            ptEffVal = dmfEffectValue;
            break;
        case DMF_PORT2NOTE:
            ptEff = MOD_PORT2NOTE; 
            ptEffVal = dmfEffectValue; //CLAMP(dmfEffectValue + 6, 0x00, 0xFF); // ???
            break;
        case DMF_VIBRATO:
            ptEff = MOD_VIBRATO; break;
        case DMF_PORT2NOTEVOLSLIDE:
            ptEff = MOD_PORT2NOTEVOLSLIDE; break;
        case DMF_VIBRATOVOLSLIDE:
            ptEff = MOD_VIBRATOVOLSLIDE; break;
        case DMF_TREMOLO:
            ptEff = MOD_TREMOLO; break;
        case DMF_PANNING:
            ptEff = MOD_PANNING; break;
        case DMF_SETSPEEDVAL1:
            break; // ?
        case DMF_VOLSLIDE:
            ptEff = MOD_VOLSLIDE; break;
        case DMF_POSJUMP:
            ptEff = MOD_POSJUMP;
            ptEffVal = dmfEffectValue + (int)m_UsingSetupPattern;
            break;
        case DMF_RETRIG:
            ptEff = MOD_RETRIGGERSAMPLE; break;  // ?
        case DMF_PATBREAK:
            ptEff = MOD_PATBREAK; break;
        case DMF_ARPTICKSPEED:
            break; // ?
        case DMF_NOTESLIDEUP:
            break; // ?
        case DMF_NOTESLIDEDOWN:
            break; // ? 
        case DMF_SETVIBRATOMODE:
            break; // ?
        case DMF_SETFINEVIBRATODEPTH:
            break; // ? 
        case DMF_SETFINETUNE:
            break; // ?
        case DMF_SETSAMPLESBANK:
            break; // ? 
        case DMF_NOTECUT:
            ptEff = MOD_CUTSAMPLE;
            ptEffVal = dmfEffectValue; // ??
            break;
        case DMF_NOTEDELAY:
            ptEff = MOD_DELAYSAMPLE; break; // ?
        case DMF_SYNCSIGNAL:
            break; // This is only used when exporting as VGM in Deflemask
        case DMF_SETGLOBALFINETUNE:
            ptEff = MOD_SETFINETUNE; break; // ?
        case DMF_SETSPEEDVAL2:
            break; // ?

        // Game Boy exclusive:
        case DMF_SETWAVE:
            break; // This is handled in the ConvertChannelRow method
        case DMF_SETNOISEPOLYCOUNTERMODE:
            break; // This is probably more than I need to worry about
        case DMF_SETDUTYCYCLE:
            break; // This is handled in the ConvertChannelRow method
        case DMF_SETSWEEPTIMESHIFT:
            break; // ?
        case DMF_SETSWEEPDIR:
            break; // ?
    }

    modEffectCode = ptEff;
    modEffectValue = ptEffVal;
}

mod_sample_id_t MOD::GetMODSampleId(dmf_sample_id_t dmfSampleId, const Note& dmfNote)
{
    // Must call CreateSampleMapping first.
    // dmfSampleId == -1 --> Returns silent sample id
    // 0 <= dmfSampleId <= 3 --> Returns square wave sample id
    // dmfSampleId >= 4 --> Returns wavetable sample id

    if (IsDMFSampleUsed(dmfSampleId))
    {
        MODSampleInfo* modSampleInfo = GetMODSampleInfo(dmfSampleId);

        // If there's no highId set, there's no high/low versions, just one version stored in lowId
        if (modSampleInfo->highId == -1)
            return modSampleInfo->lowId;

        // Use DMF note to check which MOD sample ID to use based on split point
        if (dmfNote >= modSampleInfo->splitPoint)
            return modSampleInfo->highId;
        else
            return modSampleInfo->lowId;
    }
    return -1; // Sample mapping for this DMF sample does not exist. Does not need to be imported.
}

MODSampleInfo* MOD::GetMODSampleInfo(dmf_sample_id_t dmfSampleId)
{
    if (IsDMFSampleUsed(dmfSampleId))
    {
        return &m_SampleMap2[dmfSampleId];
    }
    return nullptr;
}

bool MOD::IsDMFSampleUsed(dmf_sample_id_t dmfSampleId)
{
    return m_SampleMap2.count(dmfSampleId) > 0;
}

///////// EXPORT /////////

void MOD::ExportModuleName(std::ofstream& fout) const
{
    // Print module name, truncating or padding with zeros as needed
    for (unsigned i = 0; i < 20; i++)
    {
        if (i < m_ModuleName.size())
        {
            fout.put(tolower(m_ModuleName[i]));
        }
        else
        {
            fout.put(0);
        }
    }
}

void MOD::ExportSampleInfo(std::ofstream& fout) const
{
    const int8_t finetune = 0;
    
    // Export sample info
    for (const auto& mapPair : m_SampleMap2)
    {
        dmf_sample_id_t sampleId = mapPair.first;
        const MODSampleInfo& sampleInfo = mapPair.second;

        const bool highRangeExists = sampleInfo.highId != -1;

        for (int noteRange = 0; noteRange < 1 + (int)highRangeExists; noteRange++)
        {
            std::string name;
            if (sampleId == -1) // Silent sample
            {
                name = "Silence";
            }
            else if (sampleId < 4) // Square wave
            {
                name = "SQW, Duty ";
                switch (sampleId)
                {
                    case 0: name += "12.5%"; break;
                    case 1: name += "25%"; break;
                    case 2: name += "50%"; break;
                    case 3: name += "75%"; break;
                }

                if (highRangeExists)
                {
                    if (noteRange == 0)
                        name += " (low)";
                    else
                        name += " (high)";
                }
            }
            else // Wavetable
            {
                name = "Wavetable #";
                name += std::to_string(sampleId - 4);
                
                if (highRangeExists)
                {
                    if (noteRange == 0)
                        name += " (low)";
                    else
                        name += " (high)";
                }
            }

            // Pad name with zeros
            while (name.size() < 22)
                name += " ";
            
            fout << name;

            const unsigned modSampleLength = noteRange == 0 ? sampleInfo.lowLength : sampleInfo.highLength;

            fout.put(modSampleLength >> 9);     // Length byte 0
            fout.put(modSampleLength >> 1);     // Length byte 1
            fout.put(finetune);                 // Finetune value !!!
            fout.put(MOD_NOTE_VOLUMEMAX);        // Sample volume // TODO: Optimize this?
            fout.put(0);                        // Repeat offset byte 0
            fout.put(0);                        // Repeat offset byte 1
            fout.put(modSampleLength >> 9);     // Sample repeat length byte 0
            fout.put(modSampleLength >> 1);     // Sample repeat length byte 1
        }
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
    for (const auto& sample : m_Samples)
    {
        for (int8_t value : sample.second)
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
        case MOD::ConvertWarning::EffectIgnored:
            return "A Deflemask effect was ignored due to limitations of the MOD format.";
        default:
            return "";
    }
}

std::string ErrorMessageCreator(Status::Category category, int errorCode, const std::string& arg)
{
    switch (category)
    {
        case Status::Category::Import:
            return "No error.";
        case Status::Category::Export:
            return "No error.";
        case Status::Category::Convert:
            switch (errorCode)
            {
                case (int)MOD::ConvertError::Success:
                    return "No error.";
                case (int)MOD::ConvertError::NotGameBoy:
                    return "Only the Game Boy system is currently supported.";
                case (int)MOD::ConvertError::TooManyPatternMatrixRows:
                    return "Too many rows of patterns in the pattern matrix. 64 is the maximum. (63 if using Setup Pattern.)";
                case (int)MOD::ConvertError::Not64RowPattern:
                    return std::string("Patterns must have 64 rows.\n")
                            + std::string("       A workaround for this issue is planned for a future update to dmf2mod.");
                case (int)MOD::ConvertError::WaveDownsample:
                    return std::string("Cannot use wavetable instrument #") + arg + std::string(" without loss of information.\n")
                            + std::string("       Try using the '--downsample' option.");
                case (int)MOD::ConvertError::EffectVolume:
                    return std::string("An effect and a volume change (or Note OFF) cannot both appear in the same row of the same channel.\n")
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

MODNote::operator Note() const
{
    return (Note){ .pitch = pitch, .octave = octave };
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
