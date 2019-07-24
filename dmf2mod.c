/*
dmf2mod.c
Written by Dalton Messmer <messmer.dalton@gmail.com>. 

Converts Deflemask's GameBoy .dmf files to ProTracker's .mod files.

Usage: dmf2mod output_file.mod deflemask_gameboy_file.dmf
*/

#include <stdio.h> 
#include <stdlib.h>
#include <math.h> 
#include <string.h>

#include "dmf.h" 
#include "mod.h" 

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("dmf2mod v0.1 \nCreated by Dalton Messmer <messmer.dalton@gmail.com>\n");
        printf("Usage: dmf2mod output_file.mod deflemask_gameboy_file.dmf\n");
        exit(1);
    }

    char *fin = strdup(argv[2]); 
    char *fout = strdup(argv[1]);  
    
    // Allocate space for storing the contents of one .dmf file 
    DMFContents *dmf = malloc(1 * sizeof(DMFContents));  

    // Import the inflated .dmf file
    if (importDMF(fin, dmf)) 
    {
        // Error occurred during import  
        freeDMF(dmf);
        free(fin); 
        free(fout);
        exit(1);
    }
    
    // Export to a .mod file 
    if (exportMOD(fout, dmf))
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


