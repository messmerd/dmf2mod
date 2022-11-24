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

using namespace d2m;

// Used for returning input/output info when parsing command-line arguments
struct InputOutput
{
    std::string input_file;
    ModuleType input_type;
    std::string output_file;
    ModuleType output_type;
};

enum class OperationType
{
    kError,
    kInfo,
    kConversion
};

static OperationType ParseArgs(std::vector<std::string>& args, InputOutput& io);
static void PrintHelp(const std::string& executable, ModuleType module_type);

int main(int argc, char *argv[])
{
    auto args = Utils::GetArgsAsVector(argc, argv);

    InputOutput io;
    OperationType operation_type = ParseArgs(args, io);
    if (operation_type == OperationType::kError)
        return 1;

    // A help message was printed or some other action that doesn't require conversion
    if (operation_type == OperationType::kInfo)
        return 0;

    ConversionOptionsPtr options = Factory<ConversionOptions>::Create(io.output_type);
    if (!options)
    {
        std::cerr << "ERROR: Failed to create ConversionOptionsBase-derived object for the module type '" << Utils::GetFileExtension(io.output_file) 
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

    ModulePtr input = Factory<ModuleBase>::Create(io.input_type);
    if (!input)
    {
        std::cerr << "ERROR: Not enough memory.\n";
        return 1;
    }

    ////////// IMPORT //////////

    // Import the input file by inferring module type:
    input->Import(io.input_file);
    if (input->HandleResults())
        return 1;

    ////////// CONVERT //////////

    // Convert the input module to the output module type:
    ModulePtr output = input->Convert(io.output_type, options);
    if (!output)
    {
        std::cerr << "ERROR: Not enough memory or input and output types are the same.\n";
        return 1;
    }

    if (output->HandleResults())
        return 1;

    ////////// EXPORT //////////

    // Export the converted module to disk:
    output->Export(io.output_file);
    if (output->HandleResults())
        return 1;

    return 0;
}

OperationType ParseArgs(std::vector<std::string>& args, InputOutput& io)
{
    io.input_file = "";
    io.input_type = ModuleType::kNone;
    io.output_file = "";
    io.output_type = ModuleType::kNone;

    const size_t arg_count = args.size();

    if (arg_count == 1)
    {
        PrintHelp(args[0], ModuleType::kNone);
        return OperationType::kInfo;
    }
    else if (arg_count == 2)
    {
        if (args[1] == "--help")
        {
            PrintHelp(args[0], ModuleType::kNone);
            return OperationType::kInfo;
        }
        else if (args[1] == "-v" || args[1] == "--version")
        {
            std::cout << kVersion << "\n";
            return OperationType::kInfo;
        }
        else
        {
            std::cerr << "ERROR: Could not parse command-line arguments.\n";
            return OperationType::kError;
        }
    }
    else if (arg_count >= 3) // 3 is the minimum needed to perform a conversion
    {
        if (args[1] == "--help")
        {
            PrintHelp(args[0], Utils::GetTypeFromFileExtension(args[2]));
            return OperationType::kInfo;
        }

        std::vector<std::string> args_only_flags(args.begin() + 3, args.end());

        if (GlobalOptions::Get().ParseArgs(args_only_flags, true))
            return OperationType::kError;

        if (GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kVerbose).GetValue<bool>()) // If --verbose=true
        {
            const bool help_provided = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kHelp).GetExplicitlyProvided();
            if (help_provided)
                std::cout << "Ignoring the \"--help\" command.\n";

            const bool version_provided = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kVersion).GetExplicitlyProvided();
            if (version_provided)
                std::cout << "Ignoring the \"--version\" command.\n";
        }

        const bool force = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kForce).GetValue<bool>();

        std::string output_file, input_file;

        // Get input file
        if (Utils::FileExists(args[2]))
        {
            if (Utils::GetTypeFromFilename(args[2]) != ModuleType::kNone)
            {
                input_file = args[2];
            }
            else
            {
                std::cerr << "ERROR: Input file type '" << Utils::GetFileExtension(args[2]) << "' is unsupported.\n";
                return OperationType::kError;
            }
        }
        else
        {
            std::cerr << "ERROR: The input file '" << args[2] << "' could not be found.\n";
            return OperationType::kError;
        }

        // Get output file
        if (Utils::GetFileExtension(args[1]).empty())
        {
            if (Utils::GetTypeFromFileExtension(args[1]) != ModuleType::kNone)
            {
                const size_t dot_pos = input_file.rfind('.');
                if (dot_pos == 0 || dot_pos + 1 >= input_file.size())
                {
                    std::cerr << "ERROR: The input file is invalid.\n";
                    return OperationType::kError;
                }

                // Construct output filename from the input filename
                output_file = input_file.substr(0, dot_pos + 1) + args[1];
            }
            else
            {
                std::cerr << "ERROR: Output file type '" << args[1] << "' is unsupported.\n";
                return OperationType::kError;
            }
        }
        else
        {
            output_file = args[1];
            if (Utils::GetTypeFromFilename(args[1]) == ModuleType::kNone)
            {
                std::cerr << "ERROR: '" << Utils::GetFileExtension(args[1]) << "' is not a valid module type.\n";
                return OperationType::kError;
            }
        }

        if (Utils::FileExists(output_file) && !force)
        {
            std::cerr << "ERROR: The output file '" << output_file << "' already exists. Run with the '-f' flag to allow the file to be overwritten.\n";
            return OperationType::kError;
        }

        io.input_file = input_file;
        io.input_type = Utils::GetTypeFromFilename(input_file);
        io.output_file = output_file;
        io.output_type = Utils::GetTypeFromFilename(output_file);

        if (io.input_type == io.output_type)
        {
            std::cout << "The output file is the same type as the input file. No conversion necessary.\n";
            return OperationType::kError;
        }

        // TODO: Check if a conversion between the two types is possible

        // At this point, the input and output file arguments have been deemed valid

        // Remove executable, output file, and input file from the args list, since they've already been processed
        // What is left are module-specific command-line arguments
        args = args_only_flags;

        return OperationType::kConversion;
    }

    return OperationType::kError;
}

void PrintHelp(const std::string& executable, ModuleType module_type)
{
    // If module-specific help was requested
    if (module_type != ModuleType::kNone)
    {
        ConversionOptions::PrintHelp(module_type);
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
