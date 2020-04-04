/*
mod.c
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Provides functions for exporting the contents of a .dmf file 
to ProTracker's .mod format. 

Several limitations apply in order to export. For example, the 
.dmf file must use the Game Boy system, patterns must have 64 
rows, only one effect column is allowed per channel, etc.  
*/

// TODO: Right now, only effect column 0 is used. But two dmf effects 
//      could potentially be used if one of them is 10xx or 12xx. Need to allow for that. 

// TODO: Add '--effects=NONE' option, which does not use any ProTracker effects. 

#include "mod.h"

extern const System Systems[10]; 

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

#define PT_NOTE_VOLUMEMAX 64
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

static void _exportMOD(char *fname, DMFContents *dmfContents, CMD_Options options); 

static int initSamples(FILE *fout, Note **lowestNote, Note **highestNote); 
static int finalizeSampMap(FILE *fout, Note *lowestNote, Note *highestNote);  
static void exportSampleInfo(FILE *fout, int8_t ptSampleNumLow, int8_t ptSampleNumHigh, uint8_t indexLow, uint8_t indexHigh, int8_t finetune);

static int writeProTrackerPatternRow(FILE *fout, PatternRow *pat, MODChannelState *state); 
static uint16_t getProTrackerEffect(int16_t effectCode, int16_t effectValue);
static int checkEffects(PatternRow *pat, MODChannelState *state, uint16_t *effect); 

static void exportSampleData(FILE *fout);
static void exportSampleDataHelper(FILE *fout, uint8_t ptSampleNum, uint8_t index); 

static uint8_t getPTTempo(double bpm); 
static char *itoa_p(int _Val, char *_DstBuf);

static void setError(MOD_ERROR error); 
static void setErrorMsg(MOD_ERROR error, char *msg); 
static void addWarning(MOD_WARNING warning); 

static CMD_Options opt; 
static DMFContents *dmf; 

/*
    sampMap gives the ProTracker (PT) sample numbers for a given SQW / WAVE sample of either low note range or high note range. 
    The index for a particular SQW / WAVE sample is specified using the format below: 
    For index 0 thru 3: SQW samples, 12.5% duty cycle thru 75% duty cycle (low note range) 
    For index 4 thru totalWavetables + 3: WAVE samples (low note range) 
    For index 4 + totalWavetables thru 7 + totalWavetables: SQW samples, 12.5% duty cycle thru 75% duty cycle (high note range) 
    For index 8 + totalWavetables thru 7 + totalWavetables * 2: WAVE samples (high note range)  
    The value of sampMap is -1 if a PT sample is not needed for the given SQW / WAVE sample. 
*/
static int8_t *sampMap; 

/*
    Specifies the point at which the note range starts for a given SQW / WAVE sample (high or low note range).
    A note range always contains 36 notes. I.e. C-2 thru B-4 (Deflemask tracker note format).
    Uses the same index format of sampMap minus the high note range indices.  
    If a certain SQW / WAVE sample is unused, then pitch = 0 and octave = 0. 
*/
static Note *noteRangeStart; 

/*
    Specifies the ProTracker sample length for a given SQW / WAVE sample.
    Uses the same index format of sampMap.  
    If a certain SQW / WAVE sample is unused, then pitch = 0 and octave = 0. 
    The value of sampleLength is -1 if a PT sample is not needed for the given SQW / WAVE sample.
*/
static int16_t *sampleLength; 

static int8_t totalPTSamples;

static uint16_t proTrackerPeriodTable[5][12]; 

static bool usingSetupPattern = true; // Whether to use a pattern at the start of the module to set up the initial tempo and other stuff. 

static char *filename = NULL; // The MOD file name. May not be the same as what the user gave dmf2mod.

static bool filePreviouslyExisted = true; 

static MODConversionStatus issues = (MODConversionStatus) {{MOD_ERROR_NONE, NULL}, {MOD_WARNING_NONE, MOD_WARNING_NONE}}; 

MODConversionStatus exportMOD(char *fname, DMFContents *dmfContents, CMD_Options options) 
{
    // TODO: This function will attempt to export to mod using several different combinations of 
    //      options to find the best option that works for the dmf file 
    _exportMOD(fname, dmfContents, options); 
    
    if (issues.error.errorCode != MOD_ERROR_NONE) 
    {
        printf("An error occurred during conversion:\n\n"); 
        printError(); 
        printf("\n");
    }

    if (issues.warnings.warningCode != MOD_WARNING_NONE) 
    {
        if (issues.error.errorCode != MOD_ERROR_NONE) 
        {
            printf("Additionally, these warnings occurred before the error:\n\n"); 
        }
        else
        {
            printf("Warnings occurred during conversion:\n\n"); 
        }
        printWarnings(); 
    }
    
    return issues; 
}

void _exportMOD(char *fname, DMFContents *dmfContents, CMD_Options options) 
{
    FILE *fout;
    dmf = dmfContents; // Allow any function in this file to access DMF contents w/o passing it as an argument.
    opt = options;     // Allow any function in this file to access CMD options w/o passing it as an argument.  

    ///////////////// EXPORT SONG NAME  

    #pragma region EXPORT SONG NAME 
    if (strcmp(getFilenameExt(fname), ".mod") != 0) 
    {
        filename = malloc((sizeof(fname) + 4) * sizeof(char)); 
        strcpy(filename, fname); 
        strcat(filename, ".mod"); 
        printf("Exporting to %s.\n", filename);
        if (access(filename, F_OK) == -1) // File doesn't currently exist 
        {
            filePreviouslyExisted = false; 
        }
        else
        {
            filePreviouslyExisted = true; 
        }
        
        fout = fopen(filename, "wb"); // Add ".mod" extension if it wasn't specified in the command-line argument 
    }
    else
    {
        filename = strdup(fname); 
        if (access(fname, F_OK) == -1) // File doesn't currently exist 
        {
            filePreviouslyExisted = false; 
        }
        else
        {
            filePreviouslyExisted = true; 
        }

        fout = fopen(fname, "wb");
        printf("Exporting to %s.\n", fname);
    }

    printf("Starting to export to .mod...\n");
    
    if (dmf->sys.id != Systems[SYS_GAMEBOY].id) // If it's not a Game Boy 
    {
        setError(MOD_ERROR_NOT_GAMEBOY);
        return;
    }

    if (dmf->moduleInfo.totalRowsInPatternMatrix + (int)usingSetupPattern > 64) // totalRowsInPatternMatrix is 1 more than it actually is 
    {
        setError(MOD_ERROR_TOO_MANY_PAT_MAT_ROWS);
        return;
    }

    if (dmf->moduleInfo.totalRowsPerPattern != 64) 
    { 
        setError(MOD_ERROR_NOT_64_ROW_PATTERN);
        return;
    }

    // Print module name, truncating or padding with zeros as needed
    for (int i = 0; i < 20; i++) 
    {
        if (i < dmf->visualInfo.songNameLength) 
        {
            fputc(tolower(dmf->visualInfo.songName[i]), fout);
        }
        else
        {
            fputc(0, fout);
        }
    }
    
    #pragma endregion 

    ///////////////// EXPORT SAMPLE INFO 

    #pragma region EXPORT_SAMPLE_INFO 
    printf("Exporting sample info...\n"); 

    Note *lowestNote, *highestNote; 
    if (initSamples(fout, &lowestNote, &highestNote)) 
    {
        // An error occurred in initSamples
        free(lowestNote); 
        free(highestNote);
        free(noteRangeStart); 
        free(sampMap); 
        free(sampleLength); 
        return; 
    }
    free(lowestNote); 
    free(highestNote); 
    
    /* // For testing 
    for (int i = 0; i < 8 + dmf->totalWavetables * 2; i++) 
    {
        printf("sampMap[%i] = %i. noteRangeStart[n] = %i, %i.\n", i, sampMap[i], noteRangeStart[i].pitch, noteRangeStart[i].octave); 
    }
    */

    // The remaining samples are blank: 
    for (int i = totalPTSamples; i < 31; i++) 
    {
        // According to real ProTracker files viewed in a hex viewer, the 30th and final byte
        //    of a blank sample is 0x01 and all 29 other bytes are 0x00. 
        for (int j = 0; j < 29; j++) 
        {
            fputc(0, fout); 
        }
        fputc(1, fout); 
    }

    #pragma endregion EXPORT_SAMPLE_INFO 

    ///////////////// EXPORT OTHER INFO  

    #pragma region EXPORT_OTHER_INFO 
    printf("Exporting other information...\n");

    fputc(dmf->moduleInfo.totalRowsInPatternMatrix + (int)usingSetupPattern, fout);   // Song length in patterns (not total number of patterns) 
    fputc(127, fout);                        // 0x7F - Useless byte that has to be here 

    // Pattern matrix (Each ProTracker pattern number is the same as its pattern matrix row number)
    for (uint8_t i = 0; i < dmf->moduleInfo.totalRowsInPatternMatrix + (int)usingSetupPattern; i++) 
        fputc(i, fout); 
    for (uint8_t i = dmf->moduleInfo.totalRowsInPatternMatrix + (int)usingSetupPattern; i < 128; i++) 
        fputc(0, fout);
    
    fwrite("M.K.", 1, 4, fout);  // ProTracker uses "M!K!" if there's more than 64 pattern matrix rows...
    #pragma endregion EXPORT_OTHER_INFO 
    
    ///////////////// EXPORT PATTERN DATA  

    #pragma region EXPORT_PATTERN_DATA 
    printf("Exporting pattern data...\n");

    if (usingSetupPattern) 
    {
        // Export initial tempo (An approximation. ProTracker can only support Deflemask tempo between 16 and 127.5 bpm.) 
        uint16_t effect = ((uint16_t)PT_SETSPEED << 4) | getPTTempo(getBPM(&dmf->moduleInfo));
        fputc(0, fout);  // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        fputc(0, fout);  // Sample period/effect param. (lower 8 bits) 
        fputc((effect & 0x0F00) >> 8, fout); // Sample number (lower 4b); effect code (upper 4b)
        fputc(effect & 0x00FF, fout); // Effect code (lower 8 bits) 

        // Export position jump (once the Pattern Break effect is implemented, it can be used instead.) 
        effect = ((uint16_t)PT_POSJUMP << 4) | 1; // Jump to next pattern 
        fputc(0, fout);  // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        fputc(0, fout);  // Sample period/effect param. (lower 8 bits) 
        fputc((effect & 0x0F00) >> 8, fout); // Sample number (lower 4b); effect code (upper 4b)
        fputc(effect & 0x00FF, fout); // Effect code (lower 8 bits) 

        // Blank (WAVE channel)
        fputc(0, fout); // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        fputc(0, fout); // Sample period/effect param. (lower 8 bits) 
        fputc(0, fout); // Sample number (lower 4b); effect code (upper 4b)
        fputc(0, fout); // Effect code (lower 8 bits) 

        // Blank (NOISE channel)
        fputc(0, fout); // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        fputc(0, fout); // Sample period/effect param. (lower 8 bits) 
        fputc(0, fout); // Sample number (lower 4b); effect code (upper 4b)
        fputc(0, fout); // Effect code (lower 8 bits) 
        
        // Loop through the rest of the rows in 1st pattern: 
        for (int patRow = 1; patRow < 64; patRow++) 
        {
            // Loop through channels: 
            for (int chan = 0; chan < Systems[SYS_GAMEBOY].channels; chan++) 
            {
                fputc(0, fout); // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
                fputc(0, fout); // Sample period/effect param. (lower 8 bits) 
                fputc(0, fout); // Sample number (lower 4b); effect code (upper 4b)
                fputc(0, fout); // Effect code (lower 8 bits) 
            }
        }
    }
    
    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.
    MODChannelState state[Systems[SYS_GAMEBOY].channels], stateJumpCopy[Systems[SYS_GAMEBOY].channels]; 
    for (int i = 0; i < Systems[SYS_GAMEBOY].channels; i++) 
    {
        state[i].channel = i;   // Set channel types: SQ1, SQ2, WAVE, NOISE. 
        state[i].dutyCycle = 0; // Default is 0 or a 12.5% duty cycle square wave. 
        state[i].wavetable = 0; // Default is wavetable #0.  
        state[i].sampleChanged = true; // Whether dutyCycle or wavetable recently changed 
        state[i].volume = PT_NOTE_VOLUMEMAX; // The max volume for a channel in PT 
        state[i].notePlaying = false; // Whether a note is currently playing on a channel 
        state[i].onHighNoteRange = false; // Whether a note is using the PT sample for the high note range. 
        state[i].needToSetVolume = false; // Whether the volume needs to be set (can happen after sample changes). 

        stateJumpCopy[i] = state[i]; // Shallow copy, but it's ok since there are no pointers to anything. 
    }
    
    PatternRow pat; 
    int16_t effectCode, effectValue;  

    // The main MODChannelState structs should NOT update during patterns or parts of 
    //   patterns that the Position Jump (Bxx) effect skips over. (Ignore loops)  
    //   Keep a copy of the main state for each channel, and once it reaches the  
    //   jump destination, overwrite the current state with the copied state. 
    bool stateSuspended = false; // true == currently in part that a Position Jump skips over 
    int8_t jumpDestination = -1; // Pattern matrix row where you are jumping to. Not a loop.  

    // Loop through ProTracker pattern matrix rows (corresponds to pattern numbers):  
    for (int patMatRow = 0; patMatRow < dmf->moduleInfo.totalRowsInPatternMatrix; patMatRow++)
    {
        // Loop through rows in a pattern: 
        for (int patRow = 0; patRow < 64; patRow++) 
        {
            // Loop through channels: 
            for (int chan = 0; chan < Systems[SYS_GAMEBOY].channels; chan++) 
            {
                pat = dmf->patternValues[chan][dmf->patternMatrixValues[chan][patMatRow]][patRow]; 
                effectCode = pat.effectCode[0];
                effectValue = pat.effectValue[0]; 
                
                // If just arrived at jump destination: 
                if (patMatRow == jumpDestination && patRow == 0 && stateSuspended) 
                {
                    // Restore state copies 
                    for (int v = 0; v < Systems[SYS_GAMEBOY].channels; v++) 
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
                        for (int v = 0; v < Systems[SYS_GAMEBOY].channels; v++) 
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
                    if (effectValue >= 0 && effectValue < dmf->totalWavetables)
                    {
                        state[chan].wavetable = effectValue; 
                        state[chan].sampleChanged = true;
                    }
                }
                #pragma endregion
                
                if (writeProTrackerPatternRow(fout, &pat, &state[chan])) 
                {
                    // Error occurred while writing the pattern row 
                    free(noteRangeStart); 
                    free(sampMap); 
                    free(sampleLength);
                    return; 
                }
            }
        }
    }
    
    #pragma endregion EXPORT_PATTERN_DATA 

    ///////////////// EXPORT SAMPLE DATA 

    #pragma region EXPORT SAMPLE DATA 
    printf("Exporting samples...\n"); 
    exportSampleData(fout); 
    #pragma endregion EXPORT SAMPLE DATA 
    
    ///////////////// CLEAN UP  
    fclose(fout); 

    free(noteRangeStart); 
    free(sampMap); 
    free(sampleLength);

    printf("Done exporting to MOD!\n");

    // Success 
}


static int writeProTrackerPatternRow(FILE *fout, PatternRow *pat, MODChannelState *state) 
{
    // Writes 4 bytes of pattern row information to the .mod file 
    uint16_t effect;
    if (checkEffects(pat, state, &effect))
    {
        return 1; // An error occurred 
    }

    if (pat->note.pitch == DMF_NOTE_EMPTY)  // No note is playing. Only handle effects.
    {
        fputc(0, fout);  // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        fputc(0, fout);         // Sample period/effect param. (lower 8 bits) 
        fputc((effect & 0x0F00) >> 8, fout);  // Sample number (lower 4b) = 0 b/c there's no note; effect code (upper 4b)
        fputc(effect & 0x00FF, fout);         // Effect code (lower 8 bits) 
    }
    else if (pat->note.pitch == DMF_NOTE_OFF) // Note OFF. Only handle note OFF effect.  
    {
        state->notePlaying = false;  

        //uint16_t period = proTrackerPeriodTable[2][DMF_NOTE_C % 12]; // This note won't be played, but it makes ProTracker happy. 
        //fputc((period & 0x0F00) >> 8, fout);  // Sample number (upper 4b); sample period/effect param. (upper 4b)
        //fputc(period & 0x00FF, fout);                                // Sample period/effect param. (lower 8 bits) 
        fputc(0, fout);  // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        fputc(0, fout);         // Sample period/effect param. (lower 8 bits) 
        fputc((effect & 0x0F00) >> 8, fout);    // Sample number (lower 4b); effect code (upper 4b)
        fputc(effect & 0x00FF, fout);                                // Effect code (lower 8 bits) 
    }
    else  // A note is playing 
    {
        uint8_t indexLow, indexHigh = 0;
        uint16_t period = 0;

        uint16_t modOctave = 0; // The note's octave to export to the mod file. 

        if (pat->note.pitch >= 1 && pat->note.pitch <= 12)  // A note 
        {
            if (state->channel != DMF_GAMEBOY_NOISE) 
            {
                // The indices for this SQW / WAVE sample's low note range and high note range: 
                indexLow = state->channel == DMF_GAMEBOY_WAVE ? state->wavetable + 4 : state->dutyCycle;
                indexHigh = indexLow + 4 + dmf->totalWavetables; 
                
                // If using high note range and the current note is in the high note range: 
                if (noteRangeStart[indexHigh].pitch != 0 && noteCompare(&pat->note, &noteRangeStart[indexHigh]) >= 0) 
                {
                    // Find the note's octave in ProTracker: 
                    modOctave = pat->note.octave - noteRangeStart[indexHigh].octave;   
                    if (!state->onHighNoteRange) 
                    {
                        // Switching to a different note range requires the use of a different ProTracker sample. 
                        state->sampleChanged = true; 
                    }
                    state->onHighNoteRange = true; 
                }
                else // Using the low note range 
                {
                    // Find the note's octave in ProTracker: 
                    modOctave = pat->note.octave - noteRangeStart[indexLow].octave; 
                    if (state->onHighNoteRange) 
                    {
                        // Switching to a different note range requires the use of a different ProTracker sample. 
                        state->sampleChanged = true; 
                    }
                    state->onHighNoteRange = false; 
                }
            }
        }

        // C# is the start of next octave in .dmf, not C- like in PT / .mod / Deflemask GUI, 
        //      so the note C- needs adjusted for exporting to mod: 
        if (pat->note.pitch == DMF_NOTE_C)  
        {
            modOctave++; 
        }

        period = proTrackerPeriodTable[modOctave][pat->note.pitch % 12]; 

        uint8_t sampleNumber;
        if (!state->sampleChanged && state->notePlaying) // Sample hasn't changed (same duty cycle or wavetable as before) and a note was playing 
        {
            sampleNumber = 0; // No sample. Keeps the previous sample number and prevents channel volume from being reset.  
        }
        else if (state->channel != DMF_GAMEBOY_NOISE) // A ProTracker sample needs to change  
        {
            sampleNumber = state->onHighNoteRange ? sampMap[indexLow + 4 + dmf->totalWavetables] : sampMap[indexLow]; // Get new PT sample number
            state->sampleChanged = false; // Just changed the sample, so resetting this for next time. 
            if (effect == PT_NOEFFECT_CODE) 
            {
                // When you change PT samples, the channel volume resets, so 
                //  if there are still no effects are being used on this pattern row, 
                //  use a volume change effect to set the volume to where it needs to be. 
                uint8_t newVolume = round(state->volume / 15.0 * 65.0); // Convert DMF volume to PT volume 
                effect = ((uint16_t)PT_SETVOLUME << 4) | newVolume; 
            } 
        }
        else // Noise channel 
        {
            sampleNumber = 0; // Keep noise channel quiet since it isn't implemented yet. 
        }

        fputc((sampleNumber & 0xF0) | ((period & 0x0F00) >> 8), fout);  // Sample number (upper 4b); sample period/effect param. (upper 4b)
        fputc(period & 0x00FF, fout);                                   // Sample period/effect param. (lower 8 bits) 
        fputc((sampleNumber << 4) | ((effect & 0x0F00) >> 8), fout);    // Sample number (lower 4b); effect code (upper 4b)
        fputc(effect & 0x00FF, fout);                                   // Effect code (lower 8 bits)  
        
        state->notePlaying = true; 
    }

   return 0; // Success 
}

static int checkEffects(PatternRow *pat, MODChannelState *state, uint16_t *effect)
{
    if (opt.effects == 2) // If using maximum amount of effects 
    {
        *effect = getProTrackerEffect(pat->effectCode[0], pat->effectValue[0]); // Effects must be in first row 
        if (pat->volume != state->volume && pat->volume != DMF_NOTE_NOVOLUME && (pat->note.pitch >= 1 && pat->note.pitch <= 12)) // If the note volume changes, and the volume is connected to a note  
        {
            if (*effect != PT_NOEFFECT_CODE) // If an effect is already being used    
            {
                /* Unlike Deflemask, setting the volume in ProTracker requires the use of 
                    an effect, and only one effect can be used at a time per channel. 
                    Same with turning a note off, which requires the EC0 command as far as I know. 
                    Note that the set duty cycle effect in Deflemask is not implemented as an effect in PT, 
                    so it does not count.  
                */
                setError(MOD_ERROR_EFFECT_VOLUME);
                return 1;
            }
            else // Only the volume changed 
            {
                uint8_t newVolume = round(pat->volume / 15.0 * 65.0); // Convert DMF volume to PT volume 
                *effect = ((uint16_t)PT_SETVOLUME << 4) | newVolume; // ??? 
                //printf("Vol effect = %x, effect code = %d, effect val = %d, dmf vol = %d\n", effect, PT_SETVOLUME, newVolume, pat->volume);  
                state->volume = pat->volume; // Update the state 
            }
        } 

        if (pat->note.pitch == DMF_NOTE_OFF && state->notePlaying) // If the note needs to be turned off 
        {
            if (*effect != PT_NOEFFECT_CODE) // If an effect is already being used 
            {
                /* Unlike Deflemask, setting the volume in ProTracker requires the use of 
                    an effect, and only one effect can be used at a time per channel. 
                    Same with turning a note off, which requires the EC0 command as far as I know. 
                    Note that the set duty cycle effect in Deflemask is not implemented as an effect in PT, 
                    so it does not count.  
                */
                setError(MOD_ERROR_EFFECT_VOLUME);
                return 1;
            }
            else // No effects except the note OFF 
            {
                *effect = (uint16_t)PT_CUTSAMPLE << 4; // Cut sample effect with value 0. 
            }
        }
    }
    else // Don't use effects (except for Volume change, Note OFF, or Position Jump)  
    {
        uint8_t total_effects = 0;
        int16_t volumeCopy = state->volume; // Save copy in case the volume change is canceled later 
        if (pat->volume != state->volume && pat->volume != DMF_NOTE_NOVOLUME && (pat->note.pitch >= 1 && pat->note.pitch <= 12)) // If the volume changed, we still want to handle that. Volume must be connected to a note.  
        {
            uint8_t newVolume = round(pat->volume / 15.0 * 65.0); // Convert DMF volume to PT volume 
            *effect = ((uint16_t)PT_SETVOLUME << 4) | newVolume; // ??? 
            state->volume = pat->volume; // Update the state 
            total_effects++;
        }
        if (pat->note.pitch == DMF_NOTE_OFF && state->notePlaying) // If the note needs to be turned off 
        {
            *effect = (uint16_t)PT_CUTSAMPLE << 4; // Cut sample effect with value 0. 
            state->volume = volumeCopy; // Cancel volume change if it occurred above.  
            total_effects++;
        }
        if (pat->effectCode[0] == DMF_POSJUMP) // Position Jump. Has priority over volume changes and note cuts. 
        {
            *effect = ((uint16_t)PT_POSJUMP << 4) | (pat->effectValue[0] + (int)usingSetupPattern); // Effects must be in first row  
            state->volume = volumeCopy; // Cancel volume change if it occurred above.  
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
            setError(MOD_ERROR_MULTIPLE_EFFECT);
            return 1;
        }
        else if (total_effects == 0)
        {
            *effect = PT_NOEFFECT_CODE; // No effect  
        } 
    }
    return 0; // Success 
}

static uint16_t getProTrackerEffect(int16_t effectCode, int16_t effectValue)
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
            ptEffVal = effectValue + (int)usingSetupPattern;  
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
            break; // This is only used when exporting as .vgm in Deflemask 
        case DMF_SETGLOBALFINETUNE:
            ptEff = PT_SETFINETUNE; break; // ? 
        case DMF_SETSPEEDVAL2:
            break; // ? 

        // Game Boy exclusive: 
        case DMF_SETWAVE:
            break; // This is handled in the exportMOD function and writeProTrackerPatternRow function 
        case DMF_SETNOISEPOLYCOUNTERMODE:
            break; // This is probably more than I need to worry about 
        case DMF_SETDUTYCYCLE:
            break; // This is handled in the exportMOD function and writeProTrackerPatternRow function 
        case DMF_SETSWEEPTIMESHIFT:
            break; // ? 
        case DMF_SETSWEEPDIR: 
            break; // ? 

    }

    return ((uint16_t)ptEff << 4) | ptEffVal; 
}

static int initSamples(FILE *fout, Note **lowestNote, Note **highestNote) 
{
    // This function loops through all DMF pattern contents to find the highest and lowest notes 
    //  for each square wave duty cycle and each wavetable. It also finds which SQW duty cycles are 
    //  used and which wavetables are used and stores this info in sampMap.  
    //  Then it calls the function finalizeSampMap. 

    // See declaration of sampMap for more information. 
    sampMap = (int8_t *)calloc(8 + dmf->totalWavetables * 2, sizeof(int8_t));
    sampleLength = (int16_t *)calloc(8 + dmf->totalWavetables * 2, sizeof(int16_t));

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.
    MODChannelState state[Systems[SYS_GAMEBOY].channels], stateJumpCopy[Systems[SYS_GAMEBOY].channels]; 
    for (int i = 0; i < Systems[SYS_GAMEBOY].channels; i++) 
    {
        state[i].channel = i;   // Set channel types: SQ1, SQ2, WAVE, NOISE. 
        state[i].dutyCycle = 0; // Default is 0 or a 12.5% duty cycle square wave. 
        state[i].wavetable = 0; // Default is wavetable #0.  
        state[i].sampleChanged = true; // Whether dutyCycle or wavetable recently changed 
        state[i].volume = PT_NOTE_VOLUMEMAX; // The max volume for a channel in PT 
        state[i].notePlaying = false; // Whether a note is currently playing on a channel 
        state[i].onHighNoteRange = false; // Whether a note is using the PT sample for the high note range. 
        state[i].needToSetVolume = false; // Whether the volume needs to be set (can happen after sample changes).

        stateJumpCopy[i] = state[i]; // Shallow copy, but it's ok since there are no pointers to anything. 
    }

    *lowestNote = (Note *)malloc((4 + dmf->totalWavetables) * sizeof(Note));
    *highestNote = (Note *)malloc((4 + dmf->totalWavetables) * sizeof(Note));
    noteRangeStart = (Note *)malloc((8 + dmf->totalWavetables * 2) * sizeof(Note));

    // The following are impossible notes which won't change if there are no notes for SQW / WAVE sample i: 
    for (int i = 0; i < 4 + dmf->totalWavetables; i++) 
    {
        (*lowestNote)[i] = (Note){DMF_NOTE_C, 10}; // C-10 
        (*highestNote)[i] = (Note){DMF_NOTE_C, 0}; // C-0 

        // Initialize noteRangeStart - low and high note ranges: 
        noteRangeStart[i] = (Note){0, 0}; 
        noteRangeStart[i + 4 + dmf->totalWavetables] = (Note){0, 0}; 
    }

    // The main MODChannelState structs should NOT update during patterns or parts of 
    //   patterns that the Position Jump (Bxx) effect skips over. (Ignore loops)  
    //   Keep a copy of the main state for each channel, and once it reaches the  
    //   jump destination, overwrite the current state with the copied state. 
    bool stateSuspended = false; // true == currently in part that a Position Jump skips over 
    int8_t jumpDestination = -1; // Pattern matrix row where you are jumping to. Not a loop. 

    int16_t effectCode, effectValue;  
    uint8_t indexLow = 0;

    // Most of the following nested for loop is copied from the export pattern data loop in exportMOD. 
    // I didn't want to do this, but I think having two of the same loop is the only simple way. 
    // Loop through SQ1, SQ2, and WAVE channels: 
    for (DMF_GAMEBOY_CHANNEL chan = DMF_GAMEBOY_SQW1; chan <= DMF_GAMEBOY_WAVE; chan++)  
    {
        // Loop through Deflemask patterns 
        for (int patMatRow = 0; patMatRow < dmf->moduleInfo.totalRowsInPatternMatrix; patMatRow++)  
        {
            // Row within pattern 
            for (int patRow = 0; patRow < 64; patRow++)  
            {
                PatternRow pat = dmf->patternValues[chan][dmf->patternMatrixValues[chan][patMatRow]][patRow];
                effectCode = pat.effectCode[0];
                effectValue = pat.effectValue[0]; 

                // If just arrived at jump destination: 
                if (patMatRow == jumpDestination && patRow == 0 && stateSuspended) 
                { 
                    // Restore state copies 
                    for (int v = 0; v < Systems[SYS_GAMEBOY].channels; v++) 
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
                        for (int v = 0; v < Systems[SYS_GAMEBOY].channels; v++) 
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
                    if (effectValue >= 0 && effectValue < dmf->totalWavetables)
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
                    sampMap[indexLow] = 1; // Mark this square wave or wavetable as used - low note range 
                    
                    // I'm keeping the notes in the .dmf form where the 1st note of an octave is C# and not C-. I can convert it later.
                    if (noteCompare(&pat.note, &((*highestNote)[indexLow])) == 1) // if pat.note > highestNote[indexLow] 
                    {
                        // Found a new highest note 
                        (*highestNote)[indexLow].octave = pat.note.octave; 
                        (*highestNote)[indexLow].pitch = pat.note.pitch;  
                    }
                    if (noteCompare(&pat.note, &((*lowestNote)[indexLow])) == -1) // if pat.note < lowestNote[indexLow] 
                    {
                        // Found a new lowest note 
                        (*lowestNote)[indexLow].octave = pat.note.octave; 
                        (*lowestNote)[indexLow].pitch = pat.note.pitch;
                    }
                }

            }
        } 
    }

    return finalizeSampMap(fout, *lowestNote, *highestNote);  
}

static int finalizeSampMap(FILE *fout, Note *lowestNote, Note *highestNote) 
{
    // This function assigns ProTracker (PT) sample numbers and exports sample info 

    int8_t finetune = 0; // Not really used, at least for now 
    uint8_t ptSampleNum = 1; // PT sample #0 is special.  
    uint8_t indexLow, indexHigh; 

    // Loop through SQW (low note range) and WAVE (low note range)
    for (int i = 0; i < 4 + dmf->totalWavetables; i++)  
    {
        indexLow = i; 
        indexHigh = i + 4 + dmf->totalWavetables; 

        // If this square wave duty cycle or wavetable is unused, skip to the next one:
        if (sampMap[i] == 0)  
        {
            sampMap[indexLow] = -1; // No PT sample needed for this square wave / WAVE sample (low note range) 
            sampMap[indexHigh] = -1; // No PT sample needed for this square wave / WAVE sample (high note range)
            sampleLength[indexLow] = -1; // No PT sample needed for this square wave / WAVE sample (low note range) 
            sampleLength[indexHigh] = -1; // No PT sample needed for this square wave / WAVE sample (high note range) 
            continue; 
        }

        // If the range is 3 octaves or less: 
        if (highestNote[i].octave*12 + highestNote[i].pitch - lowestNote[i].octave*12 - lowestNote[i].pitch <= 36) 
        {
            // (Note: The note C-n in the Deflemask tracker is called C-(n-1) in DMF format because the 1st note of an octave is C# in DMF files.)
            
            sampleLength[indexLow] = 0; // Should change if any of the options below work 
            finetune = 0; 

            if (noteCompare(&lowestNote[i], &(Note){DMF_NOTE_C, 1}) >= 0 && noteCompare(&highestNote[i], &(Note){DMF_NOTE_B, 4}) <= 0) 
            {
                // If between C-2 and B-4 (Deflemask tracker note format)  
                sampleLength[indexLow] = 64; // Double length 
                noteRangeStart[indexLow].octave = 1;
                noteRangeStart[indexLow].pitch = DMF_NOTE_C;
            }
             
            if (noteCompare(&lowestNote[i], &(Note){DMF_NOTE_C, 2}) >= 0 && noteCompare(&highestNote[i], &(Note){DMF_NOTE_B, 5}) <= 0) 
            {
                // If between C-3 and B-5 (Deflemask tracker note format)
                sampleLength[indexLow] = 32; // This is the default length anyway 
                noteRangeStart[indexLow].octave = 2;
                noteRangeStart[indexLow].pitch = DMF_NOTE_C;
            }
            else if (noteCompare(&lowestNote[i], &(Note){DMF_NOTE_C, 3}) >= 0 && noteCompare(&highestNote[i], &(Note){DMF_NOTE_B, 6}) <= 0) 
            {
                // If between C-4 and B-6 (Deflemask tracker note format) and none of the above options work 
                if (i >= 4 && !opt.allowDownsampling) // If on a wavetable instrument and can't downsample it 
                {
                    setErrorMsg(MOD_ERROR_WAVE_DOWNSAMPLE, itoa_p(i - 4, calloc(2, sizeof(char))));
                    return 1; 
                }
                
                sampleLength[indexLow] = 16;
                noteRangeStart[indexLow].octave = 3;
                noteRangeStart[indexLow].pitch = DMF_NOTE_C;
                
            }
            else if (noteCompare(&lowestNote[i], &(Note){DMF_NOTE_C, 4}) >= 0 && noteCompare(&highestNote[i], &(Note){DMF_NOTE_B, 7}) <= 0) 
            {
                // If between C-5 and B-7 (Deflemask tracker note format)  
                if (i >= 4 && !opt.allowDownsampling) // If on a wavetable instrument and can't downsample it
                {
                    setErrorMsg(MOD_ERROR_WAVE_DOWNSAMPLE, itoa_p(i - 4, calloc(2, sizeof(char))));
                    return 1; 
                }
                
                sampleLength[indexLow] = 8; 
                noteRangeStart[indexLow].octave = 4;
                noteRangeStart[indexLow].pitch = DMF_NOTE_C;
            }
            else if (noteCompare(&highestNote[i], &(Note){DMF_NOTE_C, 7}) == 0) 
            {
                // If between C#5 and C-8 (highest note) (Deflemask tracker note format):
                if (i >= 4 && !opt.allowDownsampling) // If on a wavetable instrument and can't downsample it
                {
                    setErrorMsg(MOD_ERROR_WAVE_DOWNSAMPLE, itoa_p(i - 4, calloc(2, sizeof(char))));
                    return 1; 
                }
                
                finetune = 0; // One semitone up from B = C- ??? was 7
                sampleLength[indexLow] = 8; 
                noteRangeStart[indexLow].octave = 4;
                noteRangeStart[indexLow].pitch = DMF_NOTE_C;
            }

            if (sampleLength[i] != 0) // If one of the above options worked 
            {
                if (i >= 4) // If on a wavetable instrument 
                {
                    // Double wavetable sample length.
                    // This is lower all wavetable instruments by one octave, which should make it 
                    // match the octave for wavetable instruments in Deflemask 
                    sampleLength[indexLow] *= 2; 
                } 
                sampMap[indexLow] = ptSampleNum; // Assign PT sample number to this square wave / WAVE sample  
                sampMap[indexHigh] = -1; // No PT sample needed for this square wave / WAVE sample (high note range) 
                exportSampleInfo(fout, ptSampleNum, -1, indexLow, indexHigh, finetune); 
                ptSampleNum++; 
            }

        }
        
        // If none of the options above worked. Both high note range and low note range are needed.
        if (sampleLength[i] == 0)   
        {
            sampleLength[indexLow] = 64; // Low note range (C-2 to B-4)
            sampleLength[indexHigh] = 8; // High note range (C-5 to B-7) 

            // If on a wavetable sample and cannot downsample it: 
            if (i >= 4 && !opt.allowDownsampling) 
            {
                if (!opt.allowDownsampling) 
                {
                    setErrorMsg(MOD_ERROR_WAVE_DOWNSAMPLE, itoa_p(i - 4, calloc(2, sizeof(char))));
                    return 1; 
                }

                // Double wavetable sample length.
                // This is lower all wavetable instruments by one octave, which should make it 
                // match the octave for wavetable instruments in Deflemask 
                sampleLength[indexLow] *= 2;
                sampleLength[indexHigh] *= 2;
            }

            noteRangeStart[indexLow].octave = 1;
            noteRangeStart[indexLow].pitch = DMF_NOTE_C;
            noteRangeStart[indexHigh].octave = 4;
            noteRangeStart[indexHigh].pitch = DMF_NOTE_C;
            finetune = 0; 

            if (noteCompare(&highestNote[i], &(Note){DMF_NOTE_C, 7}) == 0)
            {
                addWarning(MOD_WARNING_PITCH_HIGH); 
            }

            /*
            // If lowest possible note is needed:
            if (noteCompare(&lowestNote[i], &(Note){DMF_NOTE_C, 1}) == 0) 
            {
                // If highest and lowest possible notes are both needed:
                if (noteCompare(&highestNote[i], &(Note){DMF_NOTE_C, 7}) == 0)
                {
                    ///printf("WARNING: Can't use the highest Deflemask note (C-8).\n");
                    //printf("WARNING: Can't use both the highest note (C-8) and the lowest note (C-2).\n");
                }
                else // Only highest possible note is needed 
                {
                    finetune = 7; // One semitone up from B = C-  ???  
                }
                
            }
            */

            sampMap[indexLow] = ptSampleNum; // Assign PT sample number to this square wave / WAVE sample 
            sampMap[indexHigh] = ptSampleNum + 1; // Assign PT sample number to this square wave / WAVE sample 
            exportSampleInfo(fout, ptSampleNum, ptSampleNum + 1, indexLow, indexHigh, finetune); 
            ptSampleNum += 2; 

        }
    }

    totalPTSamples = ptSampleNum - 1; // Set the number of PT samples that will be needed. (minus sample #0 which is special) 
    return 0; // Success   
}

static void exportSampleInfo(FILE *fout, int8_t ptSampleNumLow, int8_t ptSampleNumHigh, uint8_t indexLow, uint8_t indexHigh, int8_t finetune)
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
                        case 0: fprintf(fout, "SQW, Duty 12.5%% (low) "); break; 
                        case 1: fprintf(fout, "SQW, Duty 25%% (low)   "); break; 
                        case 2: fprintf(fout, "SQW, Duty 50%% (low)   "); break; 
                        case 3: fprintf(fout, "SQW, Duty 75%% (low)   "); break; 
                    }
                }
                else  // WAVE 
                {
                    fprintf(fout, "Wavetable #%-2i (low)   ", index - 4);
                }
            } 
            else // Else, no "(low)" text. 
            {
                if (indexLow <= 3) // SQW 
                {
                    switch (indexLow) 
                    {
                        case 0: fprintf(fout, "SQW, Duty 12.5%%       "); break; 
                        case 1: fprintf(fout, "SQW, Duty 25%%         "); break; 
                        case 2: fprintf(fout, "SQW, Duty 50%%         "); break; 
                        case 3: fprintf(fout, "SQW, Duty 75%%         "); break; 
                    }
                }
                else  // WAVE 
                {
                    fprintf(fout, "Wavetable #%-2i         ", index - 4); 
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
                    case 0: fprintf(fout, "SQW, Duty 12.5%% (high)"); break; 
                    case 1: fprintf(fout, "SQW, Duty 25%% (high)  "); break; 
                    case 2: fprintf(fout, "SQW, Duty 50%% (high)  "); break; 
                    case 3: fprintf(fout, "SQW, Duty 75%% (high)  "); break; 
                }
            }
            else  // WAVE 
            {
                fprintf(fout, "Wavetable #%-2i (high)  ", index - 8 - dmf->totalWavetables); 
            }
            
        }
        else // If high note range is not available, we're done. 
        {
            break; 
        }
        
        fputc(sampleLength[index] >> 9, fout);          // Length byte 0 
        fputc(sampleLength[index] >> 1, fout);          // Length byte 1 
        fputc(finetune, fout);                          // Finetune value !!!
        fputc(PT_NOTE_VOLUMEMAX, fout);                 // Sample volume 
        fputc(0 , fout);                                // Repeat offset byte 0 
        fputc(0 , fout);                                // Repeat offset byte 1 
        fputc(sampleLength[index] >> 9, fout);              // Sample repeat length byte 0   
        fputc((sampleLength[index] >> 1) & 0x00FF, fout);   // Sample repeat length byte 1 

    }
    
}

static void exportSampleData(FILE *fout)
{ 
    uint8_t ptSampleNum = 1; 
    for (uint8_t i = 0; i < 4 + dmf->totalWavetables; i++) 
    {
        if (sampMap[i] == ptSampleNum) 
        {
            exportSampleDataHelper(fout, ptSampleNum, i); 
            ptSampleNum++; 
            if (sampMap[i + 4 + dmf->totalWavetables] == ptSampleNum) 
            {
                exportSampleDataHelper(fout, ptSampleNum, i + 4 + dmf->totalWavetables); 
                ptSampleNum++;
            }
        }
    }
}

static void exportSampleDataHelper(FILE *fout, uint8_t ptSampleNum, uint8_t index) 
{
    // This function must be called for SQW / WAVE samples in the same 
    //      order as their PT sample numbers. The exportSampleData function guarantees it. 

    // If it's a square wave sample 
    if (index <= 3 || (index >= 4 + dmf->totalWavetables && index <= 7 + dmf->totalWavetables)) 
    {
        uint8_t duty[] = {1, 2, 4, 6}; 
        uint8_t dutyNum = index <= 3 ? index : index - 4 - dmf->totalWavetables; 
        
        // This loop exports a square wave with the correct length and duty cycle: 
        for (int i = 1; i <= sampleLength[index]; i++) 
        {
            if ((i * 8.f) / sampleLength[index] <= (float)duty[dutyNum])
            {
                fputc(127, fout); // high 
            }
            else
            {
                fputc(-128, fout); // low 
            }
        }
    } 
    else  // Wavetable sample 
    {
        uint8_t waveNum = index <= 3 + dmf->totalWavetables ? index - 4 : index - 8 - dmf->totalWavetables;   
        for (int i = 0; i < sampleLength[index]; i++) 
        {
            // Note: For the Deflemask Game Boy system, all wavetable lengths are 32. 
            if (sampleLength[index] == 128) // Quadruple length 
            {
                // Convert from DMF sample values (0 to 15) to PT sample values (-128 to 127). 
                fputc((int8_t)((dmf->wavetableValues[waveNum][i / 4] / 15.f * 255.f) - 128.f), fout); 
            }
            else if (sampleLength[index] == 64) // Double length 
            {
                // Convert from DMF sample values (0 to 15) to PT sample values (-128 to 127). 
                fputc((int8_t)((dmf->wavetableValues[waveNum][i / 2] / 15.f * 255.f) - 128.f), fout);  
            }
            else if (sampleLength[index] == 32) // Normal length 
            {
                // Convert from DMF sample values (0 to 15) to PT sample values (-128 to 127). 
                fputc((int8_t)((dmf->wavetableValues[waveNum][i] / 15.f * 255.f) - 128.f), fout); 
            } 
            else if (sampleLength[index] == 16) // Half length (loss of information)   
            {
                // Take average of every two sample values to make new sample value 
                int avg = (int8_t)((dmf->wavetableValues[waveNum][i * 2] / 15.f * 255.f) - 128.f);
                avg += (int8_t)((dmf->wavetableValues[waveNum][(i * 2) + 1] / 15.f * 255.f) - 128.f); 
                avg /= 2; 
                fputc((int8_t)avg, fout); 
            }
            else if (sampleLength[index] == 8) // Quarter length (loss of information)   
            {
                // Take average of every four sample values to make new sample value 
                int avg = (int8_t)((dmf->wavetableValues[waveNum][i * 4] / 15.f * 255.f) - 128.f);
                avg += (int8_t)((dmf->wavetableValues[waveNum][(i * 4) + 1] / 15.f * 255.f) - 128.f); 
                avg += (int8_t)((dmf->wavetableValues[waveNum][(i * 4) + 2] / 15.f * 255.f) - 128.f); 
                avg += (int8_t)((dmf->wavetableValues[waveNum][(i * 4) + 3] / 15.f * 255.f) - 128.f); 
                avg /= 4; 
                fputc((int8_t)avg, fout); 
            }
        }
    }
}

static uint8_t getPTTempo(double bpm)
{
    // Get ProTracker tempo from Deflemask bpm. 
    if (bpm * 2.0 > 255.0) // If tempo is too high (over 127.5 bpm)
    {
        addWarning(MOD_WARNING_TEMPO_HIGH); 
        return 255;
    }
    else if (bpm * 2.0 < 32.0) // If tempo is too low (under 16 bpm)
    {
        addWarning(MOD_WARNING_TEMPO_LOW); 
        return 32; 
    }
    else // Tempo is okay for ProTracker 
    {
        // ProTracker tempo is twice the Deflemask bpm.
        return bpm * 2; 
    }
}

// Portable version of itoa but without base argument (assumed to be 10)
char *itoa_p(int _Val, char *_DstBuf) 
{
    sprintf(_DstBuf, "%i", _Val); 
    return _DstBuf; 
}

static void setError(MOD_ERROR error)
{
    // Store error 
    issues.error.errorCode = error;
    issues.error.errorInfo = NULL;
} 

static void setErrorMsg(MOD_ERROR error, char *msg)
{
    // Store error 
    issues.error.errorCode = error;
    issues.error.errorInfo = msg;
} 

static void addWarning(MOD_WARNING warning) 
{
    if (warning != MOD_WARNING_NONE) 
    {
        // Make note if the same warning has occurred before 
        issues.warnings.multipleWarnings |= issues.warnings.warningCode & warning; 
        // Store warning 
        issues.warnings.warningCode |= warning; 
    }
    else 
    {
        // Reset warnings 
        issues.warnings.multipleWarnings = MOD_WARNING_NONE;
        issues.warnings.warningCode = MOD_WARNING_NONE; 
    }
    
}

void printError()
{
    switch (issues.error.errorCode) 
    {
        case MOD_ERROR_NONE:
            printf("No errors occurred.\n");
            break;
        case MOD_ERROR_NOT_GAMEBOY: 
            printf("ERROR: Only the Game Boy system is currently supported.\n");
            break;
        case MOD_ERROR_TOO_MANY_PAT_MAT_ROWS: 
            printf("ERROR: Too many rows of patterns in the pattern matrix. 64 is the maximum. (63 if using Setup Pattern.)\n");
            break; 
        case MOD_ERROR_NOT_64_ROW_PATTERN: 
            printf("ERROR: Patterns must have 64 rows. \n");
            printf("       A workaround for this issue is planned for a future update to dmf2mod.\n");
            break;
        case MOD_ERROR_WAVE_DOWNSAMPLE: 
            printf("ERROR: Cannot use wavetable instrument #%s without loss of information.\n", issues.error.errorInfo);
            printf("       Try using the '--downsample' option.\n");
            break; 
        case MOD_ERROR_EFFECT_VOLUME: 
            printf("ERROR: An effect and a volume change (or Note OFF) cannot both appear in the same row of the same channel.\n");
            printf("       Try fixing this issue in Deflemask or use the '--effects=MIN' option.\n"); 
            break; 
        case MOD_ERROR_MULTIPLE_EFFECT: 
            printf("ERROR: No more than one Note OFF, Volume Change, or Position Jump can appear in the same row of the same channel.\n");
            break; 
    }
}

void printWarnings()
{
    if (issues.warnings.warningCode == MOD_WARNING_NONE) 
    {
        printf("No warnings.\n"); 
    }

    unsigned int i = 1; // Iterate through warnings 

    while (i < 1 << 15) 
    {
        if (i & issues.warnings.warningCode) // If this warning occurred 
        {
            if (i & issues.warnings.multipleWarnings) // If this warning occurred more than once 
            {
                printf("WARNING (MULTIPLE OCCURRENCES): ");
            }
            else
            {
                printf("WARNING: ");
            }
            
            switch (i) 
            {
                case MOD_WARNING_PITCH_HIGH:
                    printf("Cannot use the highest Deflemask note (C-8) on some MOD players including ProTracker.\n");
                    break;
                case MOD_WARNING_TEMPO_LOW: 
                    printf("Tempo is too low for ProTracker. Using 16 bpm instead.\n");
                    printf("         ProTracker only supports tempos between 16 and 127.5 bpm.\n"); 
                    break;
                case MOD_WARNING_TEMPO_HIGH: 
                    printf("Tempo is too high for ProTracker. Using 127.5 bpm instead.\n");
                    printf("         ProTracker only supports tempos between 16 and 127.5 bpm.\n"); 
                    break; 
                case MOD_WARNING_EFFECT_IGNORED: 
                    printf("A Deflemask effect was ignored due to limitations of the MOD format.\n"); 
                    break; 
                default: 
                    // No more warnings are defined 
                    i = 1 << 14; // Cause while loop to end 
                    break; 
            }
            printf("\n"); 
        }
        
        i <<= 1; // Next warning 
    }
}

void cleanUp()
{
    free(issues.error.errorInfo); 

    // Remove the MOD file if an error occurred and dmf2mod created the MOD file: 
    if (issues.error.errorCode != MOD_ERROR_NONE && !filePreviouslyExisted) 
    {
        // This will fail if you try to run dmf2mod without elevated permissions: 
        remove(filename);
        ///perror("An error occurred when attempting to delete file: ");

        // TODO: Write MOD data to a buffer, then if conversion finishes successfully, write that buffer 
        //      to the file. This would be a better solution and solve potential problems like the one above. 
    }

    free(filename);
}

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




