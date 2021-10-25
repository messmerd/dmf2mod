/*
dmf2mod.c
Written by Dalton Messmer <messmer.dalton@gmail.com>.

Converts Deflemask's Game Boy DMF files to ProTracker's MOD files.

Usage: dmf2mod output_file.mod deflemask_game_boy_file.dmf [options]
*/

#include "modules.h"

int main(int argc, char *argv[])
{
    ModuleUtils::RegisterModules();

    InputOutput io;
    ConversionOptions* options = ModuleUtils::ParseArgs(argc, argv, io);

    if (!options)
        return 1;

    // Import the input file
    Module* input = Module::Create(io.InputType);
    if (input->Load(io.InputFile.c_str()))
    {
        // Error occurred during import
        delete input;
        delete options;
        return 1;
    }
    
    // Export to a MOD file
    if (exportMOD(io.OutputFile.c_str(), input->Cast<DMF>(), options).error.errorCode != MOD_ERROR_NONE)
    {
        // Error occurred during export
        delete input;
        delete options;
        cleanUp();
        return 1;
    }

    // Deallocate memory 
    delete input;
    delete options;
    cleanUp();
    return 0;
}
