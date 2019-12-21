/*
mod.c
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Provides functions for exporting the contents of a .dmf file 
to ProTracker's .mod format. 

Several limitations apply in order to export. For example, the 
.dmf file must use the GameBoy system, patterns must have 64 
rows, only one effect column is allowed per channel, etc.  
*/

#include "mod.h"

int exportMOD(char *fname, DMFContents *dmf) 
{
    FILE *fout;
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

    if (strcmp(dmf->sys.name, "GAMEBOY") != 0) // If it's not a GameBoy 
    {
        printf("Sorry. Only the GameBoy system is currently supported. \n");
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
    
    // Export samples (blank, but could add four square wave samples with difference duty cycles)
    
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

    // The 27 remaining samples are blank: 
    for (int i = 0; i < 27; i++) 
    {
        // According to real ProTracker files viewed in a hex viewer, the 30th and final byte
        //    of a blank sample is 0x01 and all 29 other bytes are 0x00. 
        for (int j = 0; j < 29; j++) 
        {
            fputc(0, fout); 
        }
        fputc(1, fout); 
    }

    fputc(dmf->moduleInfo.totalRowsInPatternMatrix, fout);   // Song length in patterns (not total number of patterns) 
    fputc(127, fout);                        // 0x7F - Useless byte that has to be here 

    int8_t duplicateIndices = 0;
    // The function below tries to find repeating rows of patterns in the pattern matrix so that fewer 
    //   ProTracker patterns are needed. This could allow some .dmf files to successfully be converted 
    //   to .mod that wouldn't otherwise. It also assigns the ProTracker pattern indices. 
    duplicateIndices = getProTrackerRepeatPatterns(dmf);
    if (patternMatrixRowToProTrackerPattern == NULL || dmf->moduleInfo.totalRowsInPatternMatrix - duplicateIndices > 64) 
    {
        printf("Error: Too many unique rows of patterns in the pattern matrix. 64 is the maximum.\n");
        return 1;
    }
    printf("duplicateIndices=%u\n", duplicateIndices); 

    fwrite(patternMatrixRowToProTrackerPattern, 1, 128, fout);
    
    fwrite("M.K.", 1, 4, fout);  // ProTracker uses "M!K!" if there's more than 64 pattern matrix rows...

    printf("Exporting pattern data...\n");

    for (int channel = 0; channel < dmf->sys.channels; channel++)
    {
        if (dmf->channelEffectsColumnsCount[channel] > 1) 
        {
            printf("Error: Each channel can only have 1 effects column.\n");
            return 1;
        }
        if (dmf->patternMatrixMaxValues[channel] > 63) 
        {
            printf("Too many patterns. The maximum is 64 unique rows in the pattern matrix.\n", channel); 
            return 1;
        }
    }

    uint8_t currentDutyCycle[4] = {1,1,1,1}; // Default is 1 or a 12.5% duty cycle square wave. In mod, the first sample is sample #1.  
    int8_t pat_mat_row = -1; 

    printf("dmf->moduleInfo.totalRowsInPatternMatrix - duplicateIndices = %u\n", dmf->moduleInfo.totalRowsInPatternMatrix - duplicateIndices);

    // Iterate through ProTracker patterns:  
    for (int i = 0; i < dmf->moduleInfo.totalRowsInPatternMatrix - duplicateIndices; i++)  
    {
        // Get the Deflemask pattern matrix number from the ProTracker pattern number 
        pat_mat_row = proTrackerPatternToPatternMatrixRow[i];
        printf("pat_mat_row = %u\n", pat_mat_row); 
        if (pat_mat_row == -1) 
        {
            printf("Error in proTrackerPatternToPatternMatrixRow.\n");
            return 1; 
            /*
            for (int j = 0; j < 64*dmf->sys.channels; j++) 
            {
                fputc(0, fout);
            }
            continue; 
            */
        }
        // Iterate through rows in a pattern: 
        for (int j = 0; j < 64; j++) 
        {
            for (int k = 0; k < dmf->sys.channels; k++) 
            {
                if (dmf->patternValues[k][dmf->patternMatrixValues[k][pat_mat_row]][j].effectCode[0] == DMF_SETDUTYCYCLE) 
                {
                    currentDutyCycle[k] = dmf->patternValues[k][dmf->patternMatrixValues[k][pat_mat_row]][j].effectValue[0] + 1; // Add 1 b/c first mod sample is sample #1 
                }
                if (writeProTrackerPatternRow(fout, &dmf->patternValues[k][dmf->patternMatrixValues[k][pat_mat_row]][j], currentDutyCycle[k])) 
                {
                    // Error occurred while writing the pattern row 
                    return 1; 
                }
            }
        }
    }
    
    // Samples 
    printf("Exporting samples...\n"); 

    for (int i = 0; i < 4; i++) 
    {
        //fputc(0, fout); // Repeat information 
        //fputc(0, fout); // Repeat information 

        fwrite(sqwSampleDuty[i], 1, sqwSampleLength, fout); 

    }

    free(proTrackerPatternToPatternMatrixRow); 
    free(patternMatrixRowToProTrackerPattern); 

    fclose(fout); 

    printf("Done exporting to .mod!\n");

    return 0; // Success 
}


int8_t getProTrackerRepeatPatterns(DMFContents *dmf) 
{
    // Returns the number of groups of pattern matrix rows that are identical. For 
    //    example, if rows 1, 7, and 9 are identical to each other and no other 
    //    rows have matches, then it will return 1, because there was one group of
    //    identical rows.  
    // Assumes dmf->moduleInfo.totalRowsInPatternMatrix is 128 or fewer.

    // A hash table would be a better way of doing this, but since this is 
    //    only for 128 or fewer elements, I'm not going to bother implementing one. 
    
    if (dmf->moduleInfo.totalRowsInPatternMatrix > 128 || dmf->moduleInfo.totalRowsInPatternMatrix < 1) 
    {
        return 0; 
    }

    uint8_t **patMatVal = dmf->patternMatrixValues; 
    
    if (patternMatrixRowToProTrackerPattern == NULL) 
    {
        patternMatrixRowToProTrackerPattern = (int8_t *)malloc(128 * sizeof(int8_t));  // maps Deflemask indices to PT indices
        if (patternMatrixRowToProTrackerPattern) 
        {
            memset(patternMatrixRowToProTrackerPattern, (int8_t)-1, dmf->moduleInfo.totalRowsInPatternMatrix);
            memset(patternMatrixRowToProTrackerPattern + dmf->moduleInfo.totalRowsInPatternMatrix, (int8_t)0, 128 - dmf->moduleInfo.totalRowsInPatternMatrix);
        }
        else
        {
            return 0; 
        }
    }

    if (proTrackerPatternToPatternMatrixRow == NULL)
    {
        proTrackerPatternToPatternMatrixRow = (int8_t *)malloc(128 * sizeof(int8_t));
        if (proTrackerPatternToPatternMatrixRow) 
        {
            memset(proTrackerPatternToPatternMatrixRow, (int8_t)-1, 128);
        }
        else
        {
            return 0; 
        }
    }

    // Unnecessary but a shorter name is nice: 
    int8_t *r2pt = patternMatrixRowToProTrackerPattern;  
    int8_t *pt2r = proTrackerPatternToPatternMatrixRow;  

    if (dmf->moduleInfo.totalRowsInPatternMatrix == 1) 
    {
        r2pt[0] = 0; // ? 
        pt2r[0] = 0; // ?
        printf("DEBUG: patternMatrixRowToProTrackerPattern[0] = %x", patternMatrixRowToProTrackerPattern[0]);
        printf("DEBUG: proTrackerPatternToPatternMatrixRow[0] = %x", proTrackerPatternToPatternMatrixRow[0]);
        return 0; 
    }

    uint8_t currentProTrackerIndex = 0; 
    uint8_t duplicateCount = 0; 
    for (int i = 0; i < dmf->moduleInfo.totalRowsInPatternMatrix; i++) 
    {
        if (r2pt[i] >= 0) {continue; }  // Duplicate that has already been found 
        for (int j = i + 1; j < dmf->moduleInfo.totalRowsInPatternMatrix; j++) 
        {
            //if (d2pt[j] >= 0) {continue; }  // Duplicate that has already been found 
            
            if (patMatVal[0][i] == patMatVal[0][j]
             && patMatVal[1][i] == patMatVal[1][j]
              && patMatVal[2][i] == patMatVal[2][j]
               && patMatVal[3][i] == patMatVal[3][j])
            {
                r2pt[i] = currentProTrackerIndex; 
                r2pt[j] = currentProTrackerIndex; 
                pt2r[currentProTrackerIndex] = i;
                //printf("pt2r[%u] = %u\n", currentProTrackerIndex, i); 
                duplicateCount++; 
            }
        }
        if (r2pt[i] < 0) // This row has no duplicate 
        {
            r2pt[i] = currentProTrackerIndex; 
            pt2r[currentProTrackerIndex] = i; 
            //printf("pt2r[%u] = %u\n", currentProTrackerIndex, i); 
        }

        currentProTrackerIndex++;
    }

    return duplicateCount; 
}

// Unfinished function 
int writeProTrackerPatternRow(FILE *fout, PatternRow *pat, uint8_t dutyCycle) 
{
    if (pat->note == DMF_NOTE_EMPTY)  // No note is playing. Only handle effects.
    {
        uint16_t effect = getProTrackerEffect(pat->effectCode[0], pat->effectValue[0]); //pat->effectCode;
        fputc(0, fout);  // Sample number (upper 4b) = 0 b/c there's no note; sample period/effect param. (upper 4b) = 0 b/c there's no note
        fputc(0, fout);         // Sample period/effect param. (lower 8 bits) 
        fputc((effect & 0x0F00) >> 8, fout);  // Sample number (lower 4b) = 0 b/c there's no note; effect code (upper 4b)
        fputc(effect & 0x00FF, fout);         // Effect code (lower 8 bits) 
    }
    else  // A note is playing 
    {
        uint16_t period = 0;

        uint16_t modOctave = pat->octave - 2; // Transpose down two octaves because a C-4 in Deflemask is the same pitch as a C-2 in ProTracker. 
        if (pat->note == DMF_NOTE_C) // C# is the start of next octave in .mod, not C- 
        {
            modOctave++; 
        }

        if (modOctave > 4)
        {
            printf("Error: Octave must be 4 or less. (Octave = %u)\n", modOctave);
            //return 1; 
            modOctave = 4; // !!!
        }

        if (pat->note >= 1 && pat->note <= 12)  // A note 
        {
            period = proTrackerPeriodTable[modOctave][pat->note % 12]; 
        }
        // handle other note values here 
        
        uint16_t effect = getProTrackerEffect(pat->effectCode[0], pat->effectValue[0]); 
        
        fputc((dutyCycle & 0xF0) | ((period & 0x0F00) >> 8), fout);  // Sample number (upper 4b); sample period/effect param. (upper 4b)
        fputc(period & 0x00FF, fout);                                // Sample period/effect param. (lower 8 bits) 
        fputc((dutyCycle << 4) | ((effect & 0x0F00) >> 8), fout);    // Sample number (lower 4b); effect code (upper 4b)
        fputc(effect & 0x00FF, fout);                                // Effect code (lower 8 bits) 
        
    }
    
    /*
    7654-3210 7654-3210 7654-3210 7654-3210
    wwww xxxx-xxxx-xxxx yyyy zzzz-zzzz-zzzz

        wwwwyyyy (8 bits) is the sample for this channel/division
    xxxxxxxxxxxx (12 bits) is the sample's period (or effect parameter)
    zzzzzzzzzzzz (12 bits) is the effect for this channel/division
    */
   return 0; // Success 
}


uint16_t getProTrackerEffect(int16_t effectCode, int16_t effectValue)
{
    // An effect is represented with 12 bits, which is 3 groups of 4 bits: [e][x][y]. 
    // The effect code is [e] or [e][x], and the effect value is [x][y] or [y]. 
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
            ptEff = PT_POSJUMP; break;
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
        case DMF_NOTEDELAY:
            ptEff = PT_DELAYSAMPLE; break; // ? 
        case DMF_SYNCSIGNAL:
            break; // This is only used when exporting as .vgm in Deflemask 
        case DMF_SETGLOBALFINETUNE:
            ptEff = PT_SETFINETUNE; break; // ? 
        case DMF_SETSPEEDVAL2:
            break; // ? 

        // GameBoy exclusive: 
        case DMF_SETWAVE:
            break; // Need to handle this in the exportMOD function  !!!
        case DMF_SETNOISEPOLYCOUNTERMODE:
            break; // This is probably more than I need to worry about 
        case DMF_SETDUTYCYCLE:
            break; // This is handled in the exportMOD function 
        case DMF_SETSWEEPTIMESHIFT:
            break; // ? 
        case DMF_SETSWEEPDIR: 
            break; // ? 
    }

    return ((uint16_t)ptEff << 4) | ptEffVal; 
}

// GameBoy's range is:  C-1 -> C-8
// ProTracker's range is:  C-1 -> B-3  (plus octaves 0 and 4 which are non-standard) 
uint16_t proTrackerPeriodTable[5][12] = {
    {1712,1616,1525,1440,1357,1281,1209,1141,1077,1017, 961, 907},  /* C-0 to B-0 */
    {856,808,762,720,678,640,604,570,538,508,480,453},              /* C-1 to B-1 */
    {428,404,381,360,339,320,302,285,269,254,240,226},              /* C-2 to B-2 */
    {214,202,190,180,170,160,151,143,135,127,120,113},              /* C-3 to B-3 */
    {107,101, 95, 90, 85, 80, 76, 71, 67, 64, 60, 57}               /* C-4 to B-4 */
}; 


const uint16_t sqwSampleLength = 32;
const int8_t sqwSampleDuty[4][32] = {
    {127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* Duty cycle = 12.5% */ 
    {127, 127, 127, 127, 127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* Duty cycle = 25% */ 
    {127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* Duty cycle = 50% */ 
    {127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0} /* Duty cycle = 75% */ 
};
const char sqwSampleNames[4][22] = {"SQUARE - Duty 12.5\%   ", "SQUARE - Duty 25\%     ", "SQUARE - Duty 50\%     ", "SQUARE - Duty 75\%     "};


