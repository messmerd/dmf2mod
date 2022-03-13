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
#include <unordered_set>
#include <functional>
#include <fstream>
#include <variant>
//#include <filesystem>

using namespace d2m;

// File utils

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

// Command-line arguments and options utils

std::vector<std::string> ModuleUtils::GetArgsAsVector(int argc, char *argv[])
{
    std::vector<std::string> args(argc, "");
    for (int i = 0; i < argc; i++)
    {
        args[i] = argv[i];
    }
    return args;
}

bool ModuleUtils::ParseArgs(std::vector<std::string>& args, const ModuleOptions& optionDefinitions, OptionValues& values)
{
    // --foo
    // --foo=bar
    // --foo="bar"
    // --foo="abc def ghi"
    // --foo=-123
    // --foo bar
    // --foo "bar"
    // --foo

    std::unordered_set<int> optionsParsed;

    // Sets the value of an option given a value string
    auto SetValue = [&values, &optionsParsed](const char* valueStr, const ModuleOption* optionDef) -> bool
    {
        auto& value = values[optionDef->GetId()];
        
        ModuleOption::value_t valueTemp;
        if (ModuleOptionUtils::ConvertToValue(valueStr, optionDef->GetType(), valueTemp))
        {
            return true; // Error occurred
        }
        
        if (!optionDef->IsValid(valueTemp))
        {
            std::cerr << "ERROR: The value \"" << valueStr << "\" is not valid for the option \"" << optionDef->GetName() << "\".\n";
            return true; // The value is not valid for this option definition
        }

        value = std::move(valueTemp);
        optionsParsed.insert(optionDef->GetId());
        return false;
    };

    // Main loop
    const ModuleOption* handlingOption = nullptr;
    for (unsigned i = 0; i < args.size(); i++)
    {
        auto& arg = args[i];
        StringTrimBothEnds(arg);
        if (arg.empty())
        {
            args.erase(args.begin() + i);
            i--; // Adjust for item just erased
            continue;
        }

        const ModuleOption* def = handlingOption;
        size_t equalsPos = std::string::npos;
        
        const bool thisArgIsValue = handlingOption != nullptr;
        if (thisArgIsValue)
        {
            handlingOption = nullptr;

            // Set the value
            if (SetValue(arg.c_str(), def))
                return true; // Error occurred

            // Erase both the flag and value since they have been consumed
            args.erase(args.begin() + i - 1, args.begin() + i + 1);
            i -= 2; // Adjust for items just erased
            continue;
        }

        if (arg.size() <= 1 || arg[0] != '-')
        {
            // Error: Invalid argument
            std::cerr << "ERROR: Invalid flag: \"" << arg << "\"\n";
            return true;
        }

        const bool usingShortName = arg[1] != '-';
        if (usingShortName) // -f format argument (short name)
        {
            if (!isalpha(arg[1]))
            {
                // Error: Short names must be alphabetic
                std::cerr << "ERROR: Invalid flag '" << arg.substr(1) << "': Flags must be comprised of only alphabetic characters.\n";
                return true;
            }

            equalsPos = arg.find_first_of('=');
            const bool usingEquals = equalsPos != std::string::npos;
            if (usingEquals) // Using the form: "-f=<value>"
            {
                if (equalsPos != 2)
                {
                    // Error: Short flags with an '=' must be of the form: "-f=<value>"
                    std::cerr << "ERROR: Invalid flag \"" << arg.substr(1) << "\": Unable to parse.\n";
                    return true;
                }
                def = optionDefinitions.FindByShortName(arg[1]);
            }
            else // Using the form: "-f", "-f <value>", or "-abcdef"
            {
                const bool usingSeveralShortArgs = arg.size() > 2;
                if (!usingSeveralShortArgs) // Using the form: "-f" or "-f <value>"
                {
                    def = optionDefinitions.FindByShortName(arg[1]);
                    // The argument may be of the form "-f <value>" if it is not a bool type. That will be handled later.
                }
                else // Using the form: "-abcdef"
                {
                    for (unsigned j = 1; j < arg.size(); j++)
                    {
                        const char c = arg[j];
                        if (!isalpha(c))
                        {
                            // Error: Short names must be alphabetic
                            std::cerr << "ERROR: Invalid flag '" << arg[j] << "': Flags must be comprised of only alphabetic characters.\n";
                            return true;
                        }

                        const ModuleOption* tempDef = optionDefinitions.FindByShortName(c);
                        
                        // Skip unrecognized options
                        if (!tempDef)
                            continue;
                        
                        if (tempDef->GetType() != ModuleOption::BOOL)
                        {
                            // Error: When multiple short flags are strung together, all of them must be bool-typed options
                            return true;
                        }

                        // Set the value
                        SetValue("1", tempDef);

                        arg.erase(j--, 1); // Remove this flag from argument, since it has been consumed
                    }

                    if (arg.empty())
                    {
                        // Erase argument since it has been consumed
                        args.erase(args.begin() + i);
                        i--; // Adjust for item just erased
                    }

                    continue; // SetValue() was called here and does not need to be called below.
                }
            }
        }
        else // --foo format argument
        {
            equalsPos = arg.find_first_of('=');
            std::string name = arg.substr(2, equalsPos); // From start to '=' or end of string - whichever comes first
            def = optionDefinitions.FindByName(name);
        }
        

        if (!def)
        {
            // Unknown argument; skip it
            continue;
        }

        if (optionsParsed.count(def->GetId()) > 0)
        {
            // ERROR: Setting the same option twice
            if (usingShortName)
            {
                std::cerr << "ERROR: The flag '" << arg[1] << "' is used more than once.\n";
            }
            else
            {
                std::cerr << "ERROR: The flag \"" << arg.substr(2) << "\" is used more than once.\n";
            }
            
            return true;
        }

        std::string valueStr;
        if (equalsPos != std::string::npos) // Value is after the '='
        {
            valueStr = arg.substr(equalsPos + 1);
            
            // Set the value
            if (SetValue(valueStr.c_str(), def))
                return true; // Error occurred
            
            // Erase argument since it has been consumed
            args.erase(args.begin() + i);
            i--; // Adjust for item just erased
        }
        else // No '=' present
        {
            if (def->GetType() != ModuleOption::BOOL)
            {
                // The next argument will be the value
                handlingOption = def;
            }
            else
            {
                // The presence of a bool argument means its value is true. Unless '=' is present, no value is expected to follow.
                //  So unlike other types, "--foobar false" would not be valid, but "--foobar=false" is.
                
                // Set the value
                SetValue("1", def);

                // Erase argument since it has been consumed
                args.erase(args.begin() + i);
                i--; // Adjust for item just erased
            }
        }
    }

    if (handlingOption)
    {
        std::cerr << "ERROR: The flag \"" << args.back() << "\" did not provide a value.\n";
        return true;
    }

    return false;
}

void ModuleUtils::PrintHelp(ModuleType moduleType)
{
    if (moduleType == ModuleType::NONE)
        return;
    
    const ModuleOptions& options = Registrar::GetAvailableOptions(moduleType);

    std::string name = Registrar::GetExtensionFromType(moduleType);
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

    PrintHelp(options);
}

void ModuleUtils::PrintHelp(const ModuleOptions& options)
{
    std::cout.setf(std::ios_base::left);

    for (const auto& mapPair : options)
    {
        const auto& option = mapPair.second;

        std::string str1 = "  ";
        str1 += option.HasShortName() ? "-" + std::string(1, option.GetShortName()) + ", " : "";
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

// String utils (borrowed from Stack Overflow)

void ModuleUtils::StringTrimLeft(std::string &str)
{
    // Trim string from start (in place)
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

void ModuleUtils::StringTrimRight(std::string &str)
{
    // Trim string from end (in place)
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

void ModuleUtils::StringTrimBothEnds(std::string &str)
{
    // Trim string from both ends (in place)
    StringTrimLeft(str);
    StringTrimRight(str);
}
