/*
mod.c
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Provides functions for exporting the contents of a .dmf file 
to ProTracker's .mod format. 

Several limitations apply in order to export. For example, the 
.dmf file must use the Game Boy system, patterns must have 64 
rows, only one effect column is allowed per channel, etc.  
*/

// TODO: Position jump (Bxx) effects can probably mess up the PT sample number that is being used. 
//          For example, if a position jump skips over a 10xx or 12xx effect, dmf2mod will change 
//          the PT sample number when it should stay the same. Need a more robust way of resolving this.  

#include "mod.h"

extern const System Systems[10]; 

CMD_Options Opt;

int exportMOD(char *fname, DMFContents *dmf, CMD_Options opt) 
{
    FILE *fout;

    ///////////////// EXPORT SONG NAME  

    if (strcmp(getFilenameExt(fname), "mod") != 0) 
    {
        char *fname2 = malloc(sizeof(fname) + 4*sizeof(char)); 
        strcpy(fname2, fname); 
        strcpy(fname2, ".mod"); 
        printf("Exporting to %s.\n", fname2);
        fout = fopen(fname2, "wb"); // Add ".mod" extension if it wasn't specified in the command-line argument 
        free(fname2); 
    }
    else
    {
        fout = fopen(fname, "wb");
        printf("Exporting to %s.\n", fname);
    }

    printf("Starting to export to .mod....\n");
    
    if (dmf->sys.id != Systems[SYS_GAMEBOY].id) // If it's not a Game Boy 
    {
        printf("Error: Only the Game Boy system is currently supported. \n");
        return 1;
    }

    if (dmf->moduleInfo.totalRowsInPatternMatrix > 128) // totalRowsInPatternMatrix is 1 more than it actually is 
    {
        printf("Error: There must be 128 or fewer rows in the pattern matrix.\n");
        return 1;
    }

    if (dmf->moduleInfo.totalRowsPerPattern != 64) 
    {
        printf("Error: Patterns must have 64 rows. \n");
        return 1;
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
    
    ///////////////// EXPORT SAMPLE INFO 
    
    /*
    Note lowestSQWNote, highestSQWNote, lowestWAVENote, highestWAVENote; 

    initialCheck(dmf, &lowestSQWNote, &highestSQWNote, &lowestWAVENote, &highestWAVENote);
    printf("lowestSQWNote=%i,%i\n", lowestSQWNote.pitch, lowestSQWNote.octave); 
    printf("highestSQWNote=%i,%i\n", highestSQWNote.pitch, highestSQWNote.octave); 
    printf("lowestWAVENote=%i,%i\n", lowestWAVENote.pitch, lowestWAVENote.octave); 
    printf("highestWAVENote=%i,%i\n", highestWAVENote.pitch, highestWAVENote.octave); 
    */ 

    // Export 4 square wave samples 
    for (int i = 0; i < 4; i++)
    {
        fwrite(sqwSampleNames[i], 1, 22, fout);      // Sample i+1 - 22B - name 
        fputc(sqwSampleLength >> 9, fout);           // Sample i+1 - 1B - length byte 0 (+2 because 1st 2 bytes are ignored)
        fputc(sqwSampleLength >> 1, fout);           // Sample i+1 - 1B - length byte 1 (+2 because 1st 2 bytes are ignored) 
        fputc(0, fout);                              // Sample i+1 - 1B - finetune value - 0 
        fputc(64, fout);                             // Sample i+1 - 1B - volume - full volume  
        fputc(0 , fout);                             // Sample i+1 - 1B - repeat offset byte 0 
        fputc(0 , fout);                             // Sample i+1 - 1B - repeat offset byte 1 
        fputc(sqwSampleLength >> 9, fout);           // Sample i+1 - 1B - sample repeat length byte 0 - 0   
        fputc((sqwSampleLength >> 1) & 0x00FF, fout);       // Sample i+1 - 1B - sample repeat length byte 1 - 64 
    }

    if (dmf->totalWavetables > 27) 
    {
        printf("Error: Too many wavetables. The maximum is 27.\n");
        return 1;
    }

    for (int i = 0; i < dmf->totalWavetables; i++) 
    {
        // Export Sample i+5 (wavetables)
        fprintf(fout, "Wavetable #%-11i", i+1);
        fputc(dmf->wavetableSizes[i] >> 9, fout);
        fputc(dmf->wavetableSizes[i] >> 1, fout);
        fputc(0, fout);                              // Finetune value = 0 
        fputc(64, fout);                             // Volume = full volume  
        fputc(0 , fout);                             // Repeat offset byte 0 
        fputc(0 , fout);                             // Repeat offset byte 1 
        fputc(dmf->wavetableSizes[i] >> 9, fout);             // Sample repeat length byte 0  
        fputc((dmf->wavetableSizes[i] >> 1) & 0x00FF, fout);  // Sample repeat length byte 1
    }

    // The remaining samples are blank: 
    for (int i = dmf->totalWavetables; i < 27; i++) 
    {
        // According to real ProTracker files viewed in a hex viewer, the 30th and final byte
        //    of a blank sample is 0x01 and all 29 other bytes are 0x00. 
        for (int j = 0; j < 29; j++) 
        {
            fputc(0, fout); 
        }
        fputc(1, fout); 
    }

    ///////////////// EXPORT OTHER INFO  

    fputc(dmf->moduleInfo.totalRowsInPatternMatrix, fout);   // Song length in patterns (not total number of patterns) 
    fputc(127, fout);                        // 0x7F - Useless byte that has to be here 

    if (dmf->moduleInfo.totalRowsInPatternMatrix > 64) 
    {
        printf("Error: Too many rows of patterns in the pattern matrix. 64 is the maximum.\n");
        return 1;
    }

    // Pattern matrix (Each ProTracker pattern number is the same as its pattern matrix row number)
    for (uint8_t i = 0; i < dmf->moduleInfo.totalRowsInPatternMatrix; i++) 
        fputc(i, fout); 
    for (uint8_t i = dmf->moduleInfo.totalRowsInPatternMatrix; i < 128; i++) 
        fputc(0, fout);
    
    fwrite("M.K.", 1, 4, fout);  // ProTracker uses "M!K!" if there's more than 64 pattern matrix rows...

    ///////////////// EXPORT PATTERN DATA  

    printf("Exporting pattern data...\n");

    for (int channel = 0; channel < dmf->sys.channels; channel++)
    {
        if (dmf->channelEffectsColumnsCount[channel] > 1 && opt.useEffects) 
        {
            // TODO: Allow any amount of effects columns but only use first effect it finds   
            printf("Error: Each channel can only have 1 effects column.\n");
            return 1;
        }
        
    }

    // The current square wave duty cycle, note volume, and other information that the 
    //      tracker stores for each channel while playing a tracker file.
    MODChannelState state[Systems[SYS_GAMEBOY].channels], stateJumpCopy[Systems[SYS_GAMEBOY].channels]; 
    for (int i = 0; i < 4; i++) 
    {
        state[i].channel = i;   // Set channel types: SQ1, SQ2, WAVE, NOISE. 
        state[i].dutyCycle = 1; // Default is 1 or a 12.5% duty cycle square wave. In mod, the first sample is sample #1.
        state[i].wavetable = 5; // The 5th PT sample (zero-indexed) is the 1st wavetable 
        state[i].sampleChanged = true; // Whether dutyCycle or wavetable recently changed 
        state[i].volume = PT_NOTE_VOLUMEMAX; // The max volume for a channel in PT 
        state[i].notePlaying = false; // Whether a note is currently playing on a channel 

        stateJumpCopy[i] = state[i]; // Shallow copy, but it's ok since there are no pointers to anything. 
    }

    int16_t effectCode, effectValue;  

    // Loop through ProTracker pattern matrix rows (corresponds to pattern numbers):  
    for (int patMatRow = 0; patMatRow < dmf->moduleInfo.totalRowsInPatternMatrix; patMatRow++)
    {
        // Loop through rows in a pattern: 
        for (int patRow = 0; patRow < 64; patRow++) 
        {
            // Loop through channels: 
            for (int chan = 0; chan < Systems[SYS_GAMEBOY].channels; chan++) 
            {
                effectCode = dmf->patternValues[chan][dmf->patternMatrixValues[chan][patMatRow]][patRow].effectCode[0];
                effectValue = dmf->patternValues[chan][dmf->patternMatrixValues[chan][patMatRow]][patRow].effectValue[0]; 
                
                // If just arrived at jump destination: 
                if (patMatRow == jumpDestination && patRow == 0 && stateSuspended) 
                {
                    //printf("---Restoring state. patMatRow=%i\n", patMatRow); 
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
                    //printf("---Jump found and Should suspend state here? effectValue==%i, patMatRow=%i\n", effectValue, patMatRow); 
                    if (effectValue >= patMatRow) // If not a loop 
                    {
                        //printf("------Not a loop. Saving copy...\n");
                        // Save copies of states  
                        for (int v = 0; v < Systems[SYS_GAMEBOY].channels; v++) 
                        {
                            stateJumpCopy[v] = state[v]; 
                        }
                        stateSuspended = true; 
                        jumpDestination = effectValue; 
                    }
                }
                else if (effectCode == DMF_SETDUTYCYCLE && state[chan].dutyCycle != effectValue + 1) // If sqw channel duty cycle needs to change 
                {
                    state[chan].dutyCycle = effectValue + 1; // Add 1 b/c first mod sample is sample #1 
                    state[chan].sampleChanged = true; 
                }
                else if (effectCode == DMF_SETWAVE && state[chan].wavetable != effectValue + 5) // If wave channel wavetable needs to change 
                {
                    state[chan].wavetable = effectValue + 5; // Add 5 b/c first wavetable sample is sample #5 
                    state[chan].sampleChanged = true; 
                }
                if (writeProTrackerPatternRow(fout, &dmf->patternValues[chan][dmf->patternMatrixValues[chan][patMatRow]][patRow], &state[chan], opt)) 
                {
                    // Error occurred while writing the pattern row 
                    return 1; 
                }
            }
        }
    }
    
    ///////////////// EXPORT SAMPLE DATA 

    printf("Exporting samples...\n"); 

    // Export square wave data 
    for (int i = 0; i < 4; i++) 
    {
        fwrite(sqwSampleDuty[i], 1, sqwSampleLength, fout); 
    }

    // Export wavetable data 
    for (int wt = 0; wt < dmf->totalWavetables; wt++) 
    {
        for (int val = 0; val < dmf->wavetableSizes[wt]; val++)
        {
            // Convert from DMF sample values (0 to 15) to PT sample values (-128 to 127). 
            fputc((int8_t)((dmf->wavetableValues[wt][val] / 15.f * 255.f) - 128.f), fout); 
        }
    }

    fclose(fout); 

    printf("Done exporting to .mod!\n");

    return 0; // Success 
}


// Writes 4 bytes of pattern row information to the .mod file 
int writeProTrackerPatternRow(FILE *fout, PatternRow *pat, MODChannelState *state, CMD_Options opt) 
{
    uint16_t effect;

    if (checkEffects(pat, state, opt, &effect))
    {
        return 1; // An error occurred 
    }

    if (pat->note == DMF_NOTE_EMPTY)  // No note is playing. Only handle effects.
    {
        fputc(0, fout);  // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        fputc(0, fout);         // Sample period/effect param. (lower 8 bits) 
        fputc((effect & 0x0F00) >> 8, fout);  // Sample number (lower 4b) = 0 b/c there's no note; effect code (upper 4b)
        fputc(effect & 0x00FF, fout);         // Effect code (lower 8 bits) 
    }
    else if (pat->note == DMF_NOTE_OFF) // Note OFF. Only handle note OFF effect.  
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
        uint16_t period = 0;

        uint16_t modOctave = pat->octave - 2; // Transpose down two octaves because a C-4 in Deflemask is the same pitch as a C-2 in ProTracker. 
        if (pat->note == DMF_NOTE_C) // C# is the start of next octave in .dmf, not C- 
        {
            modOctave++; 
        }

        if (modOctave > 4)
        {
            printf("Warning: Octave must be 4 or less in MOD. (Octave = %u) Setting it to 4.\n", modOctave);
            //return 1; 
            modOctave = 4; // !!!
        }

        if (pat->note >= 1 && pat->note <= 12)  // A note 
        {
            period = proTrackerPeriodTable[modOctave][pat->note % 12]; 
        }

        uint8_t sampleNumber;
        if (!state->sampleChanged && state->notePlaying) // Sample hasn't changed (same duty cycle or wavetable as before) and a note was playing 
        {
            sampleNumber = 0; // No sample. Keeps the previous sample number and prevents channel volume from being reset.  
        }
        else if (state->channel == DMF_GAMEBOY_SQW1 || state->channel == DMF_GAMEBOY_SQW2) // A square wave channel duty cycle changed 
        {
            sampleNumber = state->dutyCycle; 
            state->sampleChanged = false; // Just changed the sample, so reset this for next time. 
        }
        else if (state->channel == DMF_GAMEBOY_WAVE)  // Wave channel wavetable changed 
        {
            sampleNumber = state->wavetable; 
            state->sampleChanged = false; // Just changed the sample, so reset this for next time. 
        } 
        else // Noise channel (not implemented yet)
        {
            // Placeholder (using square wave here for now):
            sampleNumber = state->dutyCycle; 
        }

        fputc((sampleNumber & 0xF0) | ((period & 0x0F00) >> 8), fout);  // Sample number (upper 4b); sample period/effect param. (upper 4b)
        fputc(period & 0x00FF, fout);                                // Sample period/effect param. (lower 8 bits) 
        fputc((sampleNumber << 4) | ((effect & 0x0F00) >> 8), fout);    // Sample number (lower 4b); effect code (upper 4b)
        fputc(effect & 0x00FF, fout);                                // Effect code (lower 8 bits)  
        
        state->notePlaying = true; 
    }

   return 0; // Success 
}

int checkEffects(PatternRow *pat, MODChannelState *state, CMD_Options opt, uint16_t *effect)
{
    if (opt.useEffects) // If using effects 
    {
        *effect = getProTrackerEffect(pat->effectCode[0], pat->effectValue[0]); // Effects must be in first row 
        if (pat->volume != state->volume && pat->volume != DMF_NOTE_NOVOLUME) // If the note volume changes 
        {
            if (*effect != PT_NOEFFECT_CODE) // If an effect is already being used    
            {
                /* Unlike Deflemask, setting the volume in ProTracker requires the use of 
                    an effect, and only one effect can be used at a time per channel. 
                    Same with turning a note off, which requires the EC0 command as far as I know. 
                    Note that the set duty cycle effect in Deflemask is not implemented as an effect in PT, 
                    so it does not count.  
                */
                printf("Error: An effect and a volume change (or note OFF) cannot both appear in the same row of the same channel.\n");
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

        if (pat->note == DMF_NOTE_OFF && state->notePlaying) // If the note needs to be turned off 
        {
            if (*effect != PT_NOEFFECT_CODE) // If an effect is already being used 
            {
                /* Unlike Deflemask, setting the volume in ProTracker requires the use of 
                    an effect, and only one effect can be used at a time per channel. 
                    Same with turning a note off, which requires the EC0 command as far as I know. 
                    Note that the set duty cycle effect in Deflemask is not implemented as an effect in PT, 
                    so it does not count.  
                */
                printf("Error: An effect and a note OFF (or volume change) cannot both appear in the same row of the same channel.\n");
                return 1;
            }
            else // No effects except the note OFF 
            {
                *effect = (uint16_t)PT_CUTSAMPLE << 4; // Cut sample effect with value 0. 
            }
        }
    }
    else // Don't use effects (except for volume and note OFF) 
    {
        uint8_t total_effects = 0;
        if (pat->volume != state->volume && pat->volume != DMF_NOTE_NOVOLUME) // If the volume changed, we still want to handle that 
        {
            uint8_t newVolume = round(pat->volume / 15.0 * 65.0); // Convert DMF volume to PT volume 
            *effect = ((uint16_t)PT_SETVOLUME << 4) | newVolume; // ??? 
            state->volume = pat->volume; // Update the state 
            total_effects++;
        }
        if (pat->note == DMF_NOTE_OFF && state->notePlaying) // If the note needs to be turned off 
        {
            *effect = (uint16_t)PT_CUTSAMPLE << 4; // Cut sample effect with value 0. 
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
            printf("Error: An effect and a note OFF / volume change cannot both appear in the same row of the same channel.\n");
            return 1;
        }
        else if (total_effects == 0)
        {
            *effect = PT_NOEFFECT_CODE; // No effect  
        } 
    }
    return 0; // Success 
}

uint16_t getProTrackerEffect(int16_t effectCode, int16_t effectValue)
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
            ptEff = PT_ARP; break;
        case DMF_PORTUP: 
            ptEff = PT_PORTUP; break;
        case DMF_PORTDOWN: 
            ptEff = PT_PORTDOWN; break;
        case DMF_PORT2NOTE:
            ptEff = PT_PORT2NOTE; break;
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
            ptEffVal = effectValue;  
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
            ptEff = PT_CUTSAMPLE; break; // ?
            ptEffVal = 0; // Cut note immediately  
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


// Unfinished 
Note noteConvert(Note n, DMF_GAMEBOY_CHANNEL chan, bool downsamplingNeeded) 
{
    //uint8_t ptPitch, ptOctave; 
    //if (n.octave)
    /*
    if (n.octave > 6)
    {
        printf("Warning: Octave must be 4 or less in MOD. (Octave = %u) Setting it to 4.\n");
    }
    */ 
    return (Note){n.pitch % 12, n.octave - 2};
}

void initialCheck(DMFContents *dmf, Note *lowestSQWNote, Note *highestSQWNote, Note *lowestWAVENote, Note *highestWAVENote)
{
    // This function loops through all DMF pattern contents to find the highest and lowest notes 
    //  on both the SQW channels and the WAVE channel. It also finds which SQW duty cycles are 
    //  used and which wavetables are used and stores this info in sampMap. 
    //  The function finalizeSampMap must be called after this one before you can fully use sampMap. 

    // See declaration of sampMap for more information. 
    sampMap = (int8_t *)calloc(8 + dmf->totalWavetables*2, sizeof(int8_t));
    sampMap[0] = 1;  // Mark 12.5% duty cycle as used - low note range 
    sampMap[4] = 1;  // Mark 12.5% duty cycle as used - high note range 
    sampMap[8] = 1;  // Mark wavetable #0 as used - low note range 
    sampMap[8 + dmf->totalWavetables] = 1;  // Mark wavetable #0 as used - high note range  

    // The following are impossible notes which won't change if there are no 
    //  notes on the square wave channels or wave channel respectively: 

    // C-10
    lowestSQWNote->pitch = DMF_NOTE_C; 
    lowestSQWNote->octave = 10;

    // C-0
    highestSQWNote->pitch = DMF_NOTE_C; 
    highestSQWNote->octave = 0;

    // C-10
    lowestWAVENote->pitch = DMF_NOTE_C; 
    lowestWAVENote->octave = 10;

    // C-0
    highestWAVENote->pitch = DMF_NOTE_C; 
    highestWAVENote->octave = 0;

    // This cuts down on the amount of code needed in the nested for loop below  
    Note *currentChannelHighest, *currentChannelLowest; 

    // Loop through SQ1, SQ2, and WAVE channels 
    for (DMF_GAMEBOY_CHANNEL chan = DMF_GAMEBOY_SQW1; chan <= DMF_GAMEBOY_WAVE; chan++)  
    {
        if (chan < DMF_GAMEBOY_WAVE) // If a SQW channel 
        {
            currentChannelLowest = lowestSQWNote; 
            currentChannelHighest = highestSQWNote; 
        }
        else if (chan == DMF_GAMEBOY_WAVE) // If the WAVE channel 
        {
            currentChannelLowest = lowestWAVENote; 
            currentChannelHighest = highestWAVENote; 
        }
            
        // Loop through Deflemask patterns 
        for (int i = 0; i < dmf->moduleInfo.totalRowsInPatternMatrix; i++)  
        {
            for (int j = 0; j < 64; j++) // Row within pattern 
            {
                PatternRow pat = dmf->patternValues[chan][dmf->patternMatrixValues[chan][i]][j];
                
                if (pat.note >= 1 && pat.note <= 12) // A note 
                {
                    // I'm keeping the notes in the .dmf form where the 1st note of an octave is C# and not C-. I can convert it later.
                    if (pat.octave + pat.note / 13.f > currentChannelHighest->octave + currentChannelHighest->pitch / 13.f) 
                    {
                        // Found a new highest note 
                        currentChannelHighest->octave = pat.octave; 
                        currentChannelHighest->pitch = pat.note;  
                    }
                    if (pat.octave + pat.note / 13.f < currentChannelLowest->octave + currentChannelLowest->pitch / 13.f) 
                    {
                        // Found a new lowest note 
                        currentChannelLowest->octave = pat.octave; 
                        currentChannelLowest->pitch = pat.note;
                    }
                }

                // Find which SQW duty cycles are used and which wavetables are used. 
                for (int col = 0; col < dmf->channelEffectsColumnsCount[chan]; col++)
                {
                    if (chan < DMF_GAMEBOY_WAVE && pat.effectCode[col] == DMF_SETDUTYCYCLE) 
                    {
                        if (pat.effectValue[col] > 3 || pat.effectValue[col] < 1) // Must be valid. (Also ones with value == 0 are added at the start)
                            continue; 
                        if (sampMap[pat.effectValue[col]] == 0) // If this duty cycle is not marked as used yet
                        {
                            sampMap[pat.effectValue[col]] = 1;      // Mark this duty cycle as used - low note range 
                            sampMap[pat.effectValue[col] + 4] = 1;  // Mark this duty cycle as used - high note range 
                            break; 
                        }   
                    }
                    else if (chan == DMF_GAMEBOY_WAVE && pat.effectCode[col] == DMF_SETWAVE) 
                    {
                        if (pat.effectValue[col] > 3 || pat.effectValue[col] < 1) // Must be valid. (Also ones with value == 0 are added at the start)
                            continue;
                        if (sampMap[pat.effectValue[col]] == 0) // If this wavetable is not marked as used yet
                        {
                            sampMap[pat.effectValue[col] + 8] = 1;      // Mark this wavetable as used - low note range 
                            sampMap[pat.effectValue[col] + 8 + dmf->totalWavetables] = 1;  // Mark this wavetable as used - high note range 
                            break; 
                        }   
                    }
                }  
            }
        } 
    }
}

uint8_t finalizeSampMap(uint8_t totalWavetables, bool doubleSQWSamples, bool doubleWavetableSamples) 
{
    // The function initialCheck must be called before this one. 

    uint8_t ptSampleNum = 0; 
    for (int i = 0; i < 8 + totalWavetables * 2; i++) 
    {
        // If on the square wave samples for high note range, and they aren't needed  
        if (!doubleSQWSamples && i >= 4 && i <= 7) 
        {
            sampMap[i] = -1; // No PT sample needed for this square wave sample 
        }

        // If on the wavetable samples for high note range, and they aren't needed  
        if (!doubleWavetableSamples && i >= 7 + totalWavetables && i <= 11 + totalWavetables) 
        {
            sampMap[i] = -1; // No PT sample needed for this wavetable sample 
        }

        if (sampMap[i]) // If sample is used 
        {
            sampMap[i] = ptSampleNum + 1;  // Plus 1 because PT sample #0 is special.  
            ptSampleNum++; 
        }
        else // If sample is not used 
        {
            // No PT sample needed for this square wave sample or wavetable sample
            sampMap[i] = -1;  
        }
    }
    return ptSampleNum; // Return the number of PT samples that will be needed 
}

// Game Boy's range is:  C-1 -> C-8 (though in testing this, the range seems to be C-2 -> C-8 in Deflemask)
// ProTracker's range is:  C-1 -> B-3  (plus octaves 0 and 4 which are non-standard) 
//      This is C#3 -> C-5 in Deflemask for the square wave samples I'm using. 
//      ProTracker should have a finetune value of -8 so that a PT C-1 == a Deflemask C-3. 

// If I upsample current 32-length PT square wave samples to double length (64), then C-1 -> B-3 in PT will be C-2 -> B-4 in Deflemask. ("low note range")
// If I downsample current 32-length PT square wave samples to quarter length (8), then C-1 -> B-3 in PT will be C-5 -> B-7 in Deflemask. ("high note range")
//      ^^^ Assuming all samples have finetune value of -8.
//          If finetune == 0, then it would cover Deflemask's highest note for the GB (C-8), but not the lowest (C-2). 
// I would have to downsample wavetables in order to achieve notes of C-6 or higher. 
//      And in order to reach Deflemask's highest GB note (C-8), I would need to downsample the wavetables to 1/4 of the values it normally has.  

uint16_t proTrackerPeriodTable[5][12] = {
    {1712,1616,1525,1440,1357,1281,1209,1141,1077,1017, 961, 907},  /* C-0 to B-0 */
    {856,808,762,720,678,640,604,570,538,508,480,453},              /* C-1 to B-1 */
    {428,404,381,360,339,320,302,285,269,254,240,226},              /* C-2 to B-2 */
    {214,202,190,180,170,160,151,143,135,127,120,113},              /* C-3 to B-3 */
    {107,101, 95, 90, 85, 80, 76, 71, 67, 64, 60, 57}               /* C-4 to B-4 */
}; 


const uint16_t sqwSampleLength = 32;
const int8_t sqwSampleDuty[4][32] = {
    { 127,  127,  127,  127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128}, /* Duty cycle = 12.5% */ 
    { 127,  127,  127,  127,  127,  127,  127,  127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128}, /* Duty cycle = 25%   */ 
    { 127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128}, /* Duty cycle = 50%   */ 
    { 127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127, -128, -128, -128, -128, -128, -128, -128, -128}  /* Duty cycle = 75%   */ 
};
const char sqwSampleNames[4][22] = {"SQUARE - Duty 12.5\%   ", "SQUARE - Duty 25\%     ", "SQUARE - Duty 50\%     ", "SQUARE - Duty 75\%     "};


