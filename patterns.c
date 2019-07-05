
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

