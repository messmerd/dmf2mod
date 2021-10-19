/*
dmf.cpp
Written by Dalton Messmer <messmer.dalton@gmail.com>.

Provides functions for loading a dmf file according to the 
spec sheet at http://www.deflemask.com/DMF_SPECS.txt.

Requires the zlib compression library from https://zlib.net.
*/

#include "dmf.h"

// For inflating .dmf files so that they can be read 
#include "zlib.h"
#include "zconf.h"

#include <iostream>
#include <string>

#define DMF_FILE_VERSION 24 // 0x18 - Only DefleMask v0.12.0 files are supported

#define RI (*fBuff)[(*pos)++] // Read buffer at position pos, then Increment pos

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
static int inf(FILE *source, uint8_t **dest)
{
    int ret;
    unsigned have;
    z_stream strm;
    uint8_t in[CHUNK];
    uint8_t out[CHUNK];
    *dest = (uint8_t *)malloc(CHUNK * sizeof(uint8_t));
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
            // TODO: Read file size and allocate correct amount from the start?
            if (current_pos + have > current_size)
            {
                *dest = (uint8_t *)realloc(*dest, 2*current_size * sizeof(uint8_t));
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
// Using designated initialization for the array
const System DMF::m_Systems[] = {
    [SYS_ERROR] = {.id = 0x00, .name = "ERROR", .channels = 0},
	[SYS_GENESIS] = {.id = 0x02, .name = "GENESIS", .channels = 10},
	[SYS_GENESIS_CH3] = {.id = 0x12, .name = "GENESIS_CH3", .channels = 13},
	[SYS_SMS] = {.id = 0x03, .name = "SMS", .channels = 4},
	[SYS_GAMEBOY] = {.id = 0x04, .name = "GAMEBOY", .channels = 4},
	[SYS_PCENGINE] = {.id = 0x05, .name = "PCENGINE", .channels = 6},
	[SYS_NES] = {.id = 0x06, .name = "NES", .channels = 5},
	[SYS_C64_SID_8580] = {.id = 0x07, .name = "C64_SID_8580", .channels = 3},
	[SYS_C64_SID_6581] = {.id = 0x17, .name = "C64_SID_6581", .channels = 3},
	[SYS_YM2151] = {.id = 0x08, .name = "YM2151", .channels = 13}
};

DMF::DMF()
{
    // Initialize pointers to nullptr to prevent segfault when freeing memory if the import fails:
    m_VisualInfo.songName = nullptr;
    m_VisualInfo.songAuthor = nullptr;
    m_PatternValues = nullptr;
    m_ChannelEffectsColumnsCount = nullptr;
    m_PatternMatrixValues = nullptr;
    m_PatternMatrixMaxValues = nullptr;
    m_Instruments = nullptr;
    m_WavetableSizes = nullptr;
    m_WavetableValues = nullptr;
    m_ChannelEffectsColumnsCount = nullptr;
    m_PCMSamples = nullptr;

    m_ImportError = IMPORT_ERROR_FAIL;
}

DMF::~DMF()
{
    CleanUp();
}

void DMF::CleanUp()
{
    // Free memory allocated for members
    delete[] m_VisualInfo.songName;
    m_VisualInfo.songName = nullptr;

    delete[] m_VisualInfo.songAuthor;
    m_VisualInfo.songAuthor = nullptr;

    if (m_PatternMatrixMaxValues)
    {
        for (int channel = 0; channel < m_System.channels; channel++)
        {
            for (int i = 0; i < m_PatternMatrixMaxValues[channel] + 1; i++)
            {
                delete m_PatternValues[channel][i];
                m_PatternValues[channel][i] = nullptr;
            }
            delete[] m_PatternValues[channel];
            m_PatternValues[channel] = nullptr;
        }
        delete[] m_PatternValues;
        m_PatternValues = nullptr;
    }

    if (m_PatternMatrixValues)
    {
        for (int i = 0; i < m_System.channels; i++)
        {
            delete[] m_PatternMatrixValues[i];
            m_PatternMatrixValues[i] = nullptr;
        }
        delete[] m_PatternMatrixValues;
        m_PatternMatrixValues = nullptr;
    }
    
    delete[] m_PatternMatrixMaxValues;
    m_PatternMatrixMaxValues = nullptr;

    if (m_Instruments)
    {
        for (int i = 0; i < m_TotalInstruments; i++) 
        {
            delete[] m_Instruments[i].name;
            delete[] m_Instruments[i].stdArpEnvValue;
            delete[] m_Instruments[i].stdDutyNoiseEnvValue;
            delete[] m_Instruments[i].stdVolEnvValue;
            delete[] m_Instruments[i].stdWavetableEnvValue;
        }
        delete[] m_Instruments;
        m_Instruments = nullptr;
    }
    
    delete[] m_WavetableSizes;
    m_WavetableSizes = nullptr;

    if (m_WavetableValues)
    {
        for (int i = 0; i < m_TotalWavetables; i++)
        {
            delete[] m_WavetableValues[i];
            m_WavetableValues[i] = nullptr;
        }
        delete[] m_WavetableValues;
        m_WavetableValues = nullptr;
    }
    
    delete[] m_ChannelEffectsColumnsCount;
    m_ChannelEffectsColumnsCount = nullptr;

    if (m_PCMSamples)
    {
        for (int sample = 0; sample < m_TotalPCMSamples; sample++) 
        {
            delete[] m_PCMSamples[sample].name;
            delete[] m_PCMSamples[sample].data;
        }
        delete[] m_PCMSamples;
        m_PCMSamples = nullptr;
    }
}

bool DMF::Load(const char* filename)
{
    CleanUp();
    m_ImportError = IMPORT_ERROR_FAIL;

    std::cout << "Starting to import the DMF file..." << std::endl;

    if (ModuleUtils::GetType(filename) != ModuleType::DMF)
    {
        std::cout << "ERROR: Input file has the wrong file extension." << std::endl << "Please use a DMF file." << std::endl;
        m_ImportError = IMPORT_ERROR_FAIL;
        return true;
    }

    std::cout << "DMF Filename:" << filename << ".\n";
    FILE *fptr = fopen(filename, "rb");

    if (fptr) 
    {
        std::cout << "ERROR: File not found." << std::endl;
        m_ImportError = IMPORT_ERROR_FAIL;
        return true;
    }

    uint8_t *fBuff;
    int ret = inf(fptr, &fBuff);
    fclose(fptr);

    if (ret != Z_OK)
    {
        std::cout << "ERROR: Unsuccessful DMF inflation." << std::endl;
        zerr(ret);
        m_ImportError = IMPORT_ERROR_FAIL;
        return true;
    }   
    
    uint32_t pos = 0; // The current buffer position 

    ///////////////// FORMAT FLAGS  
    char header[17]; 
    strncpy(header, (char *)fBuff, 16); 
    header[16] = '\0';
    pos += 16; 
    
    if (strncmp(header, ".DelekDefleMask.", 16) != 0)
    {
        std::cout << "ERROR: DMF format header is bad.\n" << std::endl;
        m_ImportError = IMPORT_ERROR_FAIL;
        return true;
    }

    m_DMFFileVersion = fBuff[pos++]; 
    if (m_DMFFileVersion != DMF_FILE_VERSION)
    {
        printf("ERROR: Deflemask file version must be %u (0x%x). The given DMF file is version %u (0x%x).\n", DMF_FILE_VERSION, DMF_FILE_VERSION, m_DMFFileVersion, m_DMFFileVersion); 
        printf("       You can convert older DMF files to the correct version by opening them in DefleMask v0.12.0 and then saving them.\n");
        m_ImportError = IMPORT_ERROR_FAIL;
        return true;
    }

    ///////////////// SYSTEM SET
    m_System = GetSystem(fBuff[pos++]);
    printf("System: %s (channels: %u)\n", m_System.name, m_System.channels);

    ///////////////// VISUAL INFORMATION 
    LoadVisualInfo(&fBuff, &pos);
    printf("Loaded visual information.\n");

    ///////////////// MODULE INFORMATION 
    LoadModuleInfo(&fBuff, &pos);
    printf("Loaded module.\n");

    ///////////////// PATTERN MATRIX VALUES 
    LoadPatternMatrixValues(&fBuff, &pos); 
    printf("Loaded pattern matrix values.\n");

    ///////////////// INSTRUMENTS DATA 
    LoadInstrumentsData(&fBuff, &pos);
    printf("Loaded instruments.\n");

    ///////////////// WAVETABLES DATA
    LoadWavetablesData(&fBuff, &pos); 
    printf("Loaded %u wavetable(s).\n", m_TotalWavetables);

    ///////////////// PATTERNS DATA
    LoadPatternsData(&fBuff, &pos);
    printf("Loaded patterns.\n");

    ///////////////// PCM SAMPLES DATA
    LoadPCMSamplesData(&fBuff, &pos);
    printf("Loaded PCM Samples.\n");

    free(fBuff);

    printf("Done loading DMF file!\n\n");

    m_ImportError = IMPORT_ERROR_SUCCESS;
    return false;
}

bool DMF::Save(const char* filename)
{
    // Not implemented
    return false;
}

System DMF::GetSystem(uint8_t systemByte)
{
    const size_t size = sizeof(m_Systems) / sizeof(m_Systems[0]);
    for (int i = 1; i < size; i++)
    {
        if (m_Systems[i].id == systemByte)
            return m_Systems[i];
    }
    return m_Systems[SYS_ERROR]; // Error: System byte invalid
}

void DMF::LoadVisualInfo(uint8_t **fBuff, uint32_t *pos)
{
    m_VisualInfo.songNameLength = RI;
    m_VisualInfo.songName = new char[m_VisualInfo.songNameLength + 1];
    strncpy(m_VisualInfo.songName, (char *)(&(*fBuff)[*pos]), m_VisualInfo.songNameLength + 1);
    m_VisualInfo.songName[m_VisualInfo.songNameLength] = '\0';
    *pos += m_VisualInfo.songNameLength;

    printf("Title: %s\n", m_VisualInfo.songName);

    m_VisualInfo.songAuthorLength = RI;
    m_VisualInfo.songAuthor = new char[m_VisualInfo.songAuthorLength + 1];
    strncpy(m_VisualInfo.songAuthor, (char *)(&(*fBuff)[*pos]), m_VisualInfo.songAuthorLength + 1);
    m_VisualInfo.songAuthor[m_VisualInfo.songAuthorLength] = '\0';
    *pos += m_VisualInfo.songAuthorLength;

    printf("Author: %s\n", m_VisualInfo.songAuthor);

    m_VisualInfo.highlightAPatterns = RI;
    m_VisualInfo.highlightBPatterns = RI;
}

void DMF::LoadModuleInfo(uint8_t **fBuff, uint32_t *pos)
{
    m_ModuleInfo.timeBase = RI;
    m_ModuleInfo.tickTime1 = RI;
    m_ModuleInfo.tickTime2 = RI;
    m_ModuleInfo.framesMode = RI;
    m_ModuleInfo.usingCustomHZ = RI;
    m_ModuleInfo.customHZValue1 = RI;
    m_ModuleInfo.customHZValue2 = RI;
    m_ModuleInfo.customHZValue3 = RI;
    m_ModuleInfo.totalRowsPerPattern = RI;
    m_ModuleInfo.totalRowsPerPattern |= RI << 8;
    m_ModuleInfo.totalRowsPerPattern |= RI << 16;
    m_ModuleInfo.totalRowsPerPattern |= RI << 24;
    m_ModuleInfo.totalRowsInPatternMatrix = RI;

    // TODO: In previous DMF versions, arpeggio tick speed is stored here!!! 
}

void DMF::LoadPatternMatrixValues(uint8_t **fBuff, uint32_t *pos)
{
    // Format: patterMatrixValues[channel][pattern matrix row]
    m_PatternMatrixValues = new uint8_t*[m_System.channels];
    m_PatternMatrixMaxValues = new uint8_t[m_System.channels];

    for (int i = 0; i < m_System.channels; i++)
    {
        m_PatternMatrixMaxValues[i] = 0;
        m_PatternMatrixValues[i] = new uint8_t[m_ModuleInfo.totalRowsInPatternMatrix];
        
        for (int j = 0; j < m_ModuleInfo.totalRowsInPatternMatrix; j++)
        {
            m_PatternMatrixValues[i][j] = RI;
            if (m_PatternMatrixValues[i][j] > m_PatternMatrixMaxValues[i])
            {
                m_PatternMatrixMaxValues[i] = m_PatternMatrixValues[i][j];
            }
        }
    }
}

void DMF::LoadInstrumentsData(uint8_t **fBuff, uint32_t *pos)
{
    m_TotalInstruments = RI;
    m_Instruments = new Instrument[m_TotalInstruments];

    for (int i = 0; i < m_TotalInstruments; i++)
    {
        m_Instruments[i] = LoadInstrument(fBuff, pos, m_System); 
    }
}

Instrument DMF::LoadInstrument(uint8_t **fBuff, uint32_t *pos, System systemType)
{
    Instrument inst;

    uint8_t name_size = RI;
    inst.name = new char[name_size + 1];
    strncpy(inst.name, (char *)(&(*fBuff)[*pos]), name_size + 1);
    inst.name[name_size] = '\0';
    *pos += name_size;

    inst.mode = RI; // 1 = FM; 0 = Standard
    
    // Initialize to NULL in case malloc isn't used on them later:
    inst.stdVolEnvValue = nullptr;
    inst.stdArpEnvValue = nullptr;
    inst.stdDutyNoiseEnvValue = nullptr;
    inst.stdWavetableEnvValue = nullptr;

    if (inst.mode == 1) // FM instrument
    {
        inst.fmALG = RI;
        inst.fmFB = RI;
        inst.fmLFO = RI;
        inst.fmLFO2 = RI;

        constexpr int TOTAL_OPERATORS = 4;
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
        if (systemType.id != m_Systems[SYS_GAMEBOY].id)  // Not a Game Boy
        {
            // Volume macro
            inst.stdVolEnvSize = RI;
            inst.stdVolEnvValue = new int32_t[inst.stdVolEnvSize];
            
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
        inst.stdArpEnvValue = new int32_t[inst.stdArpEnvSize];
        
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
        inst.stdDutyNoiseEnvValue = new int32_t[inst.stdDutyNoiseEnvSize];
        
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
        inst.stdWavetableEnvValue = new int32_t[inst.stdWavetableEnvSize];
        
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
        if (systemType.id == m_Systems[SYS_C64_SID_8580].id || systemType.id == m_Systems[SYS_C64_SID_6581].id) // Using Commodore 64
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
        else if (systemType.id == m_Systems[SYS_GAMEBOY].id) // Using Game Boy
        {
            inst.stdGBEnvVol = RI;
            inst.stdGBEnvDir = RI;
            inst.stdGBEnvLen = RI;
            inst.stdGBSoundLen = RI;
        }
    }

    return inst;
}

void DMF::LoadWavetablesData(uint8_t **fBuff, uint32_t *pos)
{
    m_TotalWavetables = RI;
    
    m_WavetableSizes = new uint32_t[m_TotalWavetables];
    m_WavetableValues = new uint32_t*[m_TotalWavetables];

    for (int i = 0; i < m_TotalWavetables; i++)
    {
        m_WavetableSizes[i] = RI;
        m_WavetableSizes[i] |= RI << 8;
        m_WavetableSizes[i] |= RI << 16;
        m_WavetableSizes[i] |= RI << 24;

        m_WavetableValues[i] = new uint32_t[m_WavetableSizes[i]];

        for (unsigned j = 0; j < m_WavetableSizes[i]; j++)
        {
            m_WavetableValues[i][j] = RI;
            m_WavetableValues[i][j] |= RI << 8;
            m_WavetableValues[i][j] |= RI << 16;
            m_WavetableValues[i][j] |= RI << 24;
        }
    }
}

void DMF::LoadPatternsData(uint8_t **fBuff, uint32_t *pos)
{
    // patternValues[channel][pattern number][pattern row number]
    m_PatternValues = new PatternRow**[m_System.channels];
    m_ChannelEffectsColumnsCount = new uint8_t[m_System.channels];
    
    uint8_t patternMatrixNumber;

    for (int channel = 0; channel < m_System.channels; channel++)
    {
        m_ChannelEffectsColumnsCount[channel] = RI;

        // Maybe use calloc instead of malloc in the line below?
        m_PatternValues[channel] = new PatternRow*[m_PatternMatrixMaxValues[channel] + 1];
        for (int i = 0; i < m_PatternMatrixMaxValues[channel] + 1; i++)
        {
            m_PatternValues[channel][i] = nullptr;
        }

        for (int rowInPatternMatrix = 0; rowInPatternMatrix < m_ModuleInfo.totalRowsInPatternMatrix; rowInPatternMatrix++)
        {
            patternMatrixNumber = m_PatternMatrixValues[channel][rowInPatternMatrix];
            if (m_PatternValues[channel][patternMatrixNumber]) // If pattern has been loaded previously
            {
                *pos += (8 + 4 * m_ChannelEffectsColumnsCount[channel]) * m_ModuleInfo.totalRowsPerPattern; // Unnecessary information
                continue; // Skip patterns that have already been loaded 
            }

            m_PatternValues[channel][patternMatrixNumber] = new PatternRow[m_ModuleInfo.totalRowsPerPattern];
            for (uint32_t row = 0; row < m_ModuleInfo.totalRowsPerPattern; row++)
            {
                m_PatternValues[channel][patternMatrixNumber][row] = LoadPatternRow(fBuff, pos, m_ChannelEffectsColumnsCount[channel]);
            }
        }
    }
}

PatternRow DMF::LoadPatternRow(uint8_t **fBuff, uint32_t *pos, int effectsColumnsCount)
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

void DMF::LoadPCMSamplesData(uint8_t **fBuff, uint32_t *pos)
{
    m_TotalPCMSamples = RI;
    m_PCMSamples = new PCMSample[m_TotalPCMSamples];

    for (int sample = 0; sample < m_TotalPCMSamples; sample++)
    {
        m_PCMSamples[sample] = LoadPCMSample(fBuff, pos);
    }
}

PCMSample DMF::LoadPCMSample(uint8_t **fBuff, uint32_t *pos)
{
    PCMSample sample;

    sample.size = RI;
    sample.size |= RI << 8;
    sample.size |= RI << 16;
    sample.size |= RI << 24;

    uint8_t name_size = RI;
    sample.name = new char[name_size];
    strncpy(sample.name, (char *)(&(*fBuff)[*pos]), name_size + 1);
    sample.name[name_size] = '\0';
    *pos += name_size;

    sample.rate = RI;
    sample.pitch = RI;
    sample.amp = RI;
    sample.bits = RI;

    sample.data = new uint16_t[sample.size];
    for (uint32_t i = 0; i < sample.size; i++)
    {
        sample.data[i] = RI;
        sample.data[i] |= RI << 8;
    }

    return sample;
}

double DMF::GetBPM()
{
    // Returns the initial BPM of the module
    unsigned int globalTick;
    if (m_ModuleInfo.usingCustomHZ)
    {
        if (m_ModuleInfo.customHZValue1 == 0) // No digits filled in
        {
            // NTSC is used by default if custom global tick box is selected but the value is left blank:
            globalTick = 60;
        }
        else if (m_ModuleInfo.customHZValue2 == 0) // One digit filled in
        {
            globalTick = (m_ModuleInfo.customHZValue1 - 48);
        }
        else if (m_ModuleInfo.customHZValue3 == 0) // Two digits filled in
        {
            globalTick = (m_ModuleInfo.customHZValue1 - 48) * 10 + (m_ModuleInfo.customHZValue2 - 48);
        }
        else // All three digits filled in
        {
            globalTick = (m_ModuleInfo.customHZValue1 - 48) * 100 + (m_ModuleInfo.customHZValue2 - 48) * 10 + (m_ModuleInfo.customHZValue3 - 48);
        }
    }
    else
    {
        globalTick = m_ModuleInfo.framesMode ? 60 : 50; // NTSC (60 Hz) or PAL (50 Hz)
    }
    
    // Experimentally determined equation for BPM:
    return (15.0 * globalTick) / ((m_ModuleInfo.timeBase + 1) * (m_ModuleInfo.tickTime1 + m_ModuleInfo.tickTime2));
}

int8_t NoteCompare(const Note *n1, const Note *n2)
{
    // Compares notes n1 and n2
    // Assumes note isn't Note OFF or Empty note
    // Notes must use the DMF convention where the note C# is the 1st note of an octave rather than C-

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

int8_t NoteCompare(const Note* n1, const Note n2)
{
    return NoteCompare(n1, &n2);
}

