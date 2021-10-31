#include "core.h"
#include "modules.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <map>
#include <functional>
//#include <filesystem>
#include <fstream>

// Initialize module registration maps
std::map<ModuleType, std::function<ModuleBase*(void)>> ModuleUtils::RegistrationMap = {};
std::map<std::string, ModuleType> ModuleUtils::FileExtensionMap = {};
std::map<ModuleType, std::function<ConversionOptionsBase*(void)>> ModuleUtils::ConversionOptionsRegistrationMap = {};

struct CommonFlags
{
    bool force = false;
    // More to be added later
};

static bool ParseFlags(std::vector<std::string>& args, CommonFlags& flags);

bool ModuleUtils::ParseArgs(int argc, char *argv[], InputOutput& inputOutputInfo, ConversionOptions& options)
{
    inputOutputInfo.InputFile = "";
    inputOutputInfo.InputType = ModuleType::NONE;
    inputOutputInfo.OutputFile = "";
    inputOutputInfo.OutputType = ModuleType::NONE;
    //options = nullptr;

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
            std::cout << "ERROR: Could not parse arguments." << std::endl;
            return true;
        }
    }
    else if (argc >= 3) // 3 is the minimum needed to perform a conversion
    {
        if (args[1] == "--help")
        {
            return PrintHelp(args[0], GetTypeFromFileExtension(args[2]));
        }

        CommonFlags flags;
        if (ParseFlags(args, flags))
            return true;

        std::string outputFile, inputFile;
        
        // Get input file
        if (FileExists(args[2]))
        {
            if (GetTypeFromFilename(args[2]) != ModuleType::NONE)
            {
                inputFile = args[2];
            }
            else
            {
                std::cout << "ERROR: Input file type '" << GetFileExtension(args[2]) << "' is unsupported." << std::endl;
                return true;
            }
        }
        else
        {
            std::cout << "ERROR: The input file '" << args[2] << "' could not be found." << std::endl;
            return true;
        }
        
        // Get output file
        if (GetFileExtension(args[1]).empty())
        {
            if (GetTypeFromFileExtension(args[1]) != ModuleType::NONE)
            {
                const size_t dotPos = inputFile.rfind('.');
                if (dotPos == 0 || dotPos + 1 >= inputFile.size())
                {
                    std::cout << "ERROR: The input file is invalid." << std::endl;
                    return true;
                }

                // Construct output filename from the input filename
                outputFile = inputFile.substr(0, dotPos + 1) + args[1];
            }
            else
            {
                std::cout << "ERROR: '" << args[1] << "' is not a valid module type." << std::endl;
                return true;
            }
        }
        else
        {
            outputFile = args[1];
            if (GetTypeFromFilename(args[1]) == ModuleType::NONE)
            {
                std::cout << "ERROR: '" << GetFileExtension(args[1]) << "' is not a valid module type." << std::endl;
                return true;
            }
        }
        
        if (FileExists(outputFile) && !flags.force)
        {
            std::cout << "ERROR: The output file '" << outputFile << "' already exists. Run with the '-f' flag to allow the file to be overwritten." << std::endl;
            return true;
        }

        inputOutputInfo.InputFile = inputFile;
        inputOutputInfo.InputType = GetTypeFromFilename(inputFile);
        inputOutputInfo.OutputFile = outputFile;
        inputOutputInfo.OutputType = GetTypeFromFilename(outputFile);

        if (inputOutputInfo.InputType == inputOutputInfo.OutputType)
        {
            std::cout << "The output file is the same type as the input file. No conversion necessary." << std::endl;
            return true;
        }

        // TODO: Check if a conversion between the two types is possible

        // At this point, the input and output file arguments have been deemed valid

        // Remove executable, output file, and input file from the args list, since they've already been processed
        // What is left are module-specific command-line arguments
        args.erase(args.begin(), args.begin() + 3);
        argc -= 3;

        ConversionOptions optionsTemp = ConversionOptions::Create(inputOutputInfo.OutputType);
        if (!optionsTemp)
        {
            std::cout << "ERROR: Failed to create ConversionOptions-derived object for the module type '" << GetFileExtension(outputFile) 
                << "'. The module may not be properly registered with dmf2mod." << std::endl;
            return true;
        }

        if (!args.empty() && optionsTemp.ParseArgs(args))
        {
            // An error occurred while parsing the module-specific arguments
            return true;
        }
        
        options.MoveFrom(optionsTemp);
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
                    str.erase(j--);
                    break;
                default:
                    // If a flag is not recognized, it may be a flag used by a module, so don't throw an error
                    //std::cout << "ERROR: The flag '-" << c << "' is unsupported." << std::endl;
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
    }

    return false;
}

ModuleType ModuleUtils::GetTypeFromFilename(const std::string& filename)
{
    std::string ext = GetFileExtension(filename);
    if (ext.empty())
        return ModuleType::NONE;
    
    const auto iter = ModuleUtils::FileExtensionMap.find(ext);
    if (iter != ModuleUtils::FileExtensionMap.end())
        return iter->second;

    return ModuleType::NONE;
}

ModuleType ModuleUtils::GetTypeFromFileExtension(const std::string& extension)
{
    if (extension.empty())
        return ModuleType::NONE;
    
    const auto iter = ModuleUtils::FileExtensionMap.find(extension);
    if (iter != ModuleUtils::FileExtensionMap.end())
        return iter->second;

    return ModuleType::NONE;
}

std::string ModuleUtils::GetExtensionFromType(ModuleType moduleType)
{
    for (const auto& mapPair : FileExtensionMap)
    {
        if (mapPair.second == moduleType)
            return mapPair.first;
    }
    return "";
}

bool ModuleUtils::PrintHelp(const std::string& executable, ModuleType moduleType)
{
    // If module-specific help was requested
    if (moduleType != ModuleType::NONE)
    {
        ConversionOptions options = ConversionOptions::Create(moduleType);
        if (!options)
        {
            std::string extension = GetExtensionFromType(moduleType);
            if (extension.empty())
            {
                std::cout << "ERROR: The module is not propery registered with dmf2mod." << std::endl;
            }
            else
            {
                std::cout << "ERROR: Failed to create ConversionOptions-derived object for the module type '" << extension 
                    << "'. The module may not be properly registered with dmf2mod." << std::endl;
            }
            return true;
        }

        options.PrintHelp();
        return false;
    }

    // Print generic help

    std::cout << "dmf2mod v" << DMF2MOD_VERSION << std::endl;
    std::cout << "Created by Dalton Messmer <messmer.dalton@gmail.com>" << std::endl;

    /*
    std::string ext = std::string(".") + GetFileExtension(executable);
    if (ext != ".exe")
        ext = "";
    */

    std::cout.setf(std::ios_base::left);
    std::cout << std::setw(25) << "Usage:" << "dmf2mod" << " output.[ext] input.dmf [options]" << std::endl;
    std::cout << std::setw(25) << "" << "dmf2mod" << " [ext] input.dmf [options]" << std::endl;

    std::cout << "Options:" << std::endl;

    std::cout.setf(std::ios_base::left);
    std::cout << std::setw(25) << "-f, --force" << "Overwrite output file." << std::endl;
    std::cout << std::setw(25) << "--help [module type]" << "Display this help message. Provide module type (i.e. mod) for module-specific options." << std::endl;

    return false;
}

ModuleType Module::GetType() const
{
    if (!m_Module)
        return ModuleType::NONE;
    return m_Module->GetType();
}

ModuleType ConversionOptions::GetType() const
{
    if (!m_Options)
        return ModuleType::NONE;
    return m_Options->GetType();
}

bool FileExists(const std::string& filename)
{
    std::ifstream file(filename);
    return file.is_open();
}

std::string GetFileExtension(const std::string& filename)
{
    const size_t dotPos = filename.rfind('.');
    // If dot is at start, not found, or at end:
    if (dotPos == 0 || dotPos == std::string::npos || dotPos + 1 >= filename.size())
        return "";

    return filename.substr(dotPos + 1);
}
