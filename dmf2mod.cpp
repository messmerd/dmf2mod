/*
dmf2mod.c
Written by Dalton Messmer <messmer.dalton@gmail.com>.

Converts Deflemask's Game Boy DMF files to ProTracker's MOD files.

Usage: dmf2mod output_file.mod deflemask_game_boy_file.dmf [options]
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "dmf.h"
#include "mod.h"

#define DMF2MOD_VERSION "0.1"

void printHelp(char *argv[]);

int main(int argc, char *argv[])
{
    char *fin, *fout; 
    CMD_Options opt;
    opt.effects = 2; // By default, use maximum number of effects (2).  
    opt.allowDownsampling = false; // By default, wavetables cannot lose information through downsampling   

    if (argc == 1) 
    {
        printHelp(argv); 
        exit(0);
    }
    else if (argc == 2) 
    {
        if (strcmp(argv[1], "--help") == 0) 
        {
            printHelp(argv);
            exit(0);
        }
        else
        {
            printf("ERROR: Format must be: .\\dmf2mod.exe output_file.mod deflemask_game_boy_file.dmf [options]\n");
            exit(1);
        }
    }
    else if (argc >= 3) // 3 is the minimum needed to perform a conversion 
    {
        fin = strdup(argv[2]); 
        fout = strdup(argv[1]); 
        
        if (argc > 3) // Options were provided 
        {
            // Check which options were provided   
            for (int i = 3; i < argc; i++) 
            {
                if (strncmp(argv[i], "--effects=", 10) == 0) 
                {
                    if (strcmp(&(argv[i][10]), "MAX") == 0 || strcmp(&(argv[i][10]), "max") == 0)
                    {
                        opt.effects = 2; // Maximum effects 
                    }
                    else if (strcmp(&(argv[i][10]), "MIN") == 0 || strcmp(&(argv[i][10]), "min") == 0)
                    {
                        opt.effects = 1; // Minimum effects 
                    }
                    else
                    {
                        printf("ERROR: For the option '--effects=', the acceptable values are: MIN and MAX.\n");
                        free(fin);
                        free(fout);
                        exit(1);
                    }
                } 
                else if (strcmp(argv[i], "--downsample") == 0)
                {
                    opt.allowDownsampling = true; // Allow wavetables to lose information through downsampling
                }
                else 
                {
                    printf("ERROR: Unrecognized option '%s'\n", argv[i]);
                    free(fin);
                    free(fout);
                    exit(1);
                }
            }
        }
    } 
    
    const ModuleType inType = ModuleUtils::GetType(fin);
    if (inType != ModuleType::DMF)
    {
        printf("ERROR: Input is not a DMF file.\n");
        free(fin);
        free(fout);
        exit(1);
    }

    const ModuleType outType = ModuleUtils::GetType(fout);

    // Import the DMF file
    DMF* dmf = Module::Create<DMF>();
    if (dmf->Load(fin))
    {
        // Error occurred during import
        delete dmf;
        dmf = nullptr;
        free(fin);
        free(fout);
        exit(1);
    }
    
    // Export to a MOD file
    if (exportMOD(fout, dmf, opt).error.errorCode != MOD_ERROR_NONE)
    {
        // Error occurred during export
        delete dmf;
        dmf = nullptr;
        cleanUp();
        free(fin);
        free(fout);
        exit(1);
    }

    // Deallocate memory 
    delete dmf;
    dmf = nullptr;
    cleanUp();
    free(fin);
    free(fout);
    return 0;
}

void printHelp(char *argv[])
{
    printf("dmf2mod v%s \nCreated by Dalton Messmer <messmer.dalton@gmail.com>\n", DMF2MOD_VERSION);
    
    const char *ext = GetFilenameExt(argv[0]);
    if (strcmp(ext, ".exe") != 0) // If filename extension is not .exe
    {
        ext = strrchr(argv[0], '\0'); // Pointer to empty string
    }

    printf("Usage: dmf2mod%s output_file.mod deflemask_game_boy_file.dmf [options]\n", ext);
    printf("Options:\n");
    printf("%-25s%s\n","--downsample", "Allow wavetables to lose information through downsampling.");
    printf("%-25s%s\n", "--effects=<MIN, MAX>", "The number of ProTracker effects to use. (Default: MAX)");
    printf("%-25s%s\n", "--help", "Display this help message.");
}
