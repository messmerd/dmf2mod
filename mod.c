
#include "mod.h"

void exportMOD(char *fname, DMFContents *dmf) 
{
    printf("Starting to export to .mod....\n");

    if (strcmp(dmf->sys.name, "GAMEBOY") != 0) // If it's not a GameBoy 
    {
        printf("Sorry. Only the GameBoy system is currently supported. \n");
        exit(1);
    }

    if (dmf->moduleInfo.totalRowsInPatternMatrix > 128) // totalRowsInPatternMatrix is 1 more than it actually is 
    {
        printf("Error: There must be 128 or fewer rows in the pattern matrix.\n");
        exit(1);
    }

    if (dmf->moduleInfo.totalRowsPerPattern != 64) 
    {
        printf("Error: Patterns must have 64 rows. \n");
        exit(1);
    }

    FILE *fout = fopen(fname, "wb");
    

    if (dmf->visualInfo.songNameLength > 20) 
    {
        fwrite(dmf->visualInfo.songName, 1, 20, fout); 
        printf("Song name is longer than 20 characters and will be truncated.\n");
        printf("New title: %.20s\n", dmf->visualInfo.songName); 
        
    } 
    else
    {
        fputs(dmf->visualInfo.songName, fout);  // Can include lowercase letters. Does ProTracker allow lowercase? 
        if (dmf->visualInfo.songNameLength < 20) 
        {
            for (int i = dmf->visualInfo.songNameLength; i < 20; i++) 
            {
                fputc(0, fout);
            }
        }
    }
    
    // Export samples (blank, but could add four square wave samples with difference duty cycles)
    
    // Export 4 square wave samples 
    for (int i = 0; i < 4; i++)
    {
        fwrite(sqwSampleNames[i], 1, 22, fout);      // Sample i+1 - 22B - name 
        fputc(sqwSampleLength >> 8, fout);           // Sample i+1 - 1B - length byte 0 - 0
        fputc(sqwSampleLength | 0x00FF, fout);       // Sample i+1 - 1B - length byte 1 - 64
        fputc(0, fout);                              // Sample i+1 - 1B - finetune value - 0 
        fputc(64, fout);                             // Sample i+1 - 1B - volume - full volume
        fputc(0 , fout);                             // Sample i+1 - 1B - repeat offset byte 0 
        fputc(0 , fout);                             // Sample i+1 - 1B - repeat offset byte 1 
        fputc(sqwSampleLength >> 8, fout);           // Sample i+1 - 1B - sample repeat length byte 0 - 0
        fputc(sqwSampleLength | 0x00FF, fout);       // Sample i+1 - 1B - sample repeat length byte 1 - 64
    }

    // The 27 remaining samples are blank: 
    for (int i = 0; i < 27*30; i++) 
    {
        fputc(0, fout); 
    }

    fputc(dmf->moduleInfo.totalRowsInPatternMatrix, fout);   // Song length in patterns  
    fputc(127, fout);                        // Useless byte that has to be here 

    int8_t duplicateIndices = 0;
    // The function below tries to find repeating rows of patterns in the pattern matrix so that fewer 
    //   ProTracker patterns are needed. This could allow some .dmf files to successfully be converted 
    //   to .mod that wouldn't otherwise. It also assigns the ProTracker pattern indices. 
    duplicateIndices = getProTrackerRepeatPatterns(dmf);
    if (patternMatrixRowToProTrackerPattern == NULL || dmf->moduleInfo.totalRowsInPatternMatrix - duplicateIndices > 64) 
    {
        printf("Error: Too many unique rows of patterns in the pattern matrix. 64 is the maximum.\n");
        exit(1);
    }

    fwrite(patternMatrixRowToProTrackerPattern, 1, 128, fout);

    fprintf(fout, "M.K."); 

    printf("Exporting pattern data...\n");

    for (int channel = 0; channel < dmf->sys.channels; channel++)
    {
        if (dmf->channelEffectsColumnsCount[channel] > 1) 
        {
            printf("Error: Each channel can only have 1 effects column.\n");
            //exit(1);
        }
        if (dmf->patternMatrixMaxValues[channel] > 63) 
        {
            printf("Too many patterns. The maximum is 64 unique rows in the pattern matrix.\n", channel); 
            exit(1); 
        }
    }

    uint8_t currentDutyCycle[4] = {0,0,0,0}; 
    int8_t pat_mat_row = -1; 

    for (int i = 0; i < dmf->moduleInfo.totalRowsInPatternMatrix; i++)  
    {
        //printf("i=%u\n", i);
        pat_mat_row = proTrackerPatternToPatternMatrixRow[i];
        //printf("  pat_mat_row=%u\n", pat_mat_row);

        if (pat_mat_row == -1) 
        {
            for (int j = 0; j < 64*dmf->sys.channels; j++) 
            {
                fputc(0, fout);
            }
            continue; 
        }
   
        for (int j = 0; j < 64; j++) 
        {
            //printf("j=%u, ", j);
            for (int k = 0; k < dmf->sys.channels; k++) 
            {
                //printf("k=%u", k);
                if (dmf->patternValues[k][dmf->patternMatrixValues[k][pat_mat_row]][j].effectCode[0] == SETDUTYCYCLE) 
                {
                    currentDutyCycle[k] = dmf->patternValues[k][dmf->patternMatrixValues[k][pat_mat_row]][j].effectValue[0]; 
                }
                //printf("...: ");
                writeProTrackerPatternRow(fout, &dmf->patternValues[k][dmf->patternMatrixValues[k][pat_mat_row]][j], currentDutyCycle[k]);
            }
        }
        
    }
    
    // Samples 
    printf("Exporting samples...\n"); 

    for (int i = 0; i < 4; i++) 
    {
        fputc(0, fout); // Repeat information 

        for (int j = 0; j < sqwSampleLength; j++) 
        {
            fwrite(sqwSampleDuty[i], 1, sqwSampleLength, fout); 
        }
    }


    printf("Done exporting to .mod!\n");

    free(proTrackerPatternToPatternMatrixRow); 
    free(patternMatrixRowToProTrackerPattern); 

    fclose(fout); 

}


int8_t getProTrackerRepeatPatterns(DMFContents *dmf) 
{
    // Return array of indices of pattern matrix rows that are identical 
    // Assumes dmf->moduleInfo.totalRowsInPatternMatrix is 128 or fewer. And > 1. 

    // A hash table would be a better way of doing this, but since this is 
    //    only for 128 or fewer elements, I'm not going to bother implementing one. 
    
    if (dmf->moduleInfo.totalRowsInPatternMatrix <= 1 || dmf->moduleInfo.totalRowsInPatternMatrix > 128) 
    {
        return 0; 
    }

    uint8_t **patMatVal = dmf->patternMatrixValues; 
    
    if (patternMatrixRowToProTrackerPattern == NULL) 
    {
        patternMatrixRowToProTrackerPattern = (int8_t *)malloc(128 * sizeof(int8_t));  // maps Deflemask indices to PT indices
        if (patternMatrixRowToProTrackerPattern) 
        {
            memset(patternMatrixRowToProTrackerPattern, (int8_t)-1, 128);
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

    uint8_t currentProTrackerIndex = 0; 
    uint8_t duplicateCount = 0; 
    for (int i = 0; i < dmf->moduleInfo.totalRowsInPatternMatrix - 1; i++) 
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
                duplicateCount++; 
            }
        }
        if (r2pt[i] < 0) // This row has no duplicate 
        {
            r2pt[i] = currentProTrackerIndex; 
            pt2r[currentProTrackerIndex] = i; 
        }

        currentProTrackerIndex++;
    }

    return duplicateCount; 
}

// Unfinished function 
void writeProTrackerPatternRow(FILE *fout, PatternRow *pat, uint8_t dutyCycle) 
{

    if (pat->octave > 4)
    {
        printf("Error: Octave must be 4 or less.\n");
    }

    //printf("start func...");
    int8_t bytes[4];
    uint16_t period = 0;
    if (pat->note >= 1 && pat->note <= 12)  // A note 
    {
        period = proTrackerPeriodTable[pat->octave][pat->note % 12]; 
    }
    // handle other note values here 

    uint16_t effect = 0; //pat->effectCode; 

    fputc((dutyCycle & 0xF0) | ((period & 0x0F00) >> 8), fout);  // Sample number (upper 4b); sample period/effect param. (upper 4b)
    fputc(period & 0x00FF, fout);                                // Sample period/effect param. (lower 8 bits) 
    fputc((dutyCycle << 4) | ((effect & 0x0F00) >> 8), fout);    // Sample number (lower 4b); effect code (upper 4b)
    fputc(effect & 0x00FF, fout);                                // Effect code (lower 8 bits) 

    /*
    7654-3210 7654-3210 7654-3210 7654-3210
    wwww xxxx-xxxx-xxxx yyyy zzzz-zzzz-zzzz

        wwwwyyyy (8 bits) is the sample for this channel/division
    xxxxxxxxxxxx (12 bits) is the sample's period (or effect parameter)
    zzzzzzzzzzzz (12 bits) is the effect for this channel/division
    */
   //printf("end func.\n");
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


const int16_t sqwSampleLength = 32;
const int8_t sqwSampleDuty[4][32] = {
    {127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* Duty cycle = 12.5% */ 
    {127, 127, 127, 127, 127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* Duty cycle = 25% */ 
    {127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* Duty cycle = 50% */ 
    {127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0} /* Duty cycle = 75% */ 
};
const char sqwSampleNames[4][22] = {"SQUARE - Duty 12.5\%   ", "SQUARE - Duty 25\%     ", "SQUARE - Duty 50\%     ", "SQUARE - Duty 75\%     "};


