/*
    utils.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines various utility methods used by dmf2mod.
*/

#include "utils.h"
#include "core/conversion_options.h"
#include "dmf2mod_config.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <map>
#include <functional>
#include <fstream>
#include <variant>
//#include <filesystem>

using namespace d2m;

CommonFlags ModuleUtils::m_CoreOptions = {};

static bool ParseFlags(std::vector<std::string>& args, CommonFlags& flags);

// ModuleUtils class

bool ModuleUtils::ParseArgs(int argc, char *argv[], InputOutput& inputOutputInfo, ConversionOptionsPtr& options)
{
    inputOutputInfo.InputFile = "";
    inputOutputInfo.InputType = ModuleType::NONE;
    inputOutputInfo.OutputFile = "";
    inputOutputInfo.OutputType = ModuleType::NONE;

    std::vector<std::string> args(argc, "");
    for (int i = 0; i < argc; i++)
    {
        args[i] = argv[i];
    }

    if (argc == 1)
    {
        return PrintHelp(args[0], ModuleType::NONE);
    }
    else if (argc == 2)
    {
        if (args[1] == "--help")
        {
            return PrintHelp(args[0], ModuleType::NONE);
        }
        else
        {
            std::cerr << "ERROR: Could not parse command-line arguments.\n";
            return true;
        }
    }
    else if (argc >= 3) // 3 is the minimum needed to perform a conversion
    {
        if (args[1] == "--help")
        {
            return PrintHelp(args[0], Registrar::GetTypeFromFileExtension(args[2]));
        }

        CommonFlags flags;
        if (ParseFlags(args, flags))
            return true;

        SetCoreOptions(flags);

        std::string outputFile, inputFile;
        
        // Get input file
        if (FileExists(args[2]))
        {
            if (Registrar::GetTypeFromFilename(args[2]) != ModuleType::NONE)
            {
                inputFile = args[2];
            }
            else
            {
                std::cerr << "ERROR: Input file type '" << GetFileExtension(args[2]) << "' is unsupported.\n";
                return true;
            }
        }
        else
        {
            std::cerr << "ERROR: The input file '" << args[2] << "' could not be found.\n";
            return true;
        }
        
        // Get output file
        if (GetFileExtension(args[1]).empty())
        {
            if (Registrar::GetTypeFromFileExtension(args[1]) != ModuleType::NONE)
            {
                const size_t dotPos = inputFile.rfind('.');
                if (dotPos == 0 || dotPos + 1 >= inputFile.size())
                {
                    std::cerr << "ERROR: The input file is invalid.\n";
                    return true;
                }

                // Construct output filename from the input filename
                outputFile = inputFile.substr(0, dotPos + 1) + args[1];
            }
            else
            {
                std::cerr << "ERROR: Output file type '" << args[1] << "' is unsupported.\n";
                return true;
            }
        }
        else
        {
            outputFile = args[1];
            if (Registrar::GetTypeFromFilename(args[1]) == ModuleType::NONE)
            {
                std::cerr << "ERROR: '" << GetFileExtension(args[1]) << "' is not a valid module type.\n";
                return true;
            }
        }
        
        if (FileExists(outputFile) && !flags.force)
        {
            std::cerr << "ERROR: The output file '" << outputFile << "' already exists. Run with the '-f' flag to allow the file to be overwritten.\n";
            return true;
        }

        inputOutputInfo.InputFile = inputFile;
        inputOutputInfo.InputType = Registrar::GetTypeFromFilename(inputFile);
        inputOutputInfo.OutputFile = outputFile;
        inputOutputInfo.OutputType = Registrar::GetTypeFromFilename(outputFile);

        if (inputOutputInfo.InputType == inputOutputInfo.OutputType)
        {
            std::cout << "The output file is the same type as the input file. No conversion necessary.\n";
            return true;
        }

        // TODO: Check if a conversion between the two types is possible

        // At this point, the input and output file arguments have been deemed valid

        // Remove executable, output file, and input file from the args list, since they've already been processed
        // What is left are module-specific command-line arguments
        args.erase(args.begin(), args.begin() + 3);
        argc -= 3;

        ConversionOptionsPtr optionsTemp = ConversionOptions::Create(inputOutputInfo.OutputType);
        if (!optionsTemp)
        {
            std::cerr << "ERROR: Failed to create ConversionOptionsBase-derived object for the module type '" << GetFileExtension(outputFile) 
                << "'. The module may not be properly registered with dmf2mod.\n";
            return true;
        }

        if (!args.empty() && optionsTemp->ParseArgs(args))
        {
            // An error occurred while parsing the module-specific arguments
            return true;
        }
        
        options = std::move(optionsTemp); // Invoke move assignment operator
        return false;
    }

    return true;
}

bool ParseFlags(std::vector<std::string>& args, CommonFlags& flags)
{
    flags = {};

    for (unsigned i = 3; i < args.size(); i++)
    {
        std::string& str = args[i];

        // Handle single character flags
        if (str.size() >= 2 && str[0] == '-' && str[1] != '-')
        {
            // Can have multiple flags together. For example "-fsd"
            for (unsigned j = 1; j < str.size(); j++)
            {
                char c = str[j];
                switch (c)
                {
                case 'f':
                    flags.force = true;
                    str.erase(j--, 1);
                    break;
                case 's':
                    flags.silent = true;
                    str.erase(j--, 1);
                    break;
                default:
                    // If a flag is not recognized, it may be a flag used by a module, so don't throw an error
                    break;
                }
            }

            if (str.size() == 1) // Only the '-' left
            {
                args.erase(args.begin() + i);
                i--; // Adjust for item that was removed
                continue;
            }
        }
        else if (str == "--force")
        {
            flags.force = true;
            args.erase(args.begin() + i);
            i--;
        }
        else if (str == "--silent")
        {
            flags.silent = true;
            args.erase(args.begin() + i);
            i--;
        }
    }

    return false;
}

std::string ModuleUtils::GetBaseNameFromFilename(const std::string& filename)
{
    // Filename must contain base name, a dot, then the extension
    if (filename.size() <= 2)
        return "";

    const size_t slashPos = filename.find_first_of("\\/");
    // If file separator is at the end:
    if (slashPos != std::string::npos && slashPos >= filename.size() - 2)
        return "";

    const size_t startPos = slashPos == std::string::npos ? 0 : slashPos + 1;

    const size_t dotPos = filename.rfind('.');
    // If dot is at start, not found, or at end:
    if (dotPos == 0 || dotPos == std::string::npos || dotPos + 1 >= filename.size())
        return "";

    // The dot should come after the start position
    if (startPos >= dotPos)
        return "";

    return filename.substr(startPos, dotPos - startPos);
}

std::string ModuleUtils::ReplaceFileExtension(const std::string& filename, const std::string& newFileExtension)
{
    // filename must contain a file extension
    // newFileExtension should not contain a dot

    const size_t dotPos = filename.rfind('.');
    // If dot is at start, not found, or at end:
    if (dotPos == 0 || dotPos == std::string::npos || dotPos + 1 >= filename.size())
        return "";

    return filename.substr(0, dotPos + 1) + newFileExtension;
}

std::string ModuleUtils::GetFileExtension(const std::string& filename)
{
    const size_t dotPos = filename.rfind('.');
    // If dot is at start, not found, or at end:
    if (dotPos == 0 || dotPos == std::string::npos || dotPos + 1 >= filename.size())
        return "";

    return filename.substr(dotPos + 1);
}

bool ModuleUtils::FileExists(const std::string& filename)
{
    std::ifstream file(filename);
    return file.is_open();
}

void ModuleUtils::PrintHelp(ModuleType moduleType)
{
    if (moduleType == ModuleType::NONE)
        return;

    std::string name = Registrar::GetExtensionFromType(moduleType);
    const ModuleOptions options = Registrar::GetAvailableOptions(moduleType);

    if (name.empty())
        return;

    for (auto& c : name)
    {
        c = toupper(c);
    }

    if (options.Count() == 0)
    {
        std::cout << name << " files have no conversion options.\n";
        return;
    }

    std::cout << name << " Options:\n";

    std::cout.setf(std::ios_base::left);

    const size_t total = options.Count();
    for (unsigned i = 0; i < total; i++)
    {
        const auto& option = options.Item(i);

        std::string str1 = "  ";
        str1 += option.HasShortName() ? "-" + option.GetShortName() + ", " : "";
        str1 += "--" + option.GetName();

        const ModuleOption::Type optionType = option.GetType();
        if (option.UsesAcceptedValues() && optionType != ModuleOption::BOOL)
        {
            str1 += "=[";
            
            unsigned i = 0;
            const size_t total = option.GetAcceptedValues().size();
            for (auto& val : option.GetAcceptedValues())
            {
                switch (optionType)
                {
                    case ModuleOption::INT:
                        str1 += std::to_string(std::get<ModuleOption::INT>(val)); break;
                    case ModuleOption::DOUBLE:
                        str1 += std::to_string(std::get<ModuleOption::DOUBLE>(val)); break;
                    case ModuleOption::STRING:
                        str1 += "\"" + std::get<ModuleOption::STRING>(val) + "\""; break;
                    default:
                        break;
                }

                if (i + 1 != total)
                    str1 += ", ";

                i++;
            }

            str1 += "]";
        }
        else
        {
            switch (optionType)
            {
                case ModuleOption::INT:
                case ModuleOption::DOUBLE:
                    str1 += "<value>"; break;
                case ModuleOption::STRING:
                    str1 += "\"<value>\""; break;
                default:
                    break;
            }
        }

        std::string str2 = option.GetDescription() + " ";
        switch (optionType)
        {
            case ModuleOption::INT:
                str2 += "(Default: ";
                str2 += std::to_string(std::get<ModuleOption::INT>(option.GetDefaultValue()));
                str2 += ")";
                break;
            case ModuleOption::DOUBLE:
                str2 += "(Default: ";
                str2 += std::to_string(std::get<ModuleOption::DOUBLE>(option.GetDefaultValue()));
                str2 += ")";
                break;
            case ModuleOption::STRING:
            {
                const std::string defaultValue = std::get<ModuleOption::STRING>(option.GetDefaultValue());
                if (!defaultValue.empty())
                {
                    str2 += "(Default: \"";
                    str2 += defaultValue;
                    str2 += "\")";
                }
                break;
            }
            default:
                break;
        }

        std::cout << std::setw(30) << str1 << str2 << "\n";
    }
}

bool ModuleUtils::PrintHelp(const std::string& executable, ModuleType moduleType)
{
    // If module-specific help was requested
    if (moduleType != ModuleType::NONE)
    {
        ConversionOptionsPtr options = ConversionOptions::Create(moduleType);
        if (!options)
        {
            std::string extension = Registrar::GetExtensionFromType(moduleType);
            if (extension.empty())
            {
                std::cerr << "ERROR: The module is not properly registered with dmf2mod.\n";
            }
            else
            {
                std::cerr << "ERROR: Failed to create ConversionOptions-derived object for the module type '" << extension 
                    << "'. The module may not be properly registered with dmf2mod.\n";
            }
            return true;
        }

        options->PrintHelp();
        return false;
    }

    // Print generic help

    std::cout << "dmf2mod v" << DMF2MOD_VERSION << "\n";
    std::cout << "Created by Dalton Messmer <messmer.dalton@gmail.com>\n\n";

    std::cout.setf(std::ios_base::left);
    std::cout << "Usage: dmf2mod output.[ext] input.dmf [options]\n";
    std::cout << std::setw(7) << "" << "dmf2mod" << " [ext] input.dmf [options]\n";

    std::cout << "Options:\n";

    std::cout.setf(std::ios_base::left);
    std::cout << std::setw(30) << "  -f, --force" << "Overwrite output file.\n";
    std::cout << std::setw(30) << "  --help [module type]" << "Display this help message. Provide module type (i.e. mod) for module-specific options.\n";
    std::cout << std::setw(30) << "  -s, --silent" << "Print nothing to console except errors and/or warnings.\n";

    return false;
}
