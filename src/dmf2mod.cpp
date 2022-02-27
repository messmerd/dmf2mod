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
    ModulePtr input;
    try
    {
        // Import the input file by inferring module type:
        input = Module::CreateAndImport(io.InputFile);
        if (!input)
        {
            std::cerr << "ERROR: The input module type is not registered.\n";
            return 1;
        }
    }
    catch (const ModuleException& e)
    {
        // Error occurred during import
        e.Print();
        return 1;
    }
    
    if (input->GetStatus().WarningsIssued())
    {
        std::cout << "Warning(s) issued during load:\n";
        input->GetStatus().PrintWarnings();
    }

    ////////// CONVERT //////////
    ModulePtr output;
    try
    {
        // Convert the input module to the output module type:
        output = input->Convert(io.OutputType, options);
        if (!output)
        {
            std::cerr << "ERROR: The output module type is not registered.\n";
            return 1;
        }
    }
    catch (const ModuleException& e)
    {
        // Error occurred during conversion
        e.Print();
        return 1;
    }
    
    if (output->GetStatus().WarningsIssued())
    {
        std::cout << "Warning(s) issued during conversion:\n";
        output->GetStatus().PrintWarnings();
    }

    ////////// EXPORT //////////
    try
    {
        // Export the converted module to disk:
        output->Export(io.OutputFile);
    }
    catch (const ModuleException& e)
    {
        // Error occurred while exporting
        e.Print();
        return 1;
    }
    
    return 0;
}