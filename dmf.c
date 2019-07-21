
#include "dmf.h"

void importDMF(char *fname, DMFContents *dmf)
{
    // DMF file must be inflated first before calling this function.

    FILE *fin = fopen(fname, "rb");

    ///////////////// FORMAT FLAGS  
    char header[17]; 
    fgets(header, 17, fin); 
    if (strncmp(header, ".DelekDefleMask.", 17) == 0)
    {
        printf("Format header is good.\n");
    }
    else
    {
        printf(header);
        printf("Format header is bad.\n");
        exit(1); 
    }

    dmf->dmfFileVersion = fgetc(fin); 
    printf(".dmf File Version: %u\n", dmf->dmfFileVersion); 

    ///////////////// SYSTEM SET  
    dmf->sys = getSystem(fgetc(fin)); 
    printf("System: %s (channels: %u)\n", dmf->sys.name, dmf->sys.channels);

    ///////////////// VISUAL INFORMATION 
    loadVisualInfo(fin, dmf); 

    ///////////////// MODULE INFORMATION  
    loadModuleInfo(fin, dmf); 

    ///////////////// PATTERN MATRIX VALUES 
    loadPatternMatrixValues(fin, dmf); 

    ///////////////// INSTRUMENTS DATA 
    loadInstrumentsData(fin, dmf); 
    printf("Loaded instruments.\n");

    ///////////////// WAVETABLES DATA
    loadWavetablesData(fin, dmf); 
    printf("Loaded %u wavetable(s).\n", dmf->totalWavetables); 

    ///////////////// PATTERNS DATA
    loadPatternsData(fin, dmf); 
    printf("Loaded patterns.\n");

    ///////////////// PCM SAMPLES DATA
    loadPCMSamplesData(fin, dmf); 
    printf("Loaded PCM Samples.\nThe .dmf file has finished loading. \n");

    fclose(fin);

    printf("Done loading .dmf file!\n");

}

void loadVisualInfo(FILE *fin, DMFContents *dmf) 
{
    dmf->visualInfo.songNameLength = fgetc(fin);   
    dmf->visualInfo.songName = malloc(dmf->visualInfo.songNameLength); 
    fgets(dmf->visualInfo.songName, dmf->visualInfo.songNameLength + 1, fin); 
    //printf("len: %u\n", songNameLength);
    printf("Title: %s\n", dmf->visualInfo.songName);

    dmf->visualInfo.songAuthorLength = fgetc(fin);    
    dmf->visualInfo.songAuthor = malloc(dmf->visualInfo.songAuthorLength); 
    fgets(dmf->visualInfo.songAuthor, dmf->visualInfo.songAuthorLength + 1, fin); 
    //printf("len: %u\n", songAuthorLength);
    printf("Author: %s\n", dmf->visualInfo.songAuthor);

    dmf->visualInfo.highlightAPatterns = fgetc(fin);  
    dmf->visualInfo.highlightBPatterns = fgetc(fin); 
}

void loadModuleInfo(FILE *fin, DMFContents *dmf)
{
    dmf->moduleInfo.timeBase = fgetc(fin);   
    dmf->moduleInfo.tickTime1 = fgetc(fin); 
    dmf->moduleInfo.tickTime2 = fgetc(fin); 
    dmf->moduleInfo.framesMode = fgetc(fin); 
    dmf->moduleInfo.usingCustomHZ = fgetc(fin); 
    dmf->moduleInfo.customHZValue1 = fgetc(fin); 
    dmf->moduleInfo.customHZValue2 = fgetc(fin); 
    dmf->moduleInfo.customHZValue3 = fgetc(fin); 
    dmf->moduleInfo.totalRowsPerPattern = fgetc(fin); 
    dmf->moduleInfo.totalRowsPerPattern |= fgetc(fin) << 8;
    dmf->moduleInfo.totalRowsPerPattern |= fgetc(fin) << 16;
    dmf->moduleInfo.totalRowsPerPattern |= fgetc(fin) << 24;
    dmf->moduleInfo.totalRowsInPatternMatrix = fgetc(fin); 

    printf("timeBase: %u\n", dmf->moduleInfo.timeBase);    // In Def. it says 1, but here it gives 0.
    printf("tickTime1: %u\n", dmf->moduleInfo.tickTime1);  // Good 
    printf("tickTime2: %u\n", dmf->moduleInfo.tickTime2);  // Good 
    printf("framesMode: %u\n", dmf->moduleInfo.framesMode);  // If this is called "Step" in Def., then this is good 
    printf("usingCustomHZ: %u\n", dmf->moduleInfo.usingCustomHZ);    // Whether the "Custom" clock box is checked? 
    printf("customHZValue1: %u\n", dmf->moduleInfo.customHZValue1);  // Hz clock - 1st digit?
    printf("customHZValue2: %u\n", dmf->moduleInfo.customHZValue2);  // Hz clock - 2nd digit?
    printf("customHZValue3: %u\n", dmf->moduleInfo.customHZValue3);  // Hz clock - 3rd digit?

    printf("totalRowsPerPattern: %u\n", dmf->moduleInfo.totalRowsPerPattern);  // Says 64, which is what "Rows" is  
    printf("totalRowsInPatternMatrix: %u or %x\n", dmf->moduleInfo.totalRowsInPatternMatrix, dmf->moduleInfo.totalRowsInPatternMatrix); // Good. 

    // In previous .dmp versions, arpeggio tick speed is here! 

}

void loadPatternMatrixValues(FILE *fin, DMFContents *dmf) 
{
    // Format: patterMatrixValues[channel][pattern matrix row] 
    dmf->patternMatrixValues = (uint8_t **)malloc(dmf->sys.channels * sizeof(uint8_t *)); 
    dmf->patternMatrixMaxValues = (uint8_t *)malloc(dmf->sys.channels * sizeof(uint8_t)); 

    for (int i = 0; i < dmf->sys.channels; i++)
    {
        dmf->patternMatrixMaxValues[i] = 0; 
        dmf->patternMatrixValues[i] = (uint8_t *)malloc(dmf->moduleInfo.totalRowsInPatternMatrix * sizeof(uint8_t));
        for (int j = 0; j < dmf->moduleInfo.totalRowsInPatternMatrix; j++)
        {
            dmf->patternMatrixValues[i][j] = fgetc(fin); 
            if (dmf->patternMatrixValues[i][j] > dmf->patternMatrixMaxValues[i]) 
            {
                dmf->patternMatrixMaxValues[i] = dmf->patternMatrixValues[i][j]; 
            }
        }
    }
    
}

void loadInstrumentsData(FILE *fin, DMFContents *dmf)
{
    dmf->totalInstruments = fgetc(fin);
    dmf->instruments = (Instrument *)malloc(dmf->totalInstruments * sizeof(Instrument)); 

    for (int i = 0; i < dmf->totalInstruments; i++)
    {
        dmf->instruments[i] = loadInstrument(fin, dmf->sys); 
    }
}

void loadWavetablesData(FILE *fin, DMFContents *dmf) 
{
    dmf->totalWavetables = fgetc(fin); 
    
    dmf->wavetableSizes = (uint32_t *)malloc(dmf->totalWavetables * sizeof(uint32_t));     
    dmf->wavetableValues = (uint32_t **)malloc(dmf->totalWavetables * sizeof(uint32_t *)); 

    for (int i = 0; i < dmf->totalWavetables; i++)
    {
        dmf->wavetableSizes[i] = fgetc(fin); 
        dmf->wavetableSizes[i] |= fgetc(fin) << 8;
        dmf->wavetableSizes[i] |= fgetc(fin) << 16;
        dmf->wavetableSizes[i] |= fgetc(fin) << 24;

        dmf->wavetableValues[i] = (uint32_t *)malloc(dmf->wavetableSizes[i] * sizeof(uint32_t)); 
        for (int j = 0; j < dmf->wavetableSizes[i]; j++)
        {
            dmf->wavetableValues[i][j] = fgetc(fin); 
            dmf->wavetableValues[i][j] |= fgetc(fin) << 8;
            dmf->wavetableValues[i][j] |= fgetc(fin) << 16;
            dmf->wavetableValues[i][j] |= fgetc(fin) << 24;
        }
    }
}

void loadPatternsData(FILE *fin, DMFContents *dmf) 
{
    // patternValues[channel][pattern number][pattern row number]
    dmf->patternValues = (PatternRow ***)malloc(dmf->sys.channels * sizeof(PatternRow **)); 
    dmf->channelEffectsColumnsCount = (uint8_t *)malloc(dmf->sys.channels * sizeof(uint8_t)); 
    uint8_t patternMatrixNumber;

    for (int channel = 0; channel < dmf->sys.channels; channel++)
    {
        dmf->channelEffectsColumnsCount[channel] = fgetc(fin); 

        // Maybe use calloc instead of malloc in the line below?     
        dmf->patternValues[channel] = (PatternRow **)malloc((dmf->patternMatrixMaxValues[channel] + 1) * sizeof(PatternRow *));
        for (int i = 0; i < dmf->patternMatrixMaxValues[channel] + 1; i++) 
        {
            dmf->patternValues[channel][i] = NULL; 
        }

        for (int rowInPatternMatrix = 0; rowInPatternMatrix < dmf->moduleInfo.totalRowsInPatternMatrix; rowInPatternMatrix++)
        {
            patternMatrixNumber = dmf->patternMatrixValues[channel][rowInPatternMatrix];
            if (dmf->patternValues[channel][patternMatrixNumber] != NULL) // If pattern has been loaded previously 
            {
                fseek(fin, (8 + 4*dmf->channelEffectsColumnsCount[channel])*dmf->moduleInfo.totalRowsPerPattern, SEEK_CUR); // Unnecessary information
                continue; // Skip patterns that have already been loaded 
            }

            dmf->patternValues[channel][patternMatrixNumber] = (PatternRow *)malloc(dmf->moduleInfo.totalRowsPerPattern * sizeof(PatternRow));
            
            for (uint32_t row = 0; row < dmf->moduleInfo.totalRowsPerPattern; row++)
            {
                dmf->patternValues[channel][patternMatrixNumber][row] = loadPatternRow(fin, dmf->channelEffectsColumnsCount[channel]);
            }
        }
    }
}

void loadPCMSamplesData(FILE *fin, DMFContents *dmf)
{
    dmf->totalPCMSamples = fgetc(fin); 
    dmf->pcmSamples = (PCMSample *)malloc(dmf->totalPCMSamples * sizeof(PCMSample));

    for (int sample = 0; sample < dmf->totalPCMSamples; sample++) 
    {
        dmf->pcmSamples[sample] = loadPCMSample(fin);
    }

} 

void freeDMF(DMFContents *dmf) 
{
    free(dmf->visualInfo.songName); 
    free(dmf->visualInfo.songAuthor);  
    for (int channel = 0; channel < dmf->sys.channels; channel++)
    {
        for (int i = 0; i < dmf->patternMatrixMaxValues[channel] + 1; i++)
        {
            free(dmf->patternValues[channel][i]); 
        }
        free(dmf->patternValues[channel]); 
    }
    free(dmf->patternValues); 
    for (int i = 0; i < dmf->sys.channels; i++)
    {
        free(dmf->patternMatrixValues[i]); 
    }
    free(dmf->patternMatrixValues); 
    free(dmf->patternMatrixMaxValues); 
    for (int i = 0; i < dmf->totalInstruments; i++) 
    {
        free(dmf->instruments[i].name); 
        free(dmf->instruments[i].stdArpEnvValue); 
        free(dmf->instruments[i].stdDutyNoiseEnvValue); 
        free(dmf->instruments[i].stdVolEnvValue); 
        free(dmf->instruments[i].stdWavetableEnvValue);     
    }
    free(dmf->instruments); 
    free(dmf->wavetableSizes); 
    for (int i = 0; i < dmf->totalWavetables; i++)
    {
        free(dmf->wavetableValues[i]);
    }
    free(dmf->wavetableValues); 
    free(dmf->channelEffectsColumnsCount); 
    for (int sample = 0; sample < dmf->totalPCMSamples; sample++) 
    {
        free(dmf->pcmSamples[sample].name); 
        free(dmf->pcmSamples[sample].data); 
    }
    free(dmf->pcmSamples);
}

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

System systems[10] = {
    {.id = 0x00, .name = "ERROR", .channels = 0},
	{.id = 0x02, .name = "GENESIS", .channels = 10},
	{.id = 0x12, .name = "GENESIS_CH3", .channels = 13},
	{.id = 0x03, .name = "SMS", .channels = 4},
	{.id = 0x04, .name = "GAMEBOY", .channels = 4},
	{.id = 0x05, .name = "PCENGINE", .channels = 6},
	{.id = 0x06, .name = "NES", .channels = 5},
	{.id = 0x07, .name = "C64_SID_8580", .channels = 3},
	{.id = 0x17, .name = "C64_SID_6581", .channels = 3},
	{.id = 0x08, .name = "YM2151", .channels = 13}
};

System getSystem(uint8_t systemByte)
{
    for (int i = 1; i < 10; i++)
    {
        if (systems[i].id == systemByte) 
            return systems[i];
    }
    return systems[0]; // Error: System byte invalid  
}

Instrument loadInstrument(FILE *filePointer, System systemType)
{
    Instrument inst; 

    uint8_t name_size = fgetc(filePointer); 
    inst.name = (char *)malloc(name_size * sizeof(char)); 
    fgets(inst.name, name_size + 1, filePointer);

    inst.mode = fgetc(filePointer); // 1 = FM; 0 = Standard 
    
    if (inst.mode == 1) // FM instrument 
    {
        inst.fmALG = fgetc(filePointer); 
        inst.fmFB = fgetc(filePointer); 
        inst.fmLFO = fgetc(filePointer); 
        inst.fmLFO2 = fgetc(filePointer); 

        int TOTAL_OPERATORS = 1;  // I'm not sure what toal operators is or where I'm supposed to get it from 
        for (int i = 0; i < TOTAL_OPERATORS; i++)
        {
            inst.fmAM = fgetc(filePointer); 
            inst.fmAR = fgetc(filePointer); 
            inst.fmDR = fgetc(filePointer); 
            inst.fmMULT = fgetc(filePointer); 
            inst.fmRR = fgetc(filePointer); 
            inst.fmSL = fgetc(filePointer); 
            inst.fmTL = fgetc(filePointer); 
            inst.fmDT2 = fgetc(filePointer); 
            inst.fmRS = fgetc(filePointer); 
            inst.fmDT = fgetc(filePointer); 
            inst.fmD2R = fgetc(filePointer); 
            inst.fmSSGMODE = fgetc(filePointer); 
        }
    }
    else if (inst.mode == 0) // Standard instrument 
    {
        if (strcmp(systemType.name, "GAMEBOY") != 0)  // Not a GameBoy  
        {
            // Volume macro 
            inst.stdVolEnvSize = fgetc(filePointer); 
            inst.stdVolEnvValue = (int32_t *)malloc(inst.stdVolEnvSize * sizeof(int32_t));
            for (int i = 0; i < inst.stdVolEnvSize; i++)
            {
                // 4 bytes, little-endian 
                inst.stdVolEnvValue[i] = fgetc(filePointer); 
                inst.stdVolEnvValue[i] |= fgetc(filePointer) << 8;
                inst.stdVolEnvValue[i] |= fgetc(filePointer) << 16;
                inst.stdVolEnvValue[i] |= fgetc(filePointer) << 24;
            }
            if (inst.stdVolEnvSize > 0) 
                inst.stdVolEnvLoopPos = fgetc(filePointer); 
        }

        // Arpeggio macro 
        inst.stdArpEnvSize = fgetc(filePointer); 
        inst.stdArpEnvValue = (int32_t *)malloc(inst.stdArpEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdArpEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdArpEnvValue[i] = fgetc(filePointer); 
            inst.stdArpEnvValue[i] |= fgetc(filePointer) << 8;
            inst.stdArpEnvValue[i] |= fgetc(filePointer) << 16;
            inst.stdArpEnvValue[i] |= fgetc(filePointer) << 24;
        }

        if (inst.stdArpEnvSize > 0)
            inst.stdArpEnvLoopPos = fgetc(filePointer ); 
        inst.stdArpMacroMode = fgetc(filePointer); 

        // Duty/Noise macro 
        inst.stdDutyNoiseEnvSize = fgetc(filePointer); 
        inst.stdDutyNoiseEnvValue = (int32_t *)malloc(inst.stdDutyNoiseEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdDutyNoiseEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdDutyNoiseEnvValue[i] = fgetc(filePointer); 
            inst.stdDutyNoiseEnvValue[i] |= fgetc(filePointer) << 8;
            inst.stdDutyNoiseEnvValue[i] |= fgetc(filePointer) << 16;
            inst.stdDutyNoiseEnvValue[i] |= fgetc(filePointer) << 24;
        }
        if (inst.stdDutyNoiseEnvSize > 0) 
            inst.stdDutyNoiseEnvLoopPos = fgetc(filePointer); 

        // Wavetable macro 
        inst.stdWavetableEnvSize = fgetc(filePointer); 
        inst.stdWavetableEnvValue = (int32_t *)malloc(inst.stdWavetableEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdWavetableEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdWavetableEnvValue[i] = fgetc(filePointer); 
            inst.stdWavetableEnvValue[i] |= fgetc(filePointer) << 8;
            inst.stdWavetableEnvValue[i] |= fgetc(filePointer) << 16;
            inst.stdWavetableEnvValue[i] |= fgetc(filePointer) << 24;
        }
        if (inst.stdWavetableEnvSize > 0) 
            inst.stdWavetableEnvLoopPos = fgetc(filePointer); 

        // Per system data
        if (strcmp(systemType.name, "C64_SID_8580") == 0 || strcmp(systemType.name, "C64_SID_6581") == 0) // Using Commodore 64 
        {
            
            inst.stdC64TriWaveEn = fgetc(filePointer); 
            inst.stdC64SawWaveEn = fgetc(filePointer); 
            inst.stdC64PulseWaveEn = fgetc(filePointer); 
            inst.stdC64NoiseWaveEn = fgetc(filePointer); 
            inst.stdC64Attack = fgetc(filePointer); 
            inst.stdC64Decay = fgetc(filePointer); 
            inst.stdC64Sustain = fgetc(filePointer); 
            inst.stdC64Release = fgetc(filePointer); 
            inst.stdC64PulseWidth = fgetc(filePointer); 
            inst.stdC64RingModEn = fgetc(filePointer); 
            inst.stdC64SyncModEn = fgetc(filePointer); 
            inst.stdC64ToFilter = fgetc(filePointer); 
            inst.stdC64VolMacroToFilterCutoffEn = fgetc(filePointer); 
            inst.stdC64UseFilterValuesFromInst = fgetc(filePointer); 
            
            // Filter globals 
            inst.stdC64FilterResonance = fgetc(filePointer); 
            inst.stdC64FilterCutoff = fgetc(filePointer); 
            inst.stdC64FilterHighPass = fgetc(filePointer); 
            inst.stdC64FilterLowPass = fgetc(filePointer); 
            inst.stdC64FilterCH2Off = fgetc(filePointer); 
        }
        else if (strcmp(systemType.name, "GAMEBOY") == 0) // Using GameBoy 
        {
            inst.stdGBEnvVol = fgetc(filePointer); 
            inst.stdGBEnvDir = fgetc(filePointer); 
            inst.stdGBEnvLen = fgetc(filePointer); 
            inst.stdGBSoundLen = fgetc(filePointer); 
        }
    }

    return inst; 
}

PCMSample loadPCMSample(FILE *filePointer)
{
    PCMSample sample; 

    sample.size = fgetc(filePointer);
    sample.size |= fgetc(filePointer) << 8;
    sample.size |= fgetc(filePointer) << 16;
    sample.size |= fgetc(filePointer) << 24;

    uint8_t name_size = fgetc(filePointer); 
    sample.name = (char *)malloc(name_size * sizeof(char)); 
    fgets(sample.name, name_size + 1, filePointer);

    sample.rate = fgetc(filePointer); 
    sample.pitch = fgetc(filePointer); 
    sample.amp = fgetc(filePointer); 
    sample.bits = fgetc(filePointer); 

    sample.data = (uint16_t *)malloc(sample.size * sizeof(uint16_t *)); 
    for (uint32_t i = 0; i < sample.size; i++) 
    {
        sample.data[i] = fgetc(filePointer); 
        sample.data[i] |= fgetc(filePointer) << 8; 
    }

    return sample; 
}

