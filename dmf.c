/*
dmf.c
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Provides functions for loading a .dmf file according to the 
spec sheet at http://www.deflemask.com/DMF_SPECS.txt. 

Requires zlib1.dll from the zlib compression library at https://zlib.net. 
*/

#include "dmf.h"

#define RI (*fBuff)[(*pos)++] // Read buffer at position pos, then Increment pos  

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

static System getSystem(uint8_t systemByte);
static void loadVisualInfo(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
static void loadModuleInfo(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
static void loadPatternMatrixValues(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
static void loadInstrumentsData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf);
static Instrument loadInstrument(uint8_t **fBuff, uint32_t *pos, System systemType); 
static void loadWavetablesData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
static void loadPatternsData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
static PatternRow loadPatternRow(uint8_t **fBuff, uint32_t *pos, int effectsColumnsCount); 
static void loadPCMSamplesData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf); 
static PCMSample loadPCMSample(uint8_t **fBuff, uint32_t *pos); 

/* Decompress from file source to buffer dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading the file. */
static int inf(FILE *source, uint8_t **dest) 
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
static void zerr(int ret)
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

int importDMF(const char *fname, DMFContents *dmf)
{
    printf("Starting to import the .dmf file...\n");

    if (strcmp(getFilenameExt(fname), ".dmf") != 0)
    {
        printf("Input file has the wrong file extension.\nPlease use a .dmf file.\n");
        return 1;
    }

    FILE *fptr = fopen(fname, "rb");

    if (fptr == NULL) 
    {
        printf("File not found.\n");
        return 1; 
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
        return 1;
    }   
    
    uint32_t pos = 0; // The current buffer position 

    ///////////////// FORMAT FLAGS  
    char header[17]; 
    strncpy(header, (char *)fBuff, 16); 
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

    return 0; // Success 
}

static System getSystem(uint8_t systemByte)
{
    for (int i = 1; i < 10; i++)
    {
        if (Systems[i].id == systemByte) 
            return Systems[i];
    }
    return Systems[SYS_ERROR]; // Error: System byte invalid  
}

static void loadVisualInfo(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf) 
{
    dmf->visualInfo.songNameLength = RI;    
    dmf->visualInfo.songName = malloc((dmf->visualInfo.songNameLength + 1) * sizeof(char)); 
    strncpy(dmf->visualInfo.songName, (char *)(&(*fBuff)[*pos]), dmf->visualInfo.songNameLength + 1);
    dmf->visualInfo.songName[dmf->visualInfo.songNameLength] = '\0'; 
    *pos += dmf->visualInfo.songNameLength; 

    printf("Title: %s\n", dmf->visualInfo.songName);

    dmf->visualInfo.songAuthorLength = RI;    
    dmf->visualInfo.songAuthor = malloc((dmf->visualInfo.songAuthorLength + 1) * sizeof(char)); 
    strncpy(dmf->visualInfo.songAuthor, (char *)(&(*fBuff)[*pos]), dmf->visualInfo.songAuthorLength + 1);
    dmf->visualInfo.songAuthor[dmf->visualInfo.songAuthorLength] = '\0'; 
    *pos += dmf->visualInfo.songAuthorLength; 

    printf("Author: %s\n", dmf->visualInfo.songAuthor);

    dmf->visualInfo.highlightAPatterns = RI;  
    dmf->visualInfo.highlightBPatterns = RI; 
}

static void loadModuleInfo(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf)
{
    dmf->moduleInfo.timeBase = RI;   
    dmf->moduleInfo.tickTime1 = RI; 
    dmf->moduleInfo.tickTime2 = RI; 
    dmf->moduleInfo.framesMode = RI; 
    dmf->moduleInfo.usingCustomHZ = RI; 
    dmf->moduleInfo.customHZValue1 = RI; 
    dmf->moduleInfo.customHZValue2 = RI; 
    dmf->moduleInfo.customHZValue3 = RI; 
    dmf->moduleInfo.totalRowsPerPattern = RI; 
    dmf->moduleInfo.totalRowsPerPattern |= RI << 8;
    dmf->moduleInfo.totalRowsPerPattern |= RI << 16;
    dmf->moduleInfo.totalRowsPerPattern |= RI << 24;
    dmf->moduleInfo.totalRowsInPatternMatrix = RI; 

    // NOTE: In previous .dmp versions, arpeggio tick speed is stored here!!! 
}

static void loadPatternMatrixValues(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf) 
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
            dmf->patternMatrixValues[i][j] = RI; 
            if (dmf->patternMatrixValues[i][j] > dmf->patternMatrixMaxValues[i]) 
            {
                dmf->patternMatrixMaxValues[i] = dmf->patternMatrixValues[i][j]; 
            }
        }
    }
}

static void loadInstrumentsData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf)
{
    dmf->totalInstruments = RI;
    dmf->instruments = (Instrument *)malloc(dmf->totalInstruments * sizeof(Instrument)); 

    for (int i = 0; i < dmf->totalInstruments; i++)
    {
        dmf->instruments[i] = loadInstrument(fBuff, pos, dmf->sys); 
    }
}

static Instrument loadInstrument(uint8_t **fBuff, uint32_t *pos, System systemType)
{
    Instrument inst; 

    uint8_t name_size = RI;    
    inst.name = malloc((name_size + 1) * sizeof(char)); 
    strncpy(inst.name, (char *)(&(*fBuff)[*pos]), name_size + 1);
    inst.name[name_size] = '\0'; 
    *pos += name_size; 

    inst.mode = RI; // 1 = FM; 0 = Standard 
    
    // Initialize to NULL in case malloc isn't used on them later: 
    inst.stdVolEnvValue = NULL; 
    inst.stdArpEnvValue = NULL;
    inst.stdDutyNoiseEnvValue = NULL; 
    inst.stdWavetableEnvValue = NULL; 


    if (inst.mode == 1) // FM instrument 
    {
        inst.fmALG = RI; 
        inst.fmFB = RI; 
        inst.fmLFO = RI; 
        inst.fmLFO2 = RI; 

        int TOTAL_OPERATORS = 1;  // I'm not sure what total operators is or where I'm supposed to get it from 
        for (int i = 0; i < TOTAL_OPERATORS; i++)
        {
            inst.fmAM = RI; 
            inst.fmAR = RI; 
            inst.fmDR = RI; 
            inst.fmMULT = RI; 
            inst.fmRR = RI; 
            inst.fmSL = RI; 
            inst.fmTL = RI; 
            inst.fmDT2 = RI; 
            inst.fmRS = RI; 
            inst.fmDT = RI; 
            inst.fmD2R = RI; 
            inst.fmSSGMODE = RI; 
        }
    }
    else if (inst.mode == 0) // Standard instrument 
    {
        if (systemType.id != Systems[SYS_GAMEBOY].id)  // Not a Game Boy  
        {
            // Volume macro 
            inst.stdVolEnvSize = RI; 
            inst.stdVolEnvValue = (int32_t *)malloc(inst.stdVolEnvSize * sizeof(int32_t));
            for (int i = 0; i < inst.stdVolEnvSize; i++)
            {
                // 4 bytes, little-endian 
                inst.stdVolEnvValue[i] = RI; 
                inst.stdVolEnvValue[i] |= RI << 8;
                inst.stdVolEnvValue[i] |= RI << 16;
                inst.stdVolEnvValue[i] |= RI << 24;
            }
            if (inst.stdVolEnvSize > 0) 
                inst.stdVolEnvLoopPos = RI; 
        }

        // Arpeggio macro 
        inst.stdArpEnvSize = RI; 
        inst.stdArpEnvValue = (int32_t *)malloc(inst.stdArpEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdArpEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdArpEnvValue[i] = RI; 
            inst.stdArpEnvValue[i] |= RI << 8;
            inst.stdArpEnvValue[i] |= RI << 16;
            inst.stdArpEnvValue[i] |= RI << 24;
        }

        if (inst.stdArpEnvSize > 0)
            inst.stdArpEnvLoopPos = RI; 
        inst.stdArpMacroMode = RI; 

        // Duty/Noise macro 
        inst.stdDutyNoiseEnvSize = RI; 
        inst.stdDutyNoiseEnvValue = (int32_t *)malloc(inst.stdDutyNoiseEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdDutyNoiseEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdDutyNoiseEnvValue[i] = RI; 
            inst.stdDutyNoiseEnvValue[i] |= RI << 8;
            inst.stdDutyNoiseEnvValue[i] |= RI << 16;
            inst.stdDutyNoiseEnvValue[i] |= RI << 24;
        }
        if (inst.stdDutyNoiseEnvSize > 0) 
            inst.stdDutyNoiseEnvLoopPos = RI; 

        // Wavetable macro 
        inst.stdWavetableEnvSize = RI; 
        inst.stdWavetableEnvValue = (int32_t *)malloc(inst.stdWavetableEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdWavetableEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdWavetableEnvValue[i] = RI; 
            inst.stdWavetableEnvValue[i] |= RI << 8;
            inst.stdWavetableEnvValue[i] |= RI << 16;
            inst.stdWavetableEnvValue[i] |= RI << 24;
        }
        if (inst.stdWavetableEnvSize > 0) 
            inst.stdWavetableEnvLoopPos = RI; 

        // Per system data
        if (systemType.id == Systems[SYS_C64_SID_8580].id || systemType.id == Systems[SYS_C64_SID_6581].id) // Using Commodore 64 
        {
            inst.stdC64TriWaveEn = RI; 
            inst.stdC64SawWaveEn = RI; 
            inst.stdC64PulseWaveEn = RI; 
            inst.stdC64NoiseWaveEn = RI; 
            inst.stdC64Attack = RI; 
            inst.stdC64Decay = RI; 
            inst.stdC64Sustain = RI; 
            inst.stdC64Release = RI; 
            inst.stdC64PulseWidth = RI; 
            inst.stdC64RingModEn = RI; 
            inst.stdC64SyncModEn = RI; 
            inst.stdC64ToFilter = RI; 
            inst.stdC64VolMacroToFilterCutoffEn = RI; 
            inst.stdC64UseFilterValuesFromInst = RI; 
            
            // Filter globals 
            inst.stdC64FilterResonance = RI; 
            inst.stdC64FilterCutoff = RI; 
            inst.stdC64FilterHighPass = RI; 
            inst.stdC64FilterLowPass = RI; 
            inst.stdC64FilterCH2Off = RI; 
        }
        else if (systemType.id == Systems[SYS_GAMEBOY].id) // Using Game Boy 
        {
            inst.stdGBEnvVol = RI; 
            inst.stdGBEnvDir = RI; 
            inst.stdGBEnvLen = RI; 
            inst.stdGBSoundLen = RI; 
        }
    }

    return inst; 
}

static void loadWavetablesData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf) 
{
    dmf->totalWavetables = RI; 
    
    dmf->wavetableSizes = (uint32_t *)malloc(dmf->totalWavetables * sizeof(uint32_t));     
    dmf->wavetableValues = (uint32_t **)malloc(dmf->totalWavetables * sizeof(uint32_t *)); 

    for (int i = 0; i < dmf->totalWavetables; i++)
    {
        dmf->wavetableSizes[i] = RI; 
        dmf->wavetableSizes[i] |= RI << 8;
        dmf->wavetableSizes[i] |= RI << 16;
        dmf->wavetableSizes[i] |= RI << 24;

        dmf->wavetableValues[i] = (uint32_t *)malloc(dmf->wavetableSizes[i] * sizeof(uint32_t)); 
        for (int j = 0; j < dmf->wavetableSizes[i]; j++)
        {
            dmf->wavetableValues[i][j] = RI; 
            dmf->wavetableValues[i][j] |= RI << 8;
            dmf->wavetableValues[i][j] |= RI << 16;
            dmf->wavetableValues[i][j] |= RI << 24;
        }
    }
}

static void loadPatternsData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf) 
{
    // patternValues[channel][pattern number][pattern row number]
    dmf->patternValues = (PatternRow ***)malloc(dmf->sys.channels * sizeof(PatternRow **)); 
    dmf->channelEffectsColumnsCount = (uint8_t *)malloc(dmf->sys.channels * sizeof(uint8_t)); 
    uint8_t patternMatrixNumber;

    for (int channel = 0; channel < dmf->sys.channels; channel++)
    {
        dmf->channelEffectsColumnsCount[channel] = RI; 

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

static PatternRow loadPatternRow(uint8_t **fBuff, uint32_t *pos, int effectsColumnsCount)
{
    PatternRow pat;  
    pat.note = (Note){0, 0};
    pat.note.pitch = RI; 
    pat.note.pitch |= RI << 8; // Unused byte. Storing it anyway. 
    pat.note.octave = RI; 
    pat.note.octave |= RI << 8; // Unused byte. Storing it anyway.  
    pat.volume = RI; 
    pat.volume |= RI << 8; 

    // NOTE: C# is considered the 1st note of an octave rather than C- like in the Deflemask program. 

    if (pat.note.pitch == 0 && pat.note.octave == 0) 
    {
        pat.note.pitch = DMF_NOTE_EMPTY; 
    }

    for (int col = 0; col < effectsColumnsCount; col++)
    {
        pat.effectCode[col] = RI; 
        pat.effectCode[col] |= RI << 8; 
        pat.effectValue[col] = RI; 
        pat.effectValue[col] |= RI << 8; 
    }

    pat.instrument = RI; 
    pat.instrument |= RI << 8; 

    return pat;
}

static void loadPCMSamplesData(uint8_t **fBuff, uint32_t *pos, DMFContents *dmf)
{
    dmf->totalPCMSamples = RI; 
    dmf->pcmSamples = (PCMSample *)malloc(dmf->totalPCMSamples * sizeof(PCMSample));

    for (int sample = 0; sample < dmf->totalPCMSamples; sample++) 
    {
        dmf->pcmSamples[sample] = loadPCMSample(fBuff, pos);
    }

} 

static PCMSample loadPCMSample(uint8_t **fBuff, uint32_t *pos)
{
    PCMSample sample; 

    sample.size = RI;
    sample.size |= RI << 8;
    sample.size |= RI << 16;
    sample.size |= RI << 24;

    uint8_t name_size = RI; 
    sample.name = (char *)malloc(name_size * sizeof(char)); 
    strncpy(sample.name, (char *)(&(*fBuff)[*pos]), name_size + 1);
    sample.name[name_size] = '\0'; 
    *pos += name_size; 

    sample.rate = RI; 
    sample.pitch = RI; 
    sample.amp = RI; 
    sample.bits = RI; 

    sample.data = (uint16_t *)malloc(sample.size * sizeof(uint16_t *)); 
    for (uint32_t i = 0; i < sample.size; i++) 
    {
        sample.data[i] = RI; 
        sample.data[i] |= RI << 8; 
    }

    return sample; 
}

double getBPM(const ModuleInfo *info)
{
    // Returns the initial bpm of the module when given ModuleInfo 
    unsigned int globalTick; 
    if (info->usingCustomHZ) 
    {
        if (info->customHZValue1 == 0) // No digits filled in 
        {
            // NTSC is used by default if custom global tick box is selected but the value is left blank: 
            globalTick = 60; 
        }
        else if (info->customHZValue2 == 0) // One digit filled in 
        {
            globalTick = (info->customHZValue1 - 48); 
        }
        else if (info->customHZValue3 == 0) // Two digits filled in
        {
            globalTick = (info->customHZValue1 - 48) * 10 + (info->customHZValue2 - 48); 
        }
        else // All three digits filled in 
        {
            globalTick = (info->customHZValue1 - 48) * 100 + (info->customHZValue2 - 48) * 10 + (info->customHZValue3 - 48); 
        }
    }
    else
    {
        globalTick = info->framesMode ? 60 : 50; // NTSC (60 Hz) or PAL (50 Hz)  
    }
    
    // Experimentally determined equation for bpm:  
    return (15.0 * globalTick) / ((info->timeBase + 1) * (info->tickTime1 + info->tickTime2)); 
}

int8_t noteCompare(const Note *n1, const Note *n2) 
{
    // Compares notes n1 and n2.
    // Assumes note isn't Note OFF or Empty note. 
    // Notes must use the .dmf style where the note C# is the 1st note of an octave rather than C-. 
    if (n1->octave + n1->pitch / 13.f > n2->octave + n2->pitch / 13.f) 
    {
        return 1; // n1 > n2 (n1 has a higher pitch than n2) 
    }
    else if (n1->octave + n1->pitch / 13.f < n2->octave + n2->pitch / 13.f) 
    {
        return -1; // n1 < n2 (n1 has a lower pitch than n2) 
    }
    else 
    {
        return 0; // Same note 
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

char *getFilenameExt(const char *fname) 
{
    char *dot = strrchr(fname, '.');
    if (!dot || dot == fname) 
    {
        return "";
    }
    return dot;
}

