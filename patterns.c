
#include "patterns.h"

PatternRow loadPatternRow(FILE *filePointer, int effectsColumnsCount)
{
    PatternRow pat; 

    pat.note = fgetc(filePointer); 
    pat.note |= fgetc(filePointer) << 8; 
    pat.octave = fgetc(filePointer); 
    pat.octave |= fgetc(filePointer) << 8; 
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

    if (proTrackerToDeflemaskIndices == NULL)
    {
        proTrackerToDeflemaskIndices = (uint8_t *)calloc(128, sizeof(uint8_t));  // maps PT indices to Deflemask indices 
    }
    
    if (deflemaskToProTrackerIndices == NULL) 
    {
        deflemaskToProTrackerIndices = (uint8_t *)calloc(128, sizeof(uint8_t));  // maps Deflemask indices to PT indices
    }

    // Unnecessary but shorter names are nice: 
    uint8_t *d2pt = deflemaskToProTrackerIndices;  
    uint8_t *pt2d = proTrackerToDeflemaskIndices;  

    pt2d[0] = 0; 
    uint8_t currentProTrackerIndex = 1; 
    uint8_t duplicateCount = 0; 
    for (int i = 0; i < totalRows - 1; i++) 
    {
        if (d2pt[i] != 0) {continue; }  // Duplicate that has already been found 
        for (int j = i + 1; j < totalRows; j++) 
        {
            if (d2pt[j] != 0) {continue; }  // Duplicate that has already been found 

            if (patMatVal[0][i] == patMatVal[0][j]
             && patMatVal[1][i] == patMatVal[1][j]
              && patMatVal[2][i] == patMatVal[2][j]
               && patMatVal[3][i] == patMatVal[3][j])
            {
                pt2d[currentProTrackerIndex] = i + 1; // One more than actual index b/c 0 is used as a magic number in this loop
                d2pt[i] = currentProTrackerIndex; 
                d2pt[j] = currentProTrackerIndex; 
                duplicateCount++; 
            }
        }
        if (d2pt[i+1] == 0) // This row has no duplicate 
        {
            pt2d[currentProTrackerIndex] = i+1;
        }

        currentProTrackerIndex++;
    }

    return duplicateCount; 
}

