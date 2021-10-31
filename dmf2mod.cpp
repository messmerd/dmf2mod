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
    Module input = Module::CreateAndLoad(io.InputFile);
    if (input.GetStatus().Failed())
    {
        // Error occurred during import
        return 1;
    }

    Module output = input.Convert(io.OutputType, options);
    if (output.GetStatus().Failed())
    {
        // Error occurred during conversion
        return 1;
    }

    if (output.Save(io.OutputFile))
        return 1; // Error occurred while saving

    return 0;
}
