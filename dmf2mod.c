/*
dmf2mod.c
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Converts Deflemask's Game Boy .dmf files to ProTracker's .mod files.

Usage: .\dmf2mod.exe output_file.mod deflemask_game_boy_file.dmf [options]
*/

#include <stdio.h> 
#include <stdlib.h>
#include <math.h> 
#include <string.h>
#include <stdbool.h>

#include "dmf.h" 
#include "mod.h" 

#define DMF2MOD_VERSION "0.1" 

void printHelp(); 

int main(int argc, char* argv[])
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
            printHelp();
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
    
    // Allocate space for storing the contents of one .dmf file 
    DMFContents *dmf = malloc(1 * sizeof(DMFContents));  

    // Import the inflated .dmf file
    if (importDMF(fin, dmf, opt)) 
    {
        // Error occurred during import  
        freeDMF(dmf);
        free(fin); 
        free(fout);
        exit(1);
    }
    
    // Export to a .mod file 
    if (exportMOD(fout, dmf, opt))
    {
        // Error occurred during export 
        freeDMF(dmf);
        free(fin); 
        free(fout); 
        exit(1);
    } 

    // Deallocate memory 
    freeDMF(dmf);
    free(fin); 
    free(fout);

    return 0; 
}

void printHelp(char* argv[])
{
    printf("dmf2mod v%s \nCreated by Dalton Messmer <messmer.dalton@gmail.com>\n", DMF2MOD_VERSION);
    printf("Usage: %s output_file.mod deflemask_game_boy_file.dmf [options]\n", argv[0]);
    printf("Options:\n");
    printf("%-25s%s\n","--downsample", "Allow wavetables to lose information through downsampling.");
    printf("%-25s%s\n", "--effects=<MIN, MAX>", "The number of ProTracker effects to use. (Default: MAX)"); 
    printf("%-25s%s\n", "--help", "Display this help message.");   
}
