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
    ConversionOptions options;

    bool failure = ModuleUtils::ParseArgs(argc, argv, io, options);
    if (failure)
        return 1;

    // A help message was printed or some other action that doesn't require conversion
    if (!options)
        return 0;

    /*
    // Import the input file using more explicit way:
    Module input = Module::Create(io.InputType);
    if (input.Load(io.InputFile.c_str()))
    {
        // Error occurred during import
        return 1;
    }
    */

    // Import the input file by inferring module type
    Module input = Module::Create(io.InputFile);
    if (!input)
    {
        // Error occurred during import
        return 1;
    }

    // Export to a MOD file
    if (exportMOD(io.OutputFile.c_str(), input, options).error.errorCode != MOD_ERROR_NONE)
    {
        // Error occurred during export
        cleanUp();
        return 1;
    }

    // Deallocate memory
    cleanUp();
    return 0;
}
