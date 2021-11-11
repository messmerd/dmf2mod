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

static const unsigned int PT_NOTE_VOLUMEMAX = 64u; // Yes, there are 65 different values for the volume
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

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
    m_Stream.clear();
    m_Stream.str(std::string());
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
    outFile << m_Stream.rdbuf();
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
    if (moduleInfo.totalRowsInPatternMatrix + (int)m_UsingSetupPattern > 64) // totalRowsInPatternMatrix is 1 more than it actually is 
    {
        m_Status.SetError(Status::Category::Convert, MOD::ConvertError::TooManyPatternMatrixRows);
        return true;
    }

    if (moduleInfo.totalRowsPerPattern != 64)
    { 
        m_Status.SetError(Status::Category::Convert, MOD::ConvertError::Not64RowPattern);
        return true;
    }

    ///////////////// CONVERT SONG NAME

    VisualInfo visualInfo = dmf.GetVisualInfo();

    // Print module name, truncating or padding with zeros as needed
    for (int i = 0; i < 20; i++)
    {
        if (i < visualInfo.songNameLength)
        {
            m_Stream.put(tolower(visualInfo.songName[i]));
        }
        else
        {
            m_Stream.put(0);
        }
    }

    ///////////////// CONVERT SAMPLE INFO

    if (!silent)
        std::cout << "Converting sample info...\n";

    m_DMFTotalWavetables = dmf.GetTotalWavetables();

    if (InitSamples(dmf))
    {
        // An error occurred in initSamples
        m_LowestNotes.clear();
        m_HighestNotes.clear();
        m_NoteRangeStart.clear();
        m_SampleMap.clear();
        m_SampleLength.clear();
        return true;
    }
    m_LowestNotes.clear();
    m_HighestNotes.clear();
    
    /* // For testing
    for (int i = 0; i < 8 + dmf->totalWavetables * 2; i++) 
    {
        printf("sampMap[%i] = %i. noteRangeStart[n] = %i, %i.\n", i, sampMap[i], noteRangeStart[i].pitch, noteRangeStart[i].octave); 
    }
    */

    // The remaining samples are blank:
    for (int i = m_TotalMODSamples; i < 31; i++)
    {
        // According to real ProTracker files viewed in a hex viewer, the 30th and final byte
        //    of a blank sample is 0x01 and all 29 other bytes are 0x00.
        for (int j = 0; j < 29; j++)
        {
            m_Stream.put(0);
        }
        m_Stream.put(1);
    }

    ///////////////// EXPORT OTHER INFO

    if (!silent)
        std::cout << "Converting other information...\n";

    m_Stream.put(moduleInfo.totalRowsInPatternMatrix + (int)m_UsingSetupPattern);   // Song length in patterns (not total number of patterns) 
    m_Stream.put(127);                        // 0x7F - Useless byte that has to be here

    // Pattern matrix (Each ProTracker pattern number is the same as its pattern matrix row number)
    for (uint8_t i = 0; i < moduleInfo.totalRowsInPatternMatrix + (int)m_UsingSetupPattern; i++)
    {
        m_Stream.put(i);
    }
    for (uint8_t i = moduleInfo.totalRowsInPatternMatrix + (int)m_UsingSetupPattern; i < 128; i++)
    {
        m_Stream.put(0);
    }
    
    m_Stream << "M.K."; // ProTracker uses "M!K!" if there's more than 64 pattern matrix rows...

    ///////////////// CONVERT PATTERN DATA

    if (!silent)
        std::cout << "Converting pattern data...\n";

    if (m_UsingSetupPattern)
    {
        // Export initial tempo (An approximation. ProTracker can only support Deflemask tempo between 16 and 127.5 bpm.)
        uint16_t effect = ((uint16_t)PT_SETSPEED << 4) | GetPTTempo(dmf.GetBPM());
        m_Stream.put(0);  // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        m_Stream.put(0);  // Sample period/effect param. (lower 8 bits)
        m_Stream.put((effect & 0x0F00) >> 8); // Sample number (lower 4b); effect code (upper 4b)
        m_Stream.put(effect & 0x00FF); // Effect code (lower 8 bits)

        // Export position jump (once the Pattern Break effect is implemented, it can be used instead.)
        effect = ((uint16_t)PT_POSJUMP << 4) | 1; // Jump to next pattern
        m_Stream.put(0);  // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        m_Stream.put(0);  // Sample period/effect param. (lower 8 bits)
        m_Stream.put((effect & 0x0F00) >> 8); // Sample number (lower 4b); effect code (upper 4b)
        m_Stream.put(effect & 0x00FF); // Effect code (lower 8 bits)

        // Blank (WAVE channel)
        m_Stream.put(0); // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        m_Stream.put(0); // Sample period/effect param. (lower 8 bits)
        m_Stream.put(0); // Sample number (lower 4b); effect code (upper 4b)
        m_Stream.put(0); // Effect code (lower 8 bits)

        // Blank (NOISE channel)
        m_Stream.put(0); // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        m_Stream.put(0); // Sample period/effect param. (lower 8 bits)
        m_Stream.put(0); // Sample number (lower 4b); effect code (upper 4b)
        m_Stream.put(0); // Effect code (lower 8 bits)
        
        // Loop through the rest of the rows in 1st pattern:
        for (int patRow = 1; patRow < 64; patRow++)
        {
            // Loop through channels:
            for (int chan = 0; chan < DMF::SYSTEMS(SYS_GAMEBOY).channels; chan++)
            {
                m_Stream.put(0); // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
                m_Stream.put(0); // Sample period/effect param. (lower 8 bits) 
                m_Stream.put(0); // Sample number (lower 4b); effect code (upper 4b)
                m_Stream.put(0); // Effect code (lower 8 bits)
            }
        }
    }
    
    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.
    MODChannelState state[DMF::SYSTEMS(SYS_GAMEBOY).channels], stateJumpCopy[DMF::SYSTEMS(SYS_GAMEBOY).channels];
    for (int i = 0; i < DMF::SYSTEMS(SYS_GAMEBOY).channels; i++)
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

    // Loop through ProTracker pattern matrix rows (corresponds to pattern numbers):
    for (int patMatRow = 0; patMatRow < moduleInfo.totalRowsInPatternMatrix; patMatRow++)
    {
        // Loop through rows in a pattern:
        for (int patRow = 0; patRow < 64; patRow++)
        {
            // Loop through channels:
            for (int chan = 0; chan < DMF::SYSTEMS(SYS_GAMEBOY).channels; chan++)
            {
                pat = patternValues[chan][patternMatrixValues[chan][patMatRow]][patRow];
                effectCode = pat.effectCode[0];
                effectValue = pat.effectValue[0];

                // If just arrived at jump destination:
                if (patMatRow == jumpDestination && patRow == 0 && stateSuspended)
                {
                    // Restore state copies
                    for (int v = 0; v < DMF::SYSTEMS(SYS_GAMEBOY).channels; v++)
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
                        for (int v = 0; v < DMF::SYSTEMS(SYS_GAMEBOY).channels; v++)
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

                if (WriteProTrackerPatternRow(dmf, pat, state[chan]))
                {
                    // Error occurred while writing the pattern row
                    m_NoteRangeStart.clear();
                    m_SampleMap.clear();
                    m_SampleLength.clear();
                    return true;
                }
            }
        }
    }

    ///////////////// CONVERT SAMPLE DATA

    if (!silent)
        std::cout << "Converting samples...\n";
    ExportSampleData(dmf);
    
    ///////////////// CLEAN UP

    m_NoteRangeStart.clear();
    m_SampleMap.clear();
    m_SampleLength.clear();

    if (!silent)
        std::cout << "Done converting to MOD.\n\n";

    // Success
    return false;
}

int MOD::WriteProTrackerPatternRow(const DMF& dmf, const PatternRow& pat, MODChannelState& state)
{
    // Writes 4 bytes of pattern row information to the .mod file
    uint16_t effect;
    if (CheckEffects(pat, state, effect))
    {
        return 1; // An error occurred
    }

    if (pat.note.pitch == DMF_NOTE_EMPTY)  // No note is playing. Only handle effects.
    {
        m_Stream.put(0); // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        m_Stream.put(0); // Sample period/effect param. (lower 8 bits)
        m_Stream.put((effect & 0x0F00) >> 8);  // Sample number (lower 4b) = 0 b/c there's no note; effect code (upper 4b)
        m_Stream.put(effect & 0x00FF);         // Effect code (lower 8 bits)
    }
    else if (pat.note.pitch == DMF_NOTE_OFF) // Note OFF. Only handle note OFF effect.
    {
        state.notePlaying = false;

        //uint16_t period = proTrackerPeriodTable[2][DMF_NOTE_C % 12]; // This note won't be played, but it makes ProTracker happy.
        //fout.put((period & 0x0F00) >> 8);  // Sample number (upper 4b); sample period/effect param. (upper 4b)
        //fout.put(period & 0x00FF);         // Sample period/effect param. (lower 8 bits)
        m_Stream.put(0);                         // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        m_Stream.put(0);                         // Sample period/effect param. (lower 8 bits)
        m_Stream.put((effect & 0x0F00) >> 8);    // Sample number (lower 4b); effect code (upper 4b)
        m_Stream.put(effect & 0x00FF);           // Effect code (lower 8 bits)
    }
    else  // A note is playing
    {
        uint8_t indexLow, indexHigh = 0;
        uint16_t period = 0;

        uint16_t modOctave = 0; // The note's octave to export to the MOD file.

        if (pat.note.pitch >= 1 && pat.note.pitch <= 12) // A note
        {
            if (state.channel != DMF_GAMEBOY_NOISE)
            {
                // The indices for this SQW / WAVE sample's low note range and high note range:
                indexLow = state.channel == DMF_GAMEBOY_WAVE ? state.wavetable + 4 : state.dutyCycle;
                indexHigh = indexLow + 4 + m_DMFTotalWavetables;
                
                // If using high note range and the current note is in the high note range:
                if (m_NoteRangeStart[indexHigh].pitch != 0 && NoteCompare(pat.note, m_NoteRangeStart[indexHigh]) >= 0)
                {
                    // Find the note's octave in ProTracker:
                    modOctave = pat.note.octave - m_NoteRangeStart[indexHigh].octave;
                    if (!state.onHighNoteRange)
                    {
                        // Switching to a different note range requires the use of a different ProTracker sample.
                        state.sampleChanged = true;
                    }
                    state.onHighNoteRange = true;
                }
                else // Using the low note range
                {
                    // Find the note's octave in ProTracker:
                    modOctave = pat.note.octave - m_NoteRangeStart[indexLow].octave;
                    if (state.onHighNoteRange)
                    {
                        // Switching to a different note range requires the use of a different ProTracker sample. 
                        state.sampleChanged = true;
                    }
                    state.onHighNoteRange = false;
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
        else if (state.channel != DMF_GAMEBOY_NOISE) // A ProTracker sample needs to change
        {
            sampleNumber = state.onHighNoteRange ? m_SampleMap[indexLow + 4 + m_DMFTotalWavetables] : m_SampleMap[indexLow]; // Get new PT sample number
            state.sampleChanged = false; // Just changed the sample, so resetting this for next time.
            if (effect == PT_NOEFFECT_CODE)
            {
                // When you change PT samples, the channel volume resets, so 
                //  if there are still no effects are being used on this pattern row, 
                //  use a volume change effect to set the volume to where it needs to be.
                uint8_t newVolume = std::round(state.volume / (double)DMF_NOTE_VOLUMEMAX * (double)PT_NOTE_VOLUMEMAX); // Convert DMF volume to PT volume
                effect = ((uint16_t)PT_SETVOLUME << 4u) + newVolume;
                // NOTE: The WASM version does not truncate 16 bit values to 8 bit even if you 
                //  store it in a uint8_t type or mask it with (sixteen & 0x00FFu).
            }
        }
        else // Noise channel
        {
            sampleNumber = 0; // Keep noise channel quiet since it isn't implemented yet.
        }

        m_Stream.put((sampleNumber & 0xF0) | ((period & 0x0F00) >> 8));  // Sample number (upper 4b); sample period/effect param. (upper 4b)
        m_Stream.put(period & 0x00FF);                                   // Sample period/effect param. (lower 8 bits)
        m_Stream.put((sampleNumber << 4) | ((effect & 0x0F00) >> 8));    // Sample number (lower 4b); effect code (upper 4b)
        m_Stream.put(effect & 0x00FF);                                   // Effect code (lower 8 bits)
        
        state.notePlaying = true;
    }

   return 0; // Success
}

int MOD::CheckEffects(const PatternRow& pat, MODChannelState& state, uint16_t& effect)
{
    if (m_Options->GetEffects() == MODConversionOptions::EffectsEnum::Max) // If using maximum amount of effects
    {
        effect = GetProTrackerEffect(pat.effectCode[0], pat.effectValue[0]); // Effects must be in first row
        if (pat.volume != state.volume && pat.volume != DMF_NOTE_NOVOLUME && (pat.note.pitch >= 1 && pat.note.pitch <= 12)) // If the note volume changes, and the volume is connected to a note
        {
            if (effect != PT_NOEFFECT_CODE) // If an effect is already being used
            {
                /* Unlike Deflemask, setting the volume in ProTracker requires the use of 
                    an effect, and only one effect can be used at a time per channel.
                    Same with turning a note off, which requires the EC0 command as far as I know.
                    Note that the set duty cycle effect in Deflemask is not implemented as an effect in PT, 
                    so it does not count.
                */
                m_Status.SetError(Status::Category::Convert, MOD::ConvertError::EffectVolume);
                return 1;
            }
            else // Only the volume changed
            {
                uint8_t newVolume = std::round(pat.volume / (double)DMF_NOTE_VOLUMEMAX * (double)PT_NOTE_VOLUMEMAX); // Convert DMF volume to PT volume
                effect = ((uint16_t)PT_SETVOLUME << 4) + newVolume; // ???
                state.volume = pat.volume; // Update the state
            }
        } 

        if (pat.note.pitch == DMF_NOTE_OFF && state.notePlaying) // If the note needs to be turned off
        {
            if (effect != PT_NOEFFECT_CODE) // If an effect is already being used
            {
                /* Unlike Deflemask, setting the volume in ProTracker requires the use of 
                    an effect, and only one effect can be used at a time per channel.
                    Same with turning a note off, which requires the EC0 command as far as I know.
                    Note that the set duty cycle effect in Deflemask is not implemented as an effect in PT, 
                    so it does not count.
                */
                m_Status.SetError(Status::Category::Convert, MOD::ConvertError::EffectVolume);
                return 1;
            }
            else // No effects except the note OFF
            {
                effect = (uint16_t)PT_CUTSAMPLE << 4; // Cut sample effect with value 0.
            }
        }
    }
    else // Don't use effects (except for Volume change, Note OFF, or Position Jump)
    {
        uint8_t total_effects = 0;
        int16_t volumeCopy = state.volume; // Save copy in case the volume change is canceled later 
        if (pat.volume != state.volume && pat.volume != DMF_NOTE_NOVOLUME && (pat.note.pitch >= 1 && pat.note.pitch <= 12)) // If the volume changed, we still want to handle that. Volume must be connected to a note.  
        {
            uint8_t newVolume = round(pat.volume / (double)DMF_NOTE_VOLUMEMAX * (double)PT_NOTE_VOLUMEMAX); // Convert DMF volume to PT volume
            effect = ((uint16_t)PT_SETVOLUME << 4) + newVolume; // ???
            state.volume = pat.volume; // Update the state
            total_effects++;
        }
        if (pat.note.pitch == DMF_NOTE_OFF && state.notePlaying) // If the note needs to be turned off
        {
            effect = (uint16_t)PT_CUTSAMPLE << 4; // Cut sample effect with value 0.
            state.volume = volumeCopy; // Cancel volume change if it occurred above.
            total_effects++;
        }
        if (pat.effectCode[0] == DMF_POSJUMP) // Position Jump. Has priority over volume changes and note cuts.
        {
            effect = ((uint16_t)PT_POSJUMP << 4) | (pat.effectValue[0] + (int)m_UsingSetupPattern); // Effects must be in first row
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
            return 1;
        }
        else if (total_effects == 0)
        {
            effect = PT_NOEFFECT_CODE; // No effect
        }
    }
    return 0; // Success
}

uint16_t MOD::GetProTrackerEffect(int16_t effectCode, int16_t effectValue)
{
    // An effect is represented with 12 bits, which is 3 groups of 4 bits: [e][x][y].
    // The effect code is [e] or [e][x], and the effect value is [x][y] or [y].
    // Effect codes of the form [e] are stored as [e][0x0].
    uint8_t ptEff = PT_NOEFFECT;
    uint8_t ptEffVal = PT_NOEFFECTVAL;

    switch (effectCode)
    {
        case DMF_NOEFFECT:
            ptEff = PT_NOEFFECT; break; // ?
        case DMF_ARP:
            ptEff = PT_ARP;
            ptEffVal = effectValue;
            break;
        case DMF_PORTUP:
            ptEff = PT_PORTUP;
            ptEffVal = effectValue;
            break;
        case DMF_PORTDOWN:
            ptEff = PT_PORTDOWN;
            ptEffVal = effectValue;
            break;
        case DMF_PORT2NOTE:
            ptEff = PT_PORT2NOTE; 
            ptEffVal = effectValue; //CLAMP(effectValue + 6, 0x00, 0xFF); // ???
            break;
        case DMF_VIBRATO:
            ptEff = PT_VIBRATO; break;
        case DMF_PORT2NOTEVOLSLIDE:
            ptEff = PT_PORT2NOTEVOLSLIDE; break;
        case DMF_VIBRATOVOLSLIDE:
            ptEff = PT_VIBRATOVOLSLIDE; break;
        case DMF_TREMOLO:
            ptEff = PT_TREMOLO; break;
        case DMF_PANNING:
            ptEff = PT_PANNING; break;
        case DMF_SETSPEEDVAL1:
            break; // ?
        case DMF_VOLSLIDE:
            ptEff = PT_VOLSLIDE; break;
        case DMF_POSJUMP:
            ptEff = PT_POSJUMP;
            ptEffVal = effectValue + (int)m_UsingSetupPattern;
            break;
        case DMF_RETRIG:
            ptEff = PT_RETRIGGERSAMPLE; break;  // ?
        case DMF_PATBREAK:
            ptEff = PT_PATBREAK; break;
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
            ptEff = PT_CUTSAMPLE;
            ptEffVal = effectValue; // ??
            break;
        case DMF_NOTEDELAY:
            ptEff = PT_DELAYSAMPLE; break; // ?
        case DMF_SYNCSIGNAL:
            break; // This is only used when exporting as VGM in Deflemask
        case DMF_SETGLOBALFINETUNE:
            ptEff = PT_SETFINETUNE; break; // ?
        case DMF_SETSPEEDVAL2:
            break; // ?

        // Game Boy exclusive:
        case DMF_SETWAVE:
            break; // This is handled in the ConvertFrom and WriteProTrackerPatternRow methods
        case DMF_SETNOISEPOLYCOUNTERMODE:
            break; // This is probably more than I need to worry about
        case DMF_SETDUTYCYCLE:
            break; // This is handled in the ConvertFrom and WriteProTrackerPatternRow methods
        case DMF_SETSWEEPTIMESHIFT:
            break; // ?
        case DMF_SETSWEEPDIR:
            break; // ?

    }

    return ((uint16_t)ptEff << 4) | ptEffVal;
}

mod_sample_id_t MOD::GetMODSampleId(dmf_sample_id_t dmfSampleId, const Note& dmfNote)
{
    // Must call CreateSampleMapping first.

    if (m_SampleMap2.count(dmfSampleId) > 0)
    {
        MODSplitSample& splitSample = m_SampleMap2[dmfSampleId];

        // If there's no split point, there's no high/low versions, just one version stored in lowId
        if (splitSample.splitPoint.octave == 0 && splitSample.splitPoint.pitch == 0)
            return splitSample.lowId;

        // Use DMF note to check which MOD sample ID to use based on split point
        if (dmfNote >= splitSample.splitPoint)
            return splitSample.highId;
        else
            return splitSample.lowId;
    }
    return -1; // Sample mapping for this DMF sample does not exist. Does not need to be imported.
}


bool MOD::CreateSampleMapping(const DMF& dmf)
{
    // This function loops through all DMF pattern contents to find the highest and lowest notes 
    //  for each square wave duty cycle and each wavetable. It also finds which SQW duty cycles are 
    //  used and which wavetables are used and stores this info in m_SampleMap.
    //  Then it calls the function finalizeSampMap.

    m_SampleMap2.clear();
    m_SampleIdLowestHighestNotesMap.clear();

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.
    MODChannelState state[DMF::SYSTEMS(SYS_GAMEBOY).channels], stateJumpCopy[DMF::SYSTEMS(SYS_GAMEBOY).channels];
    for (int i = 0; i < DMF::SYSTEMS(SYS_GAMEBOY).channels; i++)
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
    
    // Most of the following nested for loop is copied from the export pattern data loop in ConvertFrom.
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
                    for (int v = 0; v < DMF::SYSTEMS(SYS_GAMEBOY).channels; v++)
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
                        for (int v = 0; v < DMF::SYSTEMS(SYS_GAMEBOY).channels; v++)
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

                // A note on the SQ1, SQ2, or WAVE channels:
                if (pat.note.pitch >= 1 && pat.note.pitch <= 12 && chan != DMF_GAMEBOY_NOISE)
                {
                    mod_sample_id_t sampleId = chan == DMF_GAMEBOY_WAVE ? state[chan].wavetable + 4 : state[chan].dutyCycle;

                    // Mark this square wave or wavetable as used
                    m_SampleMap2[sampleId] = {}; // TODO: Is this all zeroed out?

                    // Get lowest/highest notes
                    if (m_SampleIdLowestHighestNotesMap.count(sampleId) == 0) // 1st time
                    {
                        m_SampleIdLowestHighestNotesMap[sampleId] = std::pair<Note, Note>(pat.note, pat.note);
                    }
                    else
                    {
                        auto& notePair = m_SampleIdLowestHighestNotesMap[sampleId];
                        if (pat.note > notePair.second)
                        {
                            // Found a new highest note
                            notePair.second = pat.note;
                        }
                        if (pat.note < notePair.first)
                        {
                            // Found a new lowest note
                            notePair.first = pat.note;
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
    //  ProTracker (PT) sample numbers

    int8_t finetune = 0; // Not really used, at least for now
    uint8_t ptSampleNum = 1; // PT sample #0 is special.
    
    mod_sample_id_t currentMODSampleId = 1; // Sample #0 is special in ProTracker

    // Only the samples we need will be in this map
    for (auto& mapPair : m_SampleMap2)
    {
        dmf_sample_id_t sampleId = mapPair.first;
        MODSplitSample& splitSample = mapPair.second;

        auto& lowHighNotes = m_SampleIdLowestHighestNotesMap[sampleId];
        auto& lowestNote = lowHighNotes.first;
        auto& highestNote = lowHighNotes.second;

        bool noSplittingIsPossible = false;

        // If the range is 3 octaves or less:
        if (highestNote.octave*12 + highestNote.pitch - lowestNote.octave*12 - lowestNote.pitch <= 36)
        {
            // (Note: The note C-n in the Deflemask tracker is called C-(n-1) in DMF format because the 1st note of an octave is C# in DMF files.)
            
            finetune = 0;

            Note lowNoteToCompare = { DMF_NOTE_C, 1 };
            Note highNoteToCompare = { DMF_NOTE_B, 4 };

            if (lowestNote >= lowNoteToCompare && highNoteToCompare <= highNoteToCompare)
            {
                noSplittingIsPossible = true;

                // If between C-2 and B-4 (Deflemask tracker note format)
                splitSample.splitPoint = lowNoteToCompare;
                // Use sample length: 64 (Double length)
            }

            lowNoteToCompare = { DMF_NOTE_C, 2 };
            highNoteToCompare = { DMF_NOTE_B, 5 };

            if (lowestNote >= lowNoteToCompare && highNoteToCompare <= highNoteToCompare)
            {
                noSplittingIsPossible = true;

                // If between C-3 and B-5 (Deflemask tracker note format)
                splitSample.splitPoint = lowNoteToCompare;
                // Use sample length: 32 (regular wavetable length)
            }
            else if (lowestNote >= (Note){DMF_NOTE_C, 3} && highestNote <= (Note){DMF_NOTE_B, 6})
            {
                // If between C-4 and B-6 (Deflemask tracker note format) and none of the above options work
                if (sampleId >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                    return true;
                }

                noSplittingIsPossible = true;

                splitSample.splitPoint = { DMF_NOTE_C, 3 };
                // Use sample length: 16 (half length - downsampling needed)
            }
            else if (lowestNote >= (Note){DMF_NOTE_C, 4} && highestNotes <= (Note){DMF_NOTE_B, 7})
            {
                // If between C-5 and B-7 (Deflemask tracker note format)
                if (sampleId >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                    return true;
                }
                
                noSplittingIsPossible = true;

                splitSample.splitPoint = { DMF_NOTE_C, 4 };
                // Use sample length: 8 (1/4 length - downsampling needed)
            }
            else if (highestNote == (Note){DMF_NOTE_C, 7})
            {
                // If between C#5 and C-8 (highest note) (Deflemask tracker note format):
                if (sampleId >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(i - 4));
                    return true;
                }
                
                // TODO: This note is currently unsupported. Should fail here.

                noSplittingIsPossible = true;
                finetune = 0; // One semitone up from B = C- ??? was 7

                splitSample.splitPoint = { DMF_NOTE_C, 4 };
                // Use sample length: 8 (1/4 length - downsampling needed)
            }

            if (noSplittingIsPossible) // If one of the above options worked
            {
                if (sampleId >= 4) // If on a wavetable instrument
                {
                    // Double wavetable sample length.
                    // This will lower all wavetable instruments by one octave, which should make it
                    // match the octave for wavetable instruments in Deflemask
                    
                    // TODO:
                    ///m_SampleLength[indexLow] *= 2;
                }

                splitSample.lowId = currentMODSampleId; // Assign PT sample number to this square wave / WAVE sample
                splitSample.highId = -1; // No PT sample needed for this square wave / WAVE sample (high note range)
                // TODO: Was exporting sample info here
                currentMODSampleId++;
            }
        }

        // If none of the options above worked, both high note range and low note range are needed.
        if (!noSplittingIsPossible) // If splitting is necessary
        {
            // Use sample length: 64 (Double length) for low range (C-2 to B-4)
            // Use sample length: 8 (Quarter length) for high range (C-5 to B-7)

            // If on a wavetable sample
            if (sampleId >= 4)
            {
                // If it cannot be downsampled (square waves can always be downsampled w/o permission):
                if (!m_Options->GetDownsample())
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(sampleId - 4));
                    return true;
                }

                // !!!! 11/9/2021: This code has never been used before due to a logic issue preventing it from
                //                  being reached. It will need testing. I'm commenting it out for now to keep
                //                  previous behavior:

                // Double wavetable sample length.
                // This is lower all wavetable instruments by one octave, which should make it
                // match the octave for wavetable instruments in Deflemask
                ///m_SampleLength[indexLow] *= 2;
                ///m_SampleLength[indexHigh] *= 2;
            }

            splitSample.splitPoint = {DMF_NOTE_C, 4};

            // TODO: Previously stored two note range values instead of a single "split point". Will this work?
            //m_NoteRangeStart[indexLow].octave = 1;
            //m_NoteRangeStart[indexLow].pitch = DMF_NOTE_C;
            //m_NoteRangeStart[indexHigh].octave = 4;
            //m_NoteRangeStart[indexHigh].pitch = DMF_NOTE_C;
            finetune = 0;

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

            splitSample.lowId = currentMODSampleId++; // Assign PT sample number to this square wave / WAVE sample
            splitSample.highId = currentMODSampleId++; // Assign PT sample number to this square wave / WAVE sample
            // Called ExportSampleInfo here
        }
    }

    m_TotalMODSamples = currentMODSampleId - 1; // Set the number of PT samples that will be needed. (minus sample #0 which is special)
    return 0; // Success
}






// OLD METHOD; CAN REMOVE WHEN READY. Replaced with CreateSampleMapping
int MOD::InitSamples(const DMF& dmf)
{
    // This function loops through all DMF pattern contents to find the highest and lowest notes 
    //  for each square wave duty cycle and each wavetable. It also finds which SQW duty cycles are 
    //  used and which wavetables are used and stores this info in m_SampleMap.
    //  Then it calls the function finalizeSampMap.

    // See declaration of m_SampleMap for more information.
    m_SampleMap.assign(8 + m_DMFTotalWavetables * 2, 0);
    m_SampleLength.assign(8 + m_DMFTotalWavetables * 2, 0);

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.
    MODChannelState state[DMF::SYSTEMS(SYS_GAMEBOY).channels], stateJumpCopy[DMF::SYSTEMS(SYS_GAMEBOY).channels];
    for (int i = 0; i < DMF::SYSTEMS(SYS_GAMEBOY).channels; i++)
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

    // For calculating min/max, start with impossible notes which won't change if there are 
    //  no notes for a SQW / WAVE sample:
    m_LowestNotes.assign(4 + m_DMFTotalWavetables, { DMF_NOTE_C, 10 }); // C-10
    m_HighestNotes.assign(4 + m_DMFTotalWavetables, { DMF_NOTE_C, 0 }); // C-0
    
    // Initialize noteRangeStart - low and high note ranges:
    m_NoteRangeStart.assign(8 + m_DMFTotalWavetables * 2, { 0, 0 });

    // The main MODChannelState structs should NOT update during patterns or parts of 
    //   patterns that the Position Jump (Bxx) effect skips over. (Ignore loops)
    //   Keep a copy of the main state for each channel, and once it reaches the 
    //   jump destination, overwrite the current state with the copied state.
    bool stateSuspended = false; // true == currently in part that a Position Jump skips over
    int8_t jumpDestination = -1; // Pattern matrix row where you are jumping to. Not a loop.

    int16_t effectCode, effectValue;
    uint8_t indexLow = 0;

    const ModuleInfo& moduleInfo = dmf.GetModuleInfo();
    PatternRow*** const patternValues = dmf.GetPatternValues();
    uint8_t** const patternMatrixValues = dmf.GetPatternMatrixValues();
    
    // Most of the following nested for loop is copied from the export pattern data loop in ConvertFrom.
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
                    for (int v = 0; v < DMF::SYSTEMS(SYS_GAMEBOY).channels; v++)
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
                        for (int v = 0; v < DMF::SYSTEMS(SYS_GAMEBOY).channels; v++)
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

                // A note on the SQ1, SQ2, or WAVE channels:
                if (pat.note.pitch >= 1 && pat.note.pitch <= 12 && chan != DMF_GAMEBOY_NOISE)
                {
                    if (chan == DMF_GAMEBOY_WAVE)
                    {
                        indexLow = state[chan].wavetable + 4;
                    }
                    else 
                    {
                        indexLow = state[chan].dutyCycle;
                    }
                    m_SampleMap[indexLow] = 1; // Mark this square wave or wavetable as used - low note range
                    
                    // I'm keeping the notes in the .dmf form where the 1st note of an octave is C# and not C-. I can convert it later.
                    if (NoteCompare(pat.note, m_HighestNotes[indexLow]) == 1) // if pat.note > m_HighestNotes[indexLow]
                    {
                        // Found a new highest note
                        m_HighestNotes[indexLow].octave = pat.note.octave;
                        m_HighestNotes[indexLow].pitch = pat.note.pitch;
                    }
                    if (NoteCompare(pat.note, m_LowestNotes[indexLow]) == -1) // if pat.note < m_LowestNotes[indexLow]
                    {
                        // Found a new lowest note 
                        m_LowestNotes[indexLow].octave = pat.note.octave;
                        m_LowestNotes[indexLow].pitch = pat.note.pitch;
                    }
                }
            }
        }
    }

    return FinalizeSampMap(dmf);
}

// OLD METHOD; CAN REMOVE WHEN READY. Replaced with SampleSplittingAndAssignment
int MOD::FinalizeSampMap(const DMF& dmf)
{
    // This function assigns ProTracker (PT) sample numbers and exports sample info

    int8_t finetune = 0; // Not really used, at least for now
    uint8_t ptSampleNum = 1; // PT sample #0 is special.
    uint8_t indexLow, indexHigh;

    // Loop through SQW (low note range) and WAVE (low note range)
    for (int i = 0; i < 4 + m_DMFTotalWavetables; i++)
    {
        indexLow = i;
        indexHigh = i + 4 + m_DMFTotalWavetables;

        // If this square wave duty cycle or wavetable is unused, skip to the next one:
        if (m_SampleMap[i] == 0)
        {
            m_SampleMap[indexLow] = -1; // No PT sample needed for this square wave / WAVE sample (low note range)
            m_SampleMap[indexHigh] = -1; // No PT sample needed for this square wave / WAVE sample (high note range)
            m_SampleLength[indexLow] = -1; // No PT sample needed for this square wave / WAVE sample (low note range)
            m_SampleLength[indexHigh] = -1; // No PT sample needed for this square wave / WAVE sample (high note range)
            continue;
        }

        // If the range is 3 octaves or less:
        if (m_HighestNotes[i].octave*12 + m_HighestNotes[i].pitch - m_LowestNotes[i].octave*12 - m_LowestNotes[i].pitch <= 36)
        {
            // (Note: The note C-n in the Deflemask tracker is called C-(n-1) in DMF format because the 1st note of an octave is C# in DMF files.)
            
            m_SampleLength[indexLow] = 0; // Should change if any of the options below work
            finetune = 0;

            if (NoteCompare(m_LowestNotes[i], {DMF_NOTE_C, 1}) >= 0 && NoteCompare(m_HighestNotes[i], {DMF_NOTE_B, 4}) <= 0) 
            {
                // If between C-2 and B-4 (Deflemask tracker note format)
                m_SampleLength[indexLow] = 64; // Double length
                m_NoteRangeStart[indexLow].octave = 1;
                m_NoteRangeStart[indexLow].pitch = DMF_NOTE_C;
            }
             
            if (NoteCompare(m_LowestNotes[i], {DMF_NOTE_C, 2}) >= 0 && NoteCompare(m_HighestNotes[i], {DMF_NOTE_B, 5}) <= 0) 
            {
                // If between C-3 and B-5 (Deflemask tracker note format)
                m_SampleLength[indexLow] = 32; // This is the default length anyway
                m_NoteRangeStart[indexLow].octave = 2;
                m_NoteRangeStart[indexLow].pitch = DMF_NOTE_C;
            }
            else if (NoteCompare(m_LowestNotes[i], {DMF_NOTE_C, 3}) >= 0 && NoteCompare(m_HighestNotes[i], {DMF_NOTE_B, 6}) <= 0) 
            {
                // If between C-4 and B-6 (Deflemask tracker note format) and none of the above options work 
                if (i >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it 
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(i - 4));
                    return 1;
                }
                
                m_SampleLength[indexLow] = 16;
                m_NoteRangeStart[indexLow].octave = 3;
                m_NoteRangeStart[indexLow].pitch = DMF_NOTE_C;
                
            }
            else if (NoteCompare(m_LowestNotes[i], {DMF_NOTE_C, 4}) >= 0 && NoteCompare(m_HighestNotes[i], {DMF_NOTE_B, 7}) <= 0) 
            {
                // If between C-5 and B-7 (Deflemask tracker note format)
                if (i >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(i - 4));
                    return 1;
                }
                
                m_SampleLength[indexLow] = 8;
                m_NoteRangeStart[indexLow].octave = 4;
                m_NoteRangeStart[indexLow].pitch = DMF_NOTE_C;
            }
            else if (NoteCompare(m_HighestNotes[i], {DMF_NOTE_C, 7}) == 0)
            {
                // If between C#5 and C-8 (highest note) (Deflemask tracker note format):
                if (i >= 4 && !m_Options->GetDownsample()) // If on a wavetable instrument and can't downsample it
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(i - 4));
                    return 1;
                }
                
                finetune = 0; // One semitone up from B = C- ??? was 7
                m_SampleLength[indexLow] = 8;
                m_NoteRangeStart[indexLow].octave = 4;
                m_NoteRangeStart[indexLow].pitch = DMF_NOTE_C;
            }

            if (m_SampleLength[i] != 0) // If one of the above options worked
            {
                if (i >= 4) // If on a wavetable instrument
                {
                    // Double wavetable sample length.
                    // This is lower all wavetable instruments by one octave, which should make it
                    // match the octave for wavetable instruments in Deflemask
                    m_SampleLength[indexLow] *= 2;
                } 
                m_SampleMap[indexLow] = ptSampleNum; // Assign PT sample number to this square wave / WAVE sample
                m_SampleMap[indexHigh] = -1; // No PT sample needed for this square wave / WAVE sample (high note range)
                ExportSampleInfo(dmf, ptSampleNum, -1, indexLow, indexHigh, finetune);
                ptSampleNum++;
            }
        }
        
        // If none of the options above worked, both high note range and low note range are needed.
        if (m_SampleLength[i] == 0)
        {
            m_SampleLength[indexLow] = 64; // Low note range (C-2 to B-4)
            m_SampleLength[indexHigh] = 8; // High note range (C-5 to B-7)

            // If on a wavetable sample
            if (i >= 4)
            {
                // If it cannot be downsampled (square waves can always be downsampled w/o permission):
                if (!m_Options->GetDownsample())
                {
                    m_Status.SetError(Status::Category::Convert, MOD::ConvertError::WaveDownsample, std::to_string(i - 4));
                    return 1;
                }

                // !!!! 11/9/2021: This code has never been used before due to a logic issue preventing it from
                //                  being reached. It will need testing. I'm commenting it out for now to keep
                //                  previous behavior:

                // Double wavetable sample length.
                // This is lower all wavetable instruments by one octave, which should make it
                // match the octave for wavetable instruments in Deflemask
                ///m_SampleLength[indexLow] *= 2;
                ///m_SampleLength[indexHigh] *= 2;
            }

            m_NoteRangeStart[indexLow].octave = 1;
            m_NoteRangeStart[indexLow].pitch = DMF_NOTE_C;
            m_NoteRangeStart[indexHigh].octave = 4;
            m_NoteRangeStart[indexHigh].pitch = DMF_NOTE_C;
            finetune = 0;

            if (NoteCompare(m_HighestNotes[i], {DMF_NOTE_C, 7}) == 0)
            {
                m_Status.AddWarning(GetWarningMessage(MOD::ConvertWarning::PitchHigh));
            }

            /*
            // If lowest possible note is needed:
            if (NoteCompare(m_LowestNotes[i], {DMF_NOTE_C, 1}) == 0)
            {
                // If highest and lowest possible notes are both needed:
                if (NoteCompare(m_HighestNotes[i], {DMF_NOTE_C, 7}) == 0)
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

            m_SampleMap[indexLow] = ptSampleNum; // Assign PT sample number to this square wave / WAVE sample
            m_SampleMap[indexHigh] = ptSampleNum + 1; // Assign PT sample number to this square wave / WAVE sample
            ExportSampleInfo(dmf, ptSampleNum, ptSampleNum + 1, indexLow, indexHigh, finetune);
            ptSampleNum += 2;

        }
    }

    m_TotalMODSamples = ptSampleNum - 1; // Set the number of PT samples that will be needed. (minus sample #0 which is special)
    return 0; // Success
}



void MOD::ExportSampleInfo(const DMF& dmf, int8_t ptSampleNumLow, int8_t ptSampleNumHigh, uint8_t indexLow, uint8_t indexHigh, int8_t finetune)
{
    uint8_t index;

    for (int i = 0; i < 2; i++) // 0 == low note range, 1 == high note range
    {
        if (i == 0) // Low note range
        {
            index = indexLow;
            // If there's also a high note range version of this sample, we want the "(low)" text:
            if (ptSampleNumHigh != -1)
            {
                if (indexLow <= 3) // SQW
                {
                    switch (indexLow)
                    {
                        case 0: m_Stream << "SQW, Duty 12.5% (low) "; break;
                        case 1: m_Stream << "SQW, Duty 25% (low)   "; break;
                        case 2: m_Stream << "SQW, Duty 50% (low)   "; break;
                        case 3: m_Stream << "SQW, Duty 75% (low)   "; break;
                    }
                }
                else  // WAVE
                {
                    m_Stream << "Wavetable #" << std::left << std::setw(2) << std::to_string(index - 4) << " (low)   ";
                }
            }
            else // Else, no "(low)" text.
            {
                if (indexLow <= 3) // SQW
                {
                    switch (indexLow)
                    {
                        case 0: m_Stream << "SQW, Duty 12.5%       "; break;
                        case 1: m_Stream << "SQW, Duty 25%         "; break;
                        case 2: m_Stream << "SQW, Duty 50%         "; break;
                        case 3: m_Stream << "SQW, Duty 75%         "; break;
                    }
                }
                else  // WAVE
                {
                    m_Stream << "Wavetable #" << std::left << std::setw(2) << std::to_string(index - 4) << "         ";
                }
            }
        }
        else if (ptSampleNumHigh != -1) // High note range, if available
        { 
            index = indexHigh;
            if (indexLow <= 3) // SQW
            {
                switch (indexLow)
                {
                    case 0: m_Stream << "SQW, Duty 12.5% (high)"; break;
                    case 1: m_Stream << "SQW, Duty 25% (high)  "; break;
                    case 2: m_Stream << "SQW, Duty 50% (high)  "; break;
                    case 3: m_Stream << "SQW, Duty 75% (high)  "; break;
                }
            }
            else  // WAVE
            {
                m_Stream << "Wavetable #" << std::left << std::setw(2) << std::to_string(index - 8 - m_DMFTotalWavetables) << " (high)  ";
            }
            
        }
        else // If high note range is not available, we're done.
        {
            break; 
        }

        m_Stream.put(m_SampleLength[index] >> 9);                 // Length byte 0
        m_Stream.put(m_SampleLength[index] >> 1);                 // Length byte 1
        m_Stream.put(finetune);                                 // Finetune value !!!
        m_Stream.put(PT_NOTE_VOLUMEMAX);                        // Sample volume
        m_Stream.put(0);                                        // Repeat offset byte 0
        m_Stream.put(0);                                        // Repeat offset byte 1
        m_Stream.put(m_SampleLength[index] >> 9);                 // Sample repeat length byte 0
        m_Stream.put((m_SampleLength[index] >> 1) & 0x00FF);      // Sample repeat length byte 1
    }
}

void MOD::ExportSampleData(const DMF& dmf)
{ 
    uint8_t ptSampleNum = 1;

    for (unsigned i = 0; i < 4u + m_DMFTotalWavetables; i++)
    {
        if (m_SampleMap[i] == ptSampleNum)
        {
            ExportSampleDataHelper(dmf, ptSampleNum, i);
            ptSampleNum++;
            if (m_SampleMap[i + 4 + m_DMFTotalWavetables] == ptSampleNum)
            {
                ExportSampleDataHelper(dmf, ptSampleNum, i + 4 + m_DMFTotalWavetables);
                ptSampleNum++;
            }
        }
    }
}

void MOD::ExportSampleDataHelper(const DMF& dmf, uint8_t ptSampleNum, uint8_t index)
{
    // This function must be called for SQW / WAVE samples in the same 
    //      order as their PT sample numbers. The ExportSampleData function guarantees it.

    // If it's a square wave sample
    if (index <= 3 || (index >= 4 + m_DMFTotalWavetables && index <= 7 + m_DMFTotalWavetables))
    {
        uint8_t duty[] = {1, 2, 4, 6};
        uint8_t dutyNum = index <= 3 ? index : index - 4 - m_DMFTotalWavetables;
        
        // This loop exports a square wave with the correct length and duty cycle:
        for (int i = 1; i <= m_SampleLength[index]; i++)
        {
            if ((i * 8.f) / m_SampleLength[index] <= (float)duty[dutyNum])
            {
                m_Stream.put(127); // high
            }
            else
            {
                m_Stream.put(-10); // low
            }
        }
    }
    else  // Wavetable sample
    {
        uint8_t waveNum = index <= 3 + m_DMFTotalWavetables ? index - 4 : index - 8 - m_DMFTotalWavetables;
        for (int i = 0; i < m_SampleLength[index]; i++)
        {
            // Note: For the Deflemask Game Boy system, all wavetable lengths are 32.
            if (m_SampleLength[index] == 128) // Quadruple length
            {
                // Convert from DMF sample values (0 to 15) to PT sample values (-128 to 127).
                m_Stream.put((int8_t)((dmf.GetWavetableValue(waveNum, i / 4) / 15.f * 255.f) - 128.f));
            }
            else if (m_SampleLength[index] == 64) // Double length
            {
                // Convert from DMF sample values (0 to 15) to PT sample values (-128 to 127).
                m_Stream.put((int8_t)((dmf.GetWavetableValue(waveNum, i / 2) / 15.f * 255.f) - 128.f));
            }
            else if (m_SampleLength[index] == 32) // Normal length 
            {
                // Convert from DMF sample values (0 to 15) to PT sample values (-128 to 127).
                m_Stream.put((int8_t)((dmf.GetWavetableValue(waveNum, i) / 15.f * 255.f) - 128.f));
            }
            else if (m_SampleLength[index] == 16) // Half length (loss of information)
            {
                // Take average of every two sample values to make new sample value
                int avg = (int8_t)((dmf.GetWavetableValue(waveNum, i * 2) / 15.f * 255.f) - 128.f);
                avg += (int8_t)((dmf.GetWavetableValue(waveNum, (i * 2) + 1) / 15.f * 255.f) - 128.f);
                avg /= 2;
                m_Stream.put((int8_t)avg);
            }
            else if (m_SampleLength[index] == 8) // Quarter length (loss of information)
            {
                // Take average of every four sample values to make new sample value
                int avg = (int8_t)((dmf.GetWavetableValue(waveNum, i * 4) / 15.f * 255.f) - 128.f);
                avg += (int8_t)((dmf.GetWavetableValue(waveNum, (i * 4) + 1) / 15.f * 255.f) - 128.f);
                avg += (int8_t)((dmf.GetWavetableValue(waveNum, (i * 4) + 2) / 15.f * 255.f) - 128.f);
                avg += (int8_t)((dmf.GetWavetableValue(waveNum, (i * 4) + 3) / 15.f * 255.f) - 128.f);
                avg /= 4;
                m_Stream.put((int8_t)avg);
            }
        }
    }
}

uint8_t MOD::GetPTTempo(double bpm)
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
