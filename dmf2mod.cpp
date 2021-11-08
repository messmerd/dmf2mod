/*
    dmf2mod.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Cross-platform command-line implementation of the 
    dmf2mod core.

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

    // Import the input file by inferring module type:
    ModulePtr input = Module::CreateAndImport(io.InputFile);
    if (!input)
    {
        std::cerr << "ERROR: The input module type is not registered.\n";
        return 1;
    }
    
    if (input->GetStatus().Failed())
    {
        // Error occurred during import
        input->GetStatus().PrintAll();
        return 1;
    }

    if (input->GetStatus().WarningsIssued())
    {
        std::cout << "Warning(s) issued during load:\n";
        input->GetStatus().PrintWarnings();
    }

    // Convert the input module to the output module type:
    ModulePtr output = input->Convert(io.OutputType, options);
    if (!output)
    {
        std::cerr << "ERROR: The output module type is not registered.\n";
        return 1;
    }

    if (!output || output->GetStatus().Failed())
    {
        // Error/warnings occurred during conversion
        output->GetStatus().PrintAll();
        return 1;
    }

    if (output->GetStatus().WarningsIssued())
    {
        std::cout << "Warning(s) issued during conversion:\n";
        output->GetStatus().PrintWarnings();
    }

    // Export the converted module to disk:
    if (output->Export(io.OutputFile))
    {
        std::cerr << "ERROR: Failed to export the module to disk.\n";
        return 1; // Error occurred while exporting
    }

    return 0;
}
