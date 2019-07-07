
#include "patterns.h"

PatternRow loadPatternRow(FILE *filePointer, int effectsColumnsCount)
{
    PatternRow pat; 

    pat.note = fgetc(filePointer); 
    pat.note |= fgetc(filePointer) << 8; 
    pat.octave = fgetc(filePointer); 
    pat.octave |= fgetc(filePointer) << 8; 

    if (pat.octave > 4)
    {
        printf("Error: Octave must be 4 or less.\n");
    }

    pat.volume = fgetc(filePointer); 
    pat.volume |= fgetc(filePointer) << 8; 

    for (int col = 0; col < effectsColumnsCount; col++)
    {
        pat.effectCode[col] = fgetc(filePointer); 
        pat.effectCode[col] |= fgetc(filePointer) << 8; 
        pat.effectValue[col] = fgetc(filePointer); 
        pat.effectValue[col] |= fgetc(filePointer) << 8; 
    }

    pat.instrument = fgetc(filePointer); 
    pat.instrument |= fgetc(filePointer) << 8; 

    return pat;
}


int8_t getProTrackerRepeatPatterns(uint8_t **patMatVal, int totalRows)
{
    // Return array of indices of pattern matrix rows that are identical 
    // Assumes totalRows (of pattern matrix) is 128 or fewer. And > 1. 

    // A hash table would be a better way of doing this, but since this is 
    //    only for 128 or fewer elements, I'm not going to bother implementing one. 
    
    if (totalRows <= 1 || totalRows > 128) 
    {
        return 0; 
    }
    
    if (deflemaskToProTrackerIndices == NULL) 
    {
        deflemaskToProTrackerIndices = (int8_t *)malloc(128 * sizeof(int8_t));  // maps Deflemask indices to PT indices
        if (deflemaskToProTrackerIndices) 
        {
            memset(deflemaskToProTrackerIndices, (int8_t)-1, 128);
        }
        else
        {
            return 0; 
        }
    }

    // Unnecessary but a shorter name is nice: 
    int8_t *d2pt = deflemaskToProTrackerIndices;  

    uint8_t currentProTrackerIndex = 0; 
    uint8_t duplicateCount = 0; 
    for (int i = 0; i < totalRows - 1; i++) 
    {
        if (d2pt[i] >= 0) {continue; }  // Duplicate that has already been found 
        for (int j = i + 1; j < totalRows; j++) 
        {
            //if (d2pt[j] >= 0) {continue; }  // Duplicate that has already been found 
            
            if (patMatVal[0][i] == patMatVal[0][j]
             && patMatVal[1][i] == patMatVal[1][j]
              && patMatVal[2][i] == patMatVal[2][j]
               && patMatVal[3][i] == patMatVal[3][j])
            {
                d2pt[i] = currentProTrackerIndex; 
                d2pt[j] = currentProTrackerIndex; 
                duplicateCount++; 
            }
        }
        if (d2pt[i] < 0) // This row has no duplicate 
        {
            d2pt[i] = currentProTrackerIndex; 
        }

        currentProTrackerIndex++;
    }

    return duplicateCount; 
}

// Unfinished function 
void writeProTrackerPatternRow(PatternRow *pat)
{
    uint16_t period = 0;
    if (pat->note >= 1 && pat->note <= 12)  // A note 
    {
        period = proTrackerPeriodTable[(*pat).octave][(*pat).note % 12]; 
    }
    // handle other note values here 


    if (pat->effectCode[0] == SETDUTYCYCLE) 
    {
        
    }

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
