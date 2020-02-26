/*
dmf2mod.c
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Converts Deflemask's GameBoy .dmf files to ProTracker's .mod files.

Usage: .\dmf2mod.exe output_file.mod deflemask_gameboy_file.dmf
*/

#include <stdio.h> 
#include <stdlib.h>
#include <math.h> 
#include <string.h>
#include <stdbool.h>

#include "dmf.h" 
#include "mod.h" 

#define DMF2MOD_VERSION "0.1" 

#ifndef CMD_Options 
    typedef struct CMD_Options {
        bool useEffects; 
    } CMD_Options;
    #define CMD_Options CMD_Options
#endif

void printHelp(); 

int main(int argc, char* argv[])
{
    char *fin, *fout; 
    CMD_Options opt;
    opt.useEffects = true; // By default, use Deflemask effects column   

    if (argc == 1) 
    {
        printHelp(); 
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
            printf("Error: Format must be: .\\dmf2mod.exe output_file.mod deflemask_gameboy_file.dmf [options]\n");
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
                if (strcmp(argv[i], "--noeffects") == 0) 
                {
                    opt.useEffects = false; 
                } 
                else 
                {
                    printf("Error: Unrecognized option '%s'.\n", argv[i]); 
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

void printHelp()
{
    printf("dmf2mod v%s \nCreated by Dalton Messmer <messmer.dalton@gmail.com>\n", DMF2MOD_VERSION);
    printf("Usage: .\\dmf2mod.exe output_file.mod deflemask_gameboy_file.dmf [options]\n");
    printf("Options:\n");
    printf("%-25s%s\n", "--help", "Display this help message.");
    printf("%-25s%s\n", "--noeffects", "Ignore Deflemask effects column(s) during conversion."); 
}
