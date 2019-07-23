/*
dmf.c
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Provides functions for loading a .dmf file according to the 
spec sheet at http://www.deflemask.com/DMF_SPECS.txt. 

Requires zlib1.dll from the zlib compression library at https://zlib.net. 
*/

#include "dmf.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

/* Decompress from file source to buffer dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading the file. */
int inf(FILE *source, uint8_t **dest) 
{
    int ret;
    unsigned have;
    z_stream strm;
    uint8_t in[CHUNK];
    uint8_t out[CHUNK];
    *dest = malloc(CHUNK * sizeof(uint8_t)); 
    uint32_t current_pos = 0; 
    uint32_t current_size = CHUNK; 

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            
            // Reallocate if more room is needed. 
            // To-do: Read file size and allocate correct amount from the start? 
            if (current_pos + have > current_size) 
            {
                *dest = realloc(*dest, 2*current_size * sizeof(uint8_t)); 
                current_size *= 2; 
            }
            memcpy(&((*dest)[current_pos]), out, have);   
            current_pos += have; 
        } while (strm.avail_out == 0);
        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);
    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

// Report a zlib or I/O error 
void zerr(int ret)
{
    fputs("zpipe: ", stderr);
    switch (ret) {
    case Z_ERRNO:
        if (ferror(stdin))
            fputs("Error reading stdin\n", stderr);
        if (ferror(stdout))
            fputs("Error writing stdout\n", stderr);
        break;
    case Z_STREAM_ERROR:
        fputs("Invalid compression level\n", stderr);
        break;
    case Z_DATA_ERROR:
        fputs("Invalid or incomplete deflate data\n", stderr);
        break;
    case Z_MEM_ERROR:
        fputs("Out of memory\n", stderr);
        break;
    case Z_VERSION_ERROR:
        fputs("zlib version mismatch!\n", stderr);
    }
}

// Information about all the systems Deflemask supports 
const System Systems[10] = {
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

void importDMF(const char *fname, DMFContents *dmf)
{
    printf("Starting to import the .dmf file...\n");

    FILE *fptr = fopen(fname, "rb");

    if (fptr == NULL) 
    {
        printf("File not found.\n");
        exit(1); 
    }
    
    if (strcmp(getFilenameExt(fname), "dmf") != 0)
    {
        printf("Input file has the wrong file extension.\nPlease use a .dmf file.\n");
        exit(1);
    }

    uint8_t *fBuff; 
    int ret = inf(fptr, &fBuff); 
    fclose(fptr);

    if (ret == Z_OK)
    {
        printf("Successful inflation.\n");
    }
    else
    {
        printf("Unsuccessful inflation.\n");
        zerr(ret);
        exit(1);
    }   
    
    uint32_t pos = 0; // The current buffer position 

    ///////////////// FORMAT FLAGS  
    char header[17]; 
    strncpy(header, fBuff, 16); 
    pos += 16; 
    
    if (strncmp(header, ".DelekDefleMask.", 16) != 0)
    {
        printf("Format header is bad.\n");
        exit(1); 
    }

    dmf->dmfFileVersion = fBuff[pos++]; 
    printf(".dmf File Version: %u or 0x%x\n", dmf->dmfFileVersion, dmf->dmfFileVersion); 

    ///////////////// SYSTEM SET  
    dmf->sys = getSystem(fBuff[pos++]); 
    printf("System: %s (channels: %u)\n", dmf->sys.name, dmf->sys.channels);

    ///////////////// VISUAL INFORMATION 
    loadVisualInfo(&fBuff, &pos, dmf); 
    printf("Loaded visual information.\n");

    ///////////////// MODULE INFORMATION  
    loadModuleInfo(&fBuff, &pos, dmf); 
    printf("Loaded module.\n");

    ///////////////// PATTERN MATRIX VALUES 
    loadPatternMatrixValues(&fBuff, &pos, dmf); 
    printf("Loaded pattern matrix values.\n");

    ///////////////// INSTRUMENTS DATA 
    loadInstrumentsData(&fBuff, &pos, dmf); 
    printf("Loaded instruments.\n");

    ///////////////// WAVETABLES DATA
    loadWavetablesData(&fBuff, &pos, dmf); 
    printf("Loaded %u wavetable(s).\n", dmf->totalWavetables); 

    ///////////////// PATTERNS DATA
    loadPatternsData(&fBuff, &pos, dmf); 
    printf("Loaded patterns.\n");

    ///////////////// PCM SAMPLES DATA
    loadPCMSamplesData(&fBuff, &pos, dmf); 
    printf("Loaded PCM Samples.\n");

    free(fBuff);

    printf("Done loading .dmf file!\n\n");
}

System getSystem(uint8_t systemByte)
{
    for (int i = 1; i < 10; i++)
    {
        if (Systems[i].id == systemByte) 
            return Systems[i];
    }
    return Systems[0]; // Error: System byte invalid  
}

void loadVisualInfo(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf) 
{
    dmf->visualInfo.songNameLength = (*fBuff)[(*pos)++];    
    dmf->visualInfo.songName = malloc((dmf->visualInfo.songNameLength + 1) * sizeof(char)); 
    strncpy(dmf->visualInfo.songName, &(*fBuff)[*pos], dmf->visualInfo.songNameLength + 1);
    dmf->visualInfo.songName[dmf->visualInfo.songNameLength] = '\0'; 
    *pos += dmf->visualInfo.songNameLength; 

    printf("Title: %s\n", dmf->visualInfo.songName);

    dmf->visualInfo.songAuthorLength = (*fBuff)[(*pos)++];    
    dmf->visualInfo.songAuthor = malloc((dmf->visualInfo.songAuthorLength + 1) * sizeof(char)); 
    strncpy(dmf->visualInfo.songAuthor, &(*fBuff)[*pos], dmf->visualInfo.songAuthorLength + 1);
    dmf->visualInfo.songAuthor[dmf->visualInfo.songAuthorLength] = '\0'; 
    *pos += dmf->visualInfo.songAuthorLength; 

    printf("Author: %s\n", dmf->visualInfo.songAuthor);

    dmf->visualInfo.highlightAPatterns = (*fBuff)[(*pos)++];  
    dmf->visualInfo.highlightBPatterns = (*fBuff)[(*pos)++]; 
}

void loadModuleInfo(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf)
{
    dmf->moduleInfo.timeBase = (*fBuff)[(*pos)++];   
    dmf->moduleInfo.tickTime1 = (*fBuff)[(*pos)++]; 
    dmf->moduleInfo.tickTime2 = (*fBuff)[(*pos)++]; 
    dmf->moduleInfo.framesMode = (*fBuff)[(*pos)++]; 
    dmf->moduleInfo.usingCustomHZ = (*fBuff)[(*pos)++]; 
    dmf->moduleInfo.customHZValue1 = (*fBuff)[(*pos)++]; 
    dmf->moduleInfo.customHZValue2 = (*fBuff)[(*pos)++]; 
    dmf->moduleInfo.customHZValue3 = (*fBuff)[(*pos)++]; 
    dmf->moduleInfo.totalRowsPerPattern = (*fBuff)[(*pos)++]; 
    dmf->moduleInfo.totalRowsPerPattern |= (*fBuff)[(*pos)++] << 8;
    dmf->moduleInfo.totalRowsPerPattern |= (*fBuff)[(*pos)++] << 16;
    dmf->moduleInfo.totalRowsPerPattern |= (*fBuff)[(*pos)++] << 24;
    dmf->moduleInfo.totalRowsInPatternMatrix = (*fBuff)[(*pos)++]; 

    /*
    printf("timeBase: %u\n", dmf->moduleInfo.timeBase);  
    printf("tickTime1: %u\n", dmf->moduleInfo.tickTime1);  
    printf("tickTime2: %u\n", dmf->moduleInfo.tickTime2);  
    printf("framesMode: %u\n", dmf->moduleInfo.framesMode);  // If this is called "Step" in Deflemask, then this is good 
    printf("usingCustomHZ: %u\n", dmf->moduleInfo.usingCustomHZ);    // Whether the "Custom" clock box is checked? 
    printf("customHZValue1: %u\n", dmf->moduleInfo.customHZValue1);  // Hz clock
    printf("customHZValue2: %u\n", dmf->moduleInfo.customHZValue2);  // Hz clock
    printf("customHZValue3: %u\n", dmf->moduleInfo.customHZValue3);  // Hz clock

    printf("totalRowsPerPattern: %u\n", dmf->moduleInfo.totalRowsPerPattern);  
    printf("totalRowsInPatternMatrix: %u or %x\n", dmf->moduleInfo.totalRowsInPatternMatrix, dmf->moduleInfo.totalRowsInPatternMatrix); // Good. 
    */

    // NOTE: In previous .dmp versions, arpeggio tick speed is stored here!!! 

}

void loadPatternMatrixValues(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf) 
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
            dmf->patternMatrixValues[i][j] = (*fBuff)[(*pos)++]; 
            if (dmf->patternMatrixValues[i][j] > dmf->patternMatrixMaxValues[i]) 
            {
                dmf->patternMatrixMaxValues[i] = dmf->patternMatrixValues[i][j]; 
            }
        }
    }
}

void loadInstrumentsData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf)
{
    dmf->totalInstruments = (*fBuff)[(*pos)++];
    dmf->instruments = (Instrument *)malloc(dmf->totalInstruments * sizeof(Instrument)); 

    for (int i = 0; i < dmf->totalInstruments; i++)
    {
        dmf->instruments[i] = loadInstrument(fBuff, pos, dmf->sys); 
    }
}

Instrument loadInstrument(uint8_t **fBuff, uint32_t *pos, System systemType)
{
    Instrument inst; 

    uint8_t name_size = (*fBuff)[(*pos)++];    
    inst.name = malloc((name_size + 1) * sizeof(char)); 
    strncpy(inst.name, &(*fBuff)[*pos], name_size + 1);
    inst.name[name_size] = '\0'; 
    *pos += name_size; 

    inst.mode = (*fBuff)[(*pos)++]; // 1 = FM; 0 = Standard 
    
    if (inst.mode == 1) // FM instrument 
    {
        inst.fmALG = (*fBuff)[(*pos)++]; 
        inst.fmFB = (*fBuff)[(*pos)++]; 
        inst.fmLFO = (*fBuff)[(*pos)++]; 
        inst.fmLFO2 = (*fBuff)[(*pos)++]; 

        int TOTAL_OPERATORS = 1;  // I'm not sure what toal operators is or where I'm supposed to get it from 
        for (int i = 0; i < TOTAL_OPERATORS; i++)
        {
            inst.fmAM = (*fBuff)[(*pos)++]; 
            inst.fmAR = (*fBuff)[(*pos)++]; 
            inst.fmDR = (*fBuff)[(*pos)++]; 
            inst.fmMULT = (*fBuff)[(*pos)++]; 
            inst.fmRR = (*fBuff)[(*pos)++]; 
            inst.fmSL = (*fBuff)[(*pos)++]; 
            inst.fmTL = (*fBuff)[(*pos)++]; 
            inst.fmDT2 = (*fBuff)[(*pos)++]; 
            inst.fmRS = (*fBuff)[(*pos)++]; 
            inst.fmDT = (*fBuff)[(*pos)++]; 
            inst.fmD2R = (*fBuff)[(*pos)++]; 
            inst.fmSSGMODE = (*fBuff)[(*pos)++]; 
        }
    }
    else if (inst.mode == 0) // Standard instrument 
    {
        if (strcmp(systemType.name, "GAMEBOY") != 0)  // Not a GameBoy  
        {
            // Volume macro 
            inst.stdVolEnvSize = (*fBuff)[(*pos)++]; 
            inst.stdVolEnvValue = (int32_t *)malloc(inst.stdVolEnvSize * sizeof(int32_t));
            for (int i = 0; i < inst.stdVolEnvSize; i++)
            {
                // 4 bytes, little-endian 
                inst.stdVolEnvValue[i] = (*fBuff)[(*pos)++]; 
                inst.stdVolEnvValue[i] |= (*fBuff)[(*pos)++] << 8;
                inst.stdVolEnvValue[i] |= (*fBuff)[(*pos)++] << 16;
                inst.stdVolEnvValue[i] |= (*fBuff)[(*pos)++] << 24;
            }
            if (inst.stdVolEnvSize > 0) 
                inst.stdVolEnvLoopPos = (*fBuff)[(*pos)++]; 
        }

        // Arpeggio macro 
        inst.stdArpEnvSize = (*fBuff)[(*pos)++]; 
        inst.stdArpEnvValue = (int32_t *)malloc(inst.stdArpEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdArpEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdArpEnvValue[i] = (*fBuff)[(*pos)++]; 
            inst.stdArpEnvValue[i] |= (*fBuff)[(*pos)++] << 8;
            inst.stdArpEnvValue[i] |= (*fBuff)[(*pos)++] << 16;
            inst.stdArpEnvValue[i] |= (*fBuff)[(*pos)++] << 24;
        }

        if (inst.stdArpEnvSize > 0)
            inst.stdArpEnvLoopPos = (*fBuff)[(*pos)++]; 
        inst.stdArpMacroMode = (*fBuff)[(*pos)++]; 

        // Duty/Noise macro 
        inst.stdDutyNoiseEnvSize = (*fBuff)[(*pos)++]; 
        inst.stdDutyNoiseEnvValue = (int32_t *)malloc(inst.stdDutyNoiseEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdDutyNoiseEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdDutyNoiseEnvValue[i] = (*fBuff)[(*pos)++]; 
            inst.stdDutyNoiseEnvValue[i] |= (*fBuff)[(*pos)++] << 8;
            inst.stdDutyNoiseEnvValue[i] |= (*fBuff)[(*pos)++] << 16;
            inst.stdDutyNoiseEnvValue[i] |= (*fBuff)[(*pos)++] << 24;
        }
        if (inst.stdDutyNoiseEnvSize > 0) 
            inst.stdDutyNoiseEnvLoopPos = (*fBuff)[(*pos)++]; 

        // Wavetable macro 
        inst.stdWavetableEnvSize = (*fBuff)[(*pos)++]; 
        inst.stdWavetableEnvValue = (int32_t *)malloc(inst.stdWavetableEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdWavetableEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdWavetableEnvValue[i] = (*fBuff)[(*pos)++]; 
            inst.stdWavetableEnvValue[i] |= (*fBuff)[(*pos)++] << 8;
            inst.stdWavetableEnvValue[i] |= (*fBuff)[(*pos)++] << 16;
            inst.stdWavetableEnvValue[i] |= (*fBuff)[(*pos)++] << 24;
        }
        if (inst.stdWavetableEnvSize > 0) 
            inst.stdWavetableEnvLoopPos = (*fBuff)[(*pos)++]; 

        // Per system data
        if (strcmp(systemType.name, "C64_SID_8580") == 0 || strcmp(systemType.name, "C64_SID_6581") == 0) // Using Commodore 64 
        {
            
            inst.stdC64TriWaveEn = (*fBuff)[(*pos)++]; 
            inst.stdC64SawWaveEn = (*fBuff)[(*pos)++]; 
            inst.stdC64PulseWaveEn = (*fBuff)[(*pos)++]; 
            inst.stdC64NoiseWaveEn = (*fBuff)[(*pos)++]; 
            inst.stdC64Attack = (*fBuff)[(*pos)++]; 
            inst.stdC64Decay = (*fBuff)[(*pos)++]; 
            inst.stdC64Sustain = (*fBuff)[(*pos)++]; 
            inst.stdC64Release = (*fBuff)[(*pos)++]; 
            inst.stdC64PulseWidth = (*fBuff)[(*pos)++]; 
            inst.stdC64RingModEn = (*fBuff)[(*pos)++]; 
            inst.stdC64SyncModEn = (*fBuff)[(*pos)++]; 
            inst.stdC64ToFilter = (*fBuff)[(*pos)++]; 
            inst.stdC64VolMacroToFilterCutoffEn = (*fBuff)[(*pos)++]; 
            inst.stdC64UseFilterValuesFromInst = (*fBuff)[(*pos)++]; 
            
            // Filter globals 
            inst.stdC64FilterResonance = (*fBuff)[(*pos)++]; 
            inst.stdC64FilterCutoff = (*fBuff)[(*pos)++]; 
            inst.stdC64FilterHighPass = (*fBuff)[(*pos)++]; 
            inst.stdC64FilterLowPass = (*fBuff)[(*pos)++]; 
            inst.stdC64FilterCH2Off = (*fBuff)[(*pos)++]; 
        }
        else if (strcmp(systemType.name, "GAMEBOY") == 0) // Using GameBoy 
        {
            inst.stdGBEnvVol = (*fBuff)[(*pos)++]; 
            inst.stdGBEnvDir = (*fBuff)[(*pos)++]; 
            inst.stdGBEnvLen = (*fBuff)[(*pos)++]; 
            inst.stdGBSoundLen = (*fBuff)[(*pos)++]; 
        }
    }

    return inst; 
}

void loadWavetablesData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf) 
{
    dmf->totalWavetables = (*fBuff)[(*pos)++]; 
    
    dmf->wavetableSizes = (uint32_t *)malloc(dmf->totalWavetables * sizeof(uint32_t));     
    dmf->wavetableValues = (uint32_t **)malloc(dmf->totalWavetables * sizeof(uint32_t *)); 

    for (int i = 0; i < dmf->totalWavetables; i++)
    {
        dmf->wavetableSizes[i] = (*fBuff)[(*pos)++]; 
        dmf->wavetableSizes[i] |= (*fBuff)[(*pos)++] << 8;
        dmf->wavetableSizes[i] |= (*fBuff)[(*pos)++] << 16;
        dmf->wavetableSizes[i] |= (*fBuff)[(*pos)++] << 24;

        dmf->wavetableValues[i] = (uint32_t *)malloc(dmf->wavetableSizes[i] * sizeof(uint32_t)); 
        for (int j = 0; j < dmf->wavetableSizes[i]; j++)
        {
            dmf->wavetableValues[i][j] = (*fBuff)[(*pos)++]; 
            dmf->wavetableValues[i][j] |= (*fBuff)[(*pos)++] << 8;
            dmf->wavetableValues[i][j] |= (*fBuff)[(*pos)++] << 16;
            dmf->wavetableValues[i][j] |= (*fBuff)[(*pos)++] << 24;
        }
    }
}

void loadPatternsData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf) 
{
    // patternValues[channel][pattern number][pattern row number]
    dmf->patternValues = (PatternRow ***)malloc(dmf->sys.channels * sizeof(PatternRow **)); 
    dmf->channelEffectsColumnsCount = (uint8_t *)malloc(dmf->sys.channels * sizeof(uint8_t)); 
    uint8_t patternMatrixNumber;

    for (int channel = 0; channel < dmf->sys.channels; channel++)
    {
        dmf->channelEffectsColumnsCount[channel] = (*fBuff)[(*pos)++]; 

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
                *pos += (8 + 4 * dmf->channelEffectsColumnsCount[channel]) * dmf->moduleInfo.totalRowsPerPattern; // Unnecessary information 
                continue; // Skip patterns that have already been loaded 
            }

            dmf->patternValues[channel][patternMatrixNumber] = (PatternRow *)malloc(dmf->moduleInfo.totalRowsPerPattern * sizeof(PatternRow));
            
            for (uint32_t row = 0; row < dmf->moduleInfo.totalRowsPerPattern; row++)
            {
                dmf->patternValues[channel][patternMatrixNumber][row] = loadPatternRow(fBuff, pos, dmf->channelEffectsColumnsCount[channel]);
            }
        }
    }
}

PatternRow loadPatternRow(uint8_t **fBuff, uint32_t *pos, int effectsColumnsCount)
{
    PatternRow pat; 

    pat.note = (*fBuff)[(*pos)++]; 
    pat.note |= (*fBuff)[(*pos)++] << 8; 
    pat.octave = (*fBuff)[(*pos)++]; 
    pat.octave |= (*fBuff)[(*pos)++] << 8; 
    pat.volume = (*fBuff)[(*pos)++]; 
    pat.volume |= (*fBuff)[(*pos)++] << 8; 

    for (int col = 0; col < effectsColumnsCount; col++)
    {
        pat.effectCode[col] = (*fBuff)[(*pos)++]; 
        pat.effectCode[col] |= (*fBuff)[(*pos)++] << 8; 
        pat.effectValue[col] = (*fBuff)[(*pos)++]; 
        pat.effectValue[col] |= (*fBuff)[(*pos)++] << 8; 
    }

    pat.instrument = (*fBuff)[(*pos)++]; 
    pat.instrument |= (*fBuff)[(*pos)++] << 8; 

    return pat;
}

void loadPCMSamplesData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf)
{
    dmf->totalPCMSamples = (*fBuff)[(*pos)++]; 
    dmf->pcmSamples = (PCMSample *)malloc(dmf->totalPCMSamples * sizeof(PCMSample));

    for (int sample = 0; sample < dmf->totalPCMSamples; sample++) 
    {
        dmf->pcmSamples[sample] = loadPCMSample(fBuff, pos);
    }

} 

PCMSample loadPCMSample(uint8_t **fBuff, uint32_t *pos)
{
    PCMSample sample; 

    sample.size = (*fBuff)[(*pos)++];
    sample.size |= (*fBuff)[(*pos)++] << 8;
    sample.size |= (*fBuff)[(*pos)++] << 16;
    sample.size |= (*fBuff)[(*pos)++] << 24;

    uint8_t name_size = (*fBuff)[(*pos)++]; 
    sample.name = (char *)malloc(name_size * sizeof(char)); 
    strncpy(sample.name, &(*fBuff)[*pos], name_size + 1);
    sample.name[name_size] = '\0'; 
    *pos += name_size; 

    sample.rate = (*fBuff)[(*pos)++]; 
    sample.pitch = (*fBuff)[(*pos)++]; 
    sample.amp = (*fBuff)[(*pos)++]; 
    sample.bits = (*fBuff)[(*pos)++]; 

    sample.data = (uint16_t *)malloc(sample.size * sizeof(uint16_t *)); 
    for (uint32_t i = 0; i < sample.size; i++) 
    {
        sample.data[i] = (*fBuff)[(*pos)++]; 
        sample.data[i] |= (*fBuff)[(*pos)++] << 8; 
    }

    return sample; 
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

const char *getFilenameExt(const char *fname) 
{
    const char* dot = strrchr(fname, '.');
    if (!dot || dot == fname) 
    {
        return "";
    }
    return dot + 1;
}

