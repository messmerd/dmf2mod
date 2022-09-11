/*
    console.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Cross-platform command-line frontend for the dmf2mod core.

    Usage:
        dmf2mod output.[ext] input.[ext] [options]
        dmf2mod [ext] input.[ext] [options]
        dmf2mod [option]
*/

#include "dmf2mod.h"

#include <string>
#include <iostream>
#include <iomanip>

#include "utils.h"
#include "version.h"

using namespace d2m;

// Used for returning input/output info when parsing command-line arguments
struct InputOutput
{
    std::string InputFile;
    ModuleType InputType;
    std::string OutputFile;
    ModuleType OutputType;
};

enum class OperationType
{
    Error,
    Info,
    Conversion
};

static OperationType ParseArgs(std::vector<std::string>& args, InputOutput& inputOutputInfo);
static void PrintHelp(const std::string& executable, ModuleType moduleType);

int main(int argc, char *argv[])
{
    Initialize();

    auto args = Utils::GetArgsAsVector(argc, argv);

    InputOutput io;
    OperationType operationType = ParseArgs(args, io);
    if (operationType == OperationType::Error)
        return 1;

    // A help message was printed or some other action that doesn't require conversion
    if (operationType == OperationType::Info)
        return 0;

    ConversionOptionsPtr options = Factory<ConversionOptions>::Create(io.OutputType);
    if (!options)
    {
        std::cerr << "ERROR: Failed to create ConversionOptionsBase-derived object for the module type '" << Utils::GetFileExtension(io.OutputFile) 
            << "'. The module may not be properly registered with dmf2mod.\n";
        return 1;
    }

    if (!args.empty() && options->ParseArgs(args))
    {
        // An error occurred while parsing the module-specific arguments
        return 1;
    }

    if (!args.empty())
    {
        // All the arguments should have been consumed by this point but they aren't
        std::cerr << "ERROR: Unrecognized argument(s): ";
        for (unsigned i = 0; i < args.size(); i++)
        {
            std::cerr << args[i];
            if (i + 1 != args.size())
                std::cerr << ", ";
        }
        std::cerr << "\n";
        return 1;
    }

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

OperationType ParseArgs(std::vector<std::string>& args, InputOutput& inputOutputInfo)
{
    inputOutputInfo.InputFile = "";
    inputOutputInfo.InputType = ModuleType::NONE;
    inputOutputInfo.OutputFile = "";
    inputOutputInfo.OutputType = ModuleType::NONE;

    const size_t argCount = args.size();

    if (argCount == 1)
    {
        PrintHelp(args[0], ModuleType::NONE);
        return OperationType::Info;
    }
    else if (argCount == 2)
    {
        if (args[1] == "--help")
        {
            PrintHelp(args[0], ModuleType::NONE);
            return OperationType::Info;
        }
        else if (args[1] == "-v" || args[1] == "--version")
        {
            std::cout << DMF2MOD_VERSION << "\n";
            return OperationType::Info;
        }
        else
        {
            std::cerr << "ERROR: Could not parse command-line arguments.\n";
            return OperationType::Error;
        }
    }
    else if (argCount >= 3) // 3 is the minimum needed to perform a conversion
    {
        if (args[1] == "--help")
        {
            PrintHelp(args[0], Utils::GetTypeFromFileExtension(args[2]));
            return OperationType::Info;
        }

        std::vector<std::string> argsOnlyFlags(args.begin() + 3, args.end());

        if (GlobalOptions::Get().ParseArgs(argsOnlyFlags, true))
            return OperationType::Error;

        if (GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Verbose).GetValue<bool>()) // If --verbose=true
        {
            const bool helpProvided = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Help).GetExplicitlyProvided();
            if (helpProvided)
                std::cout << "Ignoring the \"--help\" command.\n";

            const bool versionProvided = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Version).GetExplicitlyProvided();
            if (versionProvided)
                std::cout << "Ignoring the \"--version\" command.\n";
        }

        const bool force = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Force).GetValue<bool>();

        std::string outputFile, inputFile;

        // Get input file
        if (Utils::FileExists(args[2]))
        {
            if (Utils::GetTypeFromFilename(args[2]) != ModuleType::NONE)
            {
                inputFile = args[2];
            }
            else
            {
                std::cerr << "ERROR: Input file type '" << Utils::GetFileExtension(args[2]) << "' is unsupported.\n";
                return OperationType::Error;
            }
        }
        else
        {
            std::cerr << "ERROR: The input file '" << args[2] << "' could not be found.\n";
            return OperationType::Error;
        }

        // Get output file
        if (Utils::GetFileExtension(args[1]).empty())
        {
            if (Utils::GetTypeFromFileExtension(args[1]) != ModuleType::NONE)
            {
                const size_t dotPos = inputFile.rfind('.');
                if (dotPos == 0 || dotPos + 1 >= inputFile.size())
                {
                    std::cerr << "ERROR: The input file is invalid.\n";
                    return OperationType::Error;
                }

                // Construct output filename from the input filename
                outputFile = inputFile.substr(0, dotPos + 1) + args[1];
            }
            else
            {
                std::cerr << "ERROR: Output file type '" << args[1] << "' is unsupported.\n";
                return OperationType::Error;
            }
        }
        else
        {
            outputFile = args[1];
            if (Utils::GetTypeFromFilename(args[1]) == ModuleType::NONE)
            {
                std::cerr << "ERROR: '" << Utils::GetFileExtension(args[1]) << "' is not a valid module type.\n";
                return OperationType::Error;
            }
        }

        if (Utils::FileExists(outputFile) && !force)
        {
            std::cerr << "ERROR: The output file '" << outputFile << "' already exists. Run with the '-f' flag to allow the file to be overwritten.\n";
            return OperationType::Error;
        }

        inputOutputInfo.InputFile = inputFile;
        inputOutputInfo.InputType = Utils::GetTypeFromFilename(inputFile);
        inputOutputInfo.OutputFile = outputFile;
        inputOutputInfo.OutputType = Utils::GetTypeFromFilename(outputFile);

        if (inputOutputInfo.InputType == inputOutputInfo.OutputType)
        {
            std::cout << "The output file is the same type as the input file. No conversion necessary.\n";
            return OperationType::Error;
        }

        // TODO: Check if a conversion between the two types is possible

        // At this point, the input and output file arguments have been deemed valid

        // Remove executable, output file, and input file from the args list, since they've already been processed
        // What is left are module-specific command-line arguments
        args = argsOnlyFlags;

        return OperationType::Conversion;
    }

    return OperationType::Error;
}

void PrintHelp(const std::string& executable, ModuleType moduleType)
{
    // If module-specific help was requested
    if (moduleType != ModuleType::NONE)
    {
        ConversionOptions::PrintHelp(moduleType);
        return;
    }

    // Else, print generic help

    std::cout.setf(std::ios_base::left);
    std::cout << "Usage: dmf2mod output.[ext] input.dmf [options]\n";
    std::cout << std::setw(7) << "" << "dmf2mod" << " [ext] input.dmf [options]\n";
    std::cout << std::setw(7) << "" << "dmf2mod" << " [option]\n";

    std::cout << "Options:\n";

    GlobalOptions::Get().GetDefinitions()->PrintHelp();
}
