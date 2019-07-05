/*
dmf2mod.c
Written by Dalton Messmer. 
Converts Deflemask .dmf files to .mod tracker files. 
*/

// Work needed: Currently I am loading the inflated zlib-format .dmf file into a buffer twice: Once 
//   in the inf method and again in main. This should be changed. Also, should I have a '~'-prefixed 
//   inflated zlib-format file at all if I am just using buffers? Is there any reason to keep it 
//   permanently? 

#include <stdio.h> 
#include <stdlib.h>
#include <math.h> 
#include <string.h>
#include <assert.h>
#include "zlib.h"

#include "system_info.h"
#include "instruments.h" 
#include "patterns.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif


const char* get_filename_ext(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

#define CHUNK 16384

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int inf(FILE *source, FILE *dest) // was FILE *dest)
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

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
            
            //strncpy(dest, out, have);  // New. testing  
            
            
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
            
            
        } while (strm.avail_out == 0);
        
        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

/* report a zlib or i/o error */
void zerr(int ret)
{
    fputs("zpipe: ", stderr);
    switch (ret) {
    case Z_ERRNO:
        if (ferror(stdin))
            fputs("error reading stdin\n", stderr);
        if (ferror(stdout))
            fputs("error writing stdout\n", stderr);
        break;
    case Z_STREAM_ERROR:
        fputs("invalid compression level\n", stderr);
        break;
    case Z_DATA_ERROR:
        fputs("invalid or incomplete deflate data\n", stderr);
        break;
    case Z_MEM_ERROR:
        fputs("out of memory\n", stderr);
        break;
    case Z_VERSION_ERROR:
        fputs("zlib version mismatch!\n", stderr);
    }
}

int main(int argc, char* argv[])
{
    char *inFileRaw, *inFileInflated, *outFile; 

    printf("Start\n");

    if (argc != 3)
    {
        printf("dmf2mod v0.1 \nCreated by Dalton Messmer <messmer.dalton@gmail.com>\n");
        printf("Usage: dmf2mod output_file.mod deflemask_file.dmf\n");
        return 0;
    }

    inFileRaw = strdup(argv[2]);

    inFileInflated = malloc(sizeof(argv[2]) + 1);
    *inFileInflated = '~';     // Same as inFileRaw, but with a '~' prefix (a temporary file?)
    strcpy(inFileInflated + 1, inFileRaw);
    
    outFile = strdup(argv[1]);

    printf("old: %s\n", inFileRaw);
    printf("new: %s\n", inFileInflated);
    FILE *fptrRaw = fopen(inFileRaw, "rb");

    if (fptrRaw == NULL) 
    {
        printf("File not found.\n");
        return 1;
        //exit(1); 
    }
    else
    {
        printf("File found.\n");
    }
    
    if (strcmp(get_filename_ext(inFileRaw), "dmf") != 0)
    {
        printf("Input file has the wrong file extension.\nPlease use a .dmf file.\n");
        return 1;
        //exit(1);
    }
    else
    {
        printf("File extension is dmf....good.\n");
    }

    if (get_filename_ext(outFile) != "mod")
    {
        strcat(outFile, ".mod"); // Add ".mod" extension if it wasn't specified in the argument 
    }
    
    FILE* fptrIn = fopen(inFileInflated, "wb");
    int ret = inf(fptrRaw, fptrIn);
    
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
    
    fclose(fptrRaw);

    fclose(fptrIn);
    fptrIn = fopen(inFileInflated, "rb");

    fseek(fptrIn, 0L, SEEK_END);
    long sz = ftell(fptrIn);   // How large the file is 
    unsigned char *fBuff = malloc(sz); // File buffer 
    printf("File has %u bytes.\n", sz);

    rewind(fptrIn);

    ///////////////// FORMAT FLAGS  

    fgets(fBuff, 17, fptrIn); 
    if (strncmp(fBuff, ".DelekDefleMask.", 17) == 0)
    {
        printf("Format header is good.\n");
    }
    else
    {
        printf(fBuff);
        printf("Format header is bad.\n");
        return 1;
    }

    unsigned char dmfFileVersion = fgetc(fptrIn); 
    printf(".dmf File Version: %u\n", dmfFileVersion); 

    ///////////////// SYSTEM SET  

    System sys = getSystem(fgetc(fptrIn)); 
    printf("System: %s (channels: %u)\n", sys.name, sys.channels);

    ///////////////// VISUAL INFORMATION 

    int songNameLength = fgetc(fptrIn);    
    char *songName = malloc(songNameLength); 
    fgets(songName, songNameLength + 1, fptrIn); 
    //printf("len: %u\n", songNameLength);
    printf("Title: %s\n", songName);

    int songAuthorLength = fgetc(fptrIn);    
    char *songAuthor = malloc(songAuthorLength); 
    fgets(songAuthor, songAuthorLength + 1, fptrIn); 
    //printf("len: %u\n", songAuthorLength);
    printf("Author: %s\n", songAuthor);

    unsigned char highlightAPatterns = fgetc(fptrIn);  
    unsigned char highlightBPatterns = fgetc(fptrIn); 

    ///////////////// MODULE INFORMATION  

    int timeBase = fgetc(fptrIn);   
    int tickTime1 = fgetc(fptrIn); 
    int tickTime2 = fgetc(fptrIn); 
    int framesMode = fgetc(fptrIn); 
    int usingCustomHZ = fgetc(fptrIn); 
    int customHZValue1 = fgetc(fptrIn); 
    int customHZValue2 = fgetc(fptrIn); 
    int customHZValue3 = fgetc(fptrIn); 
    uint32_t totalRowsPerPattern = fgetc(fptrIn); 
    totalRowsPerPattern |= fgetc(fptrIn) << 8;
    totalRowsPerPattern |= fgetc(fptrIn) << 16;
    totalRowsPerPattern |= fgetc(fptrIn) << 24;
    int totalRowsInPatternMatrix = fgetc(fptrIn); 

    printf("timeBase: %u\n", timeBase);    // In Def. it says 1, but here it gives 0.
    printf("tickTime1: %u\n", tickTime1);  // Good 
    printf("tickTime2: %u\n", tickTime2);  // Good 
    printf("framesMode: %u\n", framesMode);  // If this is called "Step" in Def., then this is good 
    printf("usingCustomHZ: %u\n", usingCustomHZ);    // Whether the "Custom" clock box is checked? 
    printf("customHZValue1: %u\n", customHZValue1);  // Hz clock - 1st digit?
    printf("customHZValue2: %u\n", customHZValue2);  // Hz clock - 2nd digit?
    printf("customHZValue3: %u\n", customHZValue3);  // Hz clock - 3rd digit?

    printf("totalRowsPerPattern: %u\n", totalRowsPerPattern);  // Says 64, which is what "Rows" is  
    printf("totalRowsInPatternMatrix: %u or %x\n", totalRowsInPatternMatrix, totalRowsInPatternMatrix); // Good. 

    // In previous .dmp versions, arpeggio tick speed is here! 

    ///////////////// PATTERN MATRIX VALUES 

    // Format: patterMatrixValues[channel][pattern matrix row] 
    uint8_t **patternMatrixValues = (uint8_t **)malloc(sys.channels * sizeof(uint8_t *)); 
    uint8_t *patternMatrixMaxValues = (uint8_t *)malloc(sys.channels * sizeof(uint8_t)); 

    for (int i = 0; i < sys.channels; i++)
    {
        patternMatrixMaxValues[i] = 0; 
        patternMatrixValues[i] = (uint8_t *)malloc(totalRowsInPatternMatrix * sizeof(uint8_t));
        for (int j = 0; j < totalRowsInPatternMatrix; j++)
        {
            patternMatrixValues[i][j] = fgetc(fptrIn); 
            if (patternMatrixValues[i][j] > patternMatrixMaxValues[i]) 
            {
                patternMatrixMaxValues[i] = patternMatrixValues[i][j]; 
            }
        }
    }
    
    ///////////////// INSTRUMENTS DATA 

    unsigned char totalInstruments = fgetc(fptrIn);
    Instrument* instruments = (Instrument *)malloc(totalInstruments * sizeof(Instrument)); 

    for (int i = 0; i < totalInstruments; i++)
    {
        instruments[i] = loadInstrument(fptrIn, sys);
    }

    printf("Loaded instruments.\n");

    ///////////////// WAVETABLES DATA

    unsigned char totalWavetables = fgetc(fptrIn); 
    
    uint32_t *wavetableSizes = (uint32_t *)malloc(totalWavetables * sizeof(uint32_t));     
    uint32_t **wavetableValues = (uint32_t **)malloc(totalWavetables * sizeof(uint32_t *)); 

    for (int i = 0; i < totalWavetables; i++)
    {
        wavetableSizes[i] = fgetc(fptrIn); 
        wavetableSizes[i] |= fgetc(fptrIn) << 8;
        wavetableSizes[i] |= fgetc(fptrIn) << 16;
        wavetableSizes[i] |= fgetc(fptrIn) << 24;

        wavetableValues[i] = (uint32_t *)malloc(wavetableSizes[i] * sizeof(uint32_t)); 
        for (int j = 0; j < wavetableSizes[i]; j++)
        {
            wavetableValues[i][j] = fgetc(fptrIn); 
            wavetableValues[i][j] |= fgetc(fptrIn) << 8;
            wavetableValues[i][j] |= fgetc(fptrIn) << 16;
            wavetableValues[i][j] |= fgetc(fptrIn) << 24;
        }
    }
    printf("Loaded %u wavetable(s).\n", totalWavetables);

    ///////////////// PATTERNS DATA

    // patternValues[channel][pattern matrix number][pattern row number]
    PatternRow ***patternValues = (PatternRow ***)malloc(sys.channels * sizeof(PatternRow **)); 
    int *channelEffectsColumnsCount = (int *)malloc(sys.channels * sizeof(int));
    uint8_t patternMatrixNumber;

    for (int channel = 0; channel < sys.channels; channel++)
    {
        channelEffectsColumnsCount[channel] = fgetc(fptrIn); 

        patternValues[channel] = (PatternRow **)malloc((patternMatrixMaxValues[channel] + 1) * sizeof(PatternRow *));
        for (int i = 0; i < patternMatrixMaxValues[channel] + 1; i++) 
        {
            patternValues[channel][i] = NULL; 
        }

        for (int rowInPatternMatrix = 0; rowInPatternMatrix < totalRowsInPatternMatrix; rowInPatternMatrix++)
        {
            patternMatrixNumber = patternMatrixValues[channel][rowInPatternMatrix];
            if (patternValues[channel][patternMatrixNumber] != NULL) // If pattern has been loaded previously 
            {
                fseek(fptrIn, (8 + 4*channelEffectsColumnsCount[channel])*totalRowsPerPattern, SEEK_CUR); // Unnecessary information
                continue; // Skip patterns that have already been loaded 
            }

            patternValues[channel][patternMatrixNumber] = (PatternRow *)malloc(totalRowsPerPattern * sizeof(PatternRow));
            
            for (uint32_t row = 0; row < totalRowsPerPattern; row++)
            {
                patternValues[channel][patternMatrixNumber][row] = loadPatternRow(fptrIn, channelEffectsColumnsCount[channel]);
            }
        }
    }

    printf("Loaded patterns.\n");

    ///////////////// PCM SAMPLES DATA

    uint8_t totalPCMSamples = fgetc(fptrIn); 
    PCMSample *pcmSamples = (PCMSample *)malloc(totalPCMSamples * sizeof(PCMSample));

    for (int sample = 0; sample < totalPCMSamples; sample++) 
    {
        pcmSamples[sample] = loadPCMSample(fptrIn);
    }

    printf("Loaded PCM Samples.\nThe .dmf file has finished loading. \n");

    fclose(fptrIn);


    // Need to close files here!! (and anywhere the program might end prematurely)
    // Need to deallocate memory here!! (and anywhere the program might end prematurely)

    return 0; 
}


