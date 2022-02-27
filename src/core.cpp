/*
    core.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines various classes used by dmf2mod.

    Everything in core.h/core.cpp is written to be 
    module-independent.
*/

#include "core.h"
#include "modules.h"
#include "dmf2mod_config.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <map>
#include <functional>
#include <fstream>
//#include <filesystem>

// Initialize module registration maps
std::map<ModuleType, std::function<ModuleBase*(void)>> ModuleUtils::RegistrationMap = {};
std::map<std::string, ModuleType> ModuleUtils::FileExtensionMap = {};
std::map<ModuleType, std::function<ConversionOptionsBase*(void)>> ModuleUtils::ConversionOptionsRegistrationMap = {};
std::map<ModuleType, std::vector<std::string>> ModuleUtils::AvailableOptionsMap = {};

CommonFlags ModuleUtils::m_CoreOptions = {};

static bool ParseFlags(std::vector<std::string>& args, CommonFlags& flags);

// ModuleUtils class

std::vector<std::string> ModuleUtils::GetAvailableModules()
{
    std::vector<std::string> vec;
    for (const auto& mapPair : FileExtensionMap)
    {
        vec.push_back(mapPair.first);
    }
    return vec;
}

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
            return PrintHelp(args[0], GetTypeFromFileExtension(args[2]));
        }

        CommonFlags flags;
        if (ParseFlags(args, flags))
            return true;

        SetCoreOptions(flags);

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
            if (GetTypeFromFileExtension(args[1]) != ModuleType::NONE)
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
            if (GetTypeFromFilename(args[1]) == ModuleType::NONE)
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
        inputOutputInfo.InputType = GetTypeFromFilename(inputFile);
        inputOutputInfo.OutputFile = outputFile;
        inputOutputInfo.OutputType = GetTypeFromFilename(outputFile);

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

std::string ModuleUtils::GetExtensionFromType(ModuleType moduleType)
{
    for (const auto& mapPair : FileExtensionMap)
    {
        if (mapPair.second == moduleType)
            return mapPair.first;
    }
    return "";
}

std::vector<std::string> ModuleUtils::GetAvailableOptions(ModuleType moduleType)
{
    if (AvailableOptionsMap.count(moduleType) > 0)
        return AvailableOptionsMap[moduleType];
    return {};
}

bool ModuleUtils::PrintHelp(const std::string& executable, ModuleType moduleType)
{
    // If module-specific help was requested
    if (moduleType != ModuleType::NONE)
    {
        ConversionOptionsPtr options = ConversionOptions::Create(moduleType);
        if (!options)
        {
            std::string extension = GetExtensionFromType(moduleType);
            if (extension.empty())
            {
                std::cerr << "ERROR: The module is not propery registered with dmf2mod.\n";
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

// Status class

void Status::PrintWarnings(bool useStdErr)
{
    if (!m_WarningMessages.empty())
    {
        for (const auto& message : m_WarningMessages)
        {
            if (useStdErr)
                std::cerr << message << "\n";
            else
                std::cout << message << "\n";
        }
        if (useStdErr)
            std::cerr << "\n";
        else
            std::cout << "\n";
    }
}

std::string ModuleException::CommonErrorMessageCreator(Category category, int errorCode, const std::string& arg)
{
    switch (category)
    {
        case Category::Import:
            switch (errorCode)
            {
                case (int)ImportError::Success:
                    return "No error.";
                default:
                    return "";
            }
            break;
        case Category::Export:
            switch (errorCode)
            {
                case (int)ExportError::Success:
                    return "No error.";
                case (int)ExportError::FileOpen:
                    return "Failed to open file for writing.";
                default:
                    return "";
            }
            break;
        case Category::Convert:
            switch (errorCode)
            {
                case (int)ConvertError::Success:
                    return "No error.";
                case (int)ConvertError::InvalidArgument:
                    return "Invalid argument.";
                case (int)ConvertError::UnsupportedInputType:
                    return "Input type '" + arg + "' is unsupported for this module.";
                default:
                    return "";
            }
            break;
    }
    return "";
}

void ModuleException::Print() const
{
    std::cerr << m_ErrorMessage << "\n\n";
}

// ModuleStatic class

template <typename T>
Module* ModuleStatic<T>::CreateStatic()
{
    return new T;
}

template <typename T>
ModuleType ModuleStatic<T>::GetTypeStatic()
{
    return m_Type;
}

template <typename T>
std::string ModuleStatic<T>::GetFileExtensionStatic()
{
    return m_FileExtension;
}

// ConversionOptionsStatic class

template <typename T>
ConversionOptionsBase* ConversionOptionsStatic<T>::CreateStatic()
{
    return new T;
}

template <typename T>
ModuleType ConversionOptionsStatic<T>::GetTypeStatic()
{
    return m_Type;
}

// Base classes

template <class T, class>
ModulePtr ModuleBase::Create()
{
    return ModulePtr(new T);
}

template <class moduleClass, class>
ConversionOptionsPtr ConversionOptionsBase::Create()
{
    return ConversionOptionsPtr(ModuleStatic<moduleClass>::m_CreateConversionOptionsStatic());
}

// Interface classes

template <typename T, typename O>
ModuleType ModuleInterface<T, O>::GetType() const
{
    return ModuleStatic<T>::GetTypeStatic();
}

template <typename T, typename O>
std::string ModuleInterface<T, O>::GetFileExtension() const
{
    return ModuleStatic<T>::GetFileExtensionStatic();
}

template <typename T, typename O>
std::vector<std::string> ModuleInterface<T, O>::GetAvailableOptions() const
{
    return ConversionOptionsStatic<O>::GetAvailableOptionsStatic();
}

template <typename T>
ModuleType ConversionOptionsInterface<T>::GetType() const
{
    return ConversionOptionsStatic<T>::GetTypeStatic();
}

template <typename T>
std::vector<std::string> ConversionOptionsInterface<T>::GetAvailableOptions() const
{
    return ConversionOptionsStatic<T>::GetAvailableOptionsStatic();
}