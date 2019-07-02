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

struct Instrument
{
    unsigned char nameLength;
    char *name;
    unsigned char mode;  // 0 = STANDARD INS, 1 = FM INS 


};


/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int inf(FILE *source, FILE *dest)
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
        printf("File loaded.\n");
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
    //fread(fBuff, 1, sz, fptrIn);  // Read entire file, putting it in fBuff    
    //printf("sz=%u\n", sz);

    //ong pos = 0;   // Just use ftell(fptrIn) instead 


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
    char totalRowsPerPattern[4];
    fgets(totalRowsPerPattern, 5, fptrIn);
    int totalRowsInPatternMatrix = fgetc(fptrIn); 

    printf("timeBase: %u\n", timeBase);    // In Def. it says 1, but here it gives 0.
    printf("tickTime1: %u\n", tickTime1);  // Good 
    printf("tickTime2: %u\n", tickTime2);  // Good 
    printf("framesMode: %u\n", framesMode);  // If this is called "Step" in Def., then this is good 
    printf("usingCustomHZ: %u\n", usingCustomHZ);    // Whether the "Custom" clock box is checked? 
    printf("customHZValue1: %u\n", customHZValue1);  // Hz clock - 1st digit?
    printf("customHZValue2: %u\n", customHZValue2);  // Hz clock - 2nd digit?
    printf("customHZValue3: %u\n", customHZValue3);  // Hz clock - 3rd digit?

    printf("totalRowsPerPattern[0]: %u\n", totalRowsPerPattern[0]);  // Says 64, which is what "Rows" is  
    printf("totalRowsPerPattern[1]: %u\n", totalRowsPerPattern[1]);  // Says 0 
    printf("totalRowsPerPattern[2]: %u\n", totalRowsPerPattern[2]);  // Says 0 
    printf("totalRowsPerPattern[3]: %u\n", totalRowsPerPattern[3]);  // Says 0 

    printf("totalRowsInPatternMatrix: %u or %x\n", totalRowsInPatternMatrix, totalRowsInPatternMatrix); // Good. 

    // In previous .dmp versions, arpeggio tick speed is here! 

    ///////////////// PATTERN MATRIX VALUES (A matrix of SYSTEM_TOTAL_CHANNELS x TOTAL_ROWS_IN_PATTERN_MATRIX)

    // Format: patterMatrixValues[columns (channel)][rows] 
    unsigned char **patternMatrixValues = (unsigned char **)malloc(sys.channels * sizeof(unsigned char)); 

    for (int i = 0; i < sys.channels; i++)
    {
        patternMatrixValues[i] = (unsigned char *)malloc(totalRowsInPatternMatrix * sizeof(unsigned char));
        for (int j = 0; j < totalRowsInPatternMatrix; j++)
        {
            patternMatrixValues[i][j] = fgetc(fptrIn); 
        }
    }

    ///////////////// INSTRUMENTS DATA 






    // .... To be completed....

    /*
    int c;
    unsigned int pos = 0;
    while (1) {
        c = fgetc(fptrIn);
        if (feof(fptrIn)) {
            break;
        }
        //printf("%c", c);

        if (pos >= 0 && pos <= 15)
        {
            printf("%u", c);
        }
        pos++;
   }*/

   fclose(fptrIn);


    return 0; 
}


