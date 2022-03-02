/*
    dmf2mod.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Cross-platform command-line frontend for the dmf2mod core.

    Usage:
        dmf2mod output.[ext] input.[ext] [options]
        dmf2mod [ext] input.[ext] [options]
*/

#include "modules.h"

int main(int argc, char *argv[])
{
    ModuleUtils::RegisterModules();

    InputOutput io;
    ConversionOptionsPtr options;

    bool failure = ModuleUtils::ParseArgs(argc, argv, io, options);
    if (failure)
        return 1;

    // A help message was printed or some other action that doesn't require conversion
    if (!options)
        return 0;

    ////////// IMPORT //////////

    // Import the input file by inferring module type:
    ModulePtr input = Module::CreateAndImport(io.InputFile);
    if (!input)
    {
        std::cerr << "ERROR: The input module type is not registered.\n";
        return 1;
    }

    if (input->HandleResults())
        return 1;

    ////////// CONVERT //////////

    // Convert the input module to the output module type:
    ModulePtr output = input->Convert(io.OutputType, options);
    if (!output)
    {
        std::cerr << "ERROR: The output module type is not registered.\n";
        return 1;
    }

    if (output->HandleResults())
        return 1;

    ////////// EXPORT //////////

    // Export the converted module to disk:
    output->Export(io.OutputFile);
    if (output->HandleResults())
        return 1;
    
    return 0;
}
