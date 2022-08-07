/*
    options.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines Option, OptionCollection, OptionDefinition, and OptionDefinitionCollection, 
    which are used when working with command-line options.
*/

#include "options.h"
#include "utils/utils.h"

#include <unordered_set>
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace d2m;

// OptionDefinition

std::string OptionDefinition::GetDisplayName() const
{
    if (!m_Name.empty())
        return "--" + m_Name;
    return "-" + std::string(1, m_ShortName);
}

bool OptionDefinition::IsValid(const value_t& value) const
{
    if (value.index() != GetValueType())
        return false;
    if (!UsesAcceptedValues())
        return true;
    return m_AcceptedValues.count(value) > 0;
}

void OptionDefinition::PrintHelp() const
{
    std::cout.setf(std::ios_base::left);

    std::string str1 = "  ";
    if (HasShortName())
    {
        str1 += "-" + std::string(1, GetShortName());
        if (HasName())
            str1 += ", ";
    }
    if (HasName())
    {
        str1 += "--" + GetName();
    }

    const bool useDoubleQuotes = m_AcceptedValuesContainSpaces;
    const std::string preferredSeparator = GetOptionType() == COMMAND ? " " : "=";

    const OptionDefinition::Type optionType = GetValueType();
    if (UsesAcceptedValues() && optionType != OptionDefinition::BOOL)
    {
        str1 += preferredSeparator + "[";

        unsigned i = 0;
        const size_t total = GetAcceptedValuesOrdered().size();
        for (const auto& val : GetAcceptedValuesOrdered())
        {
            switch (optionType)
            {
                case OptionDefinition::INT:
                    str1 += std::to_string(std::get<OptionDefinition::INT>(val)); break;
                case OptionDefinition::DOUBLE:
                    str1 += std::to_string(std::get<OptionDefinition::DOUBLE>(val)); break;
                case OptionDefinition::STRING:
                    if (useDoubleQuotes)
                        str1 += "\"" + std::get<OptionDefinition::STRING>(val) + "\"";
                    else
                        str1 += std::get<OptionDefinition::STRING>(val);
                    break;
                default:
                    break;
            }

            if (i + 1 != total)
                str1 += ", ";

            i++;
        }

        str1 += "]";
    }
    else if (!m_CustomAcceptedValuesText.empty()) // If it uses custom accepted values text
    {
        str1 += preferredSeparator;
        str1 += m_CustomAcceptedValuesText;
    }
    else
    {
        switch (optionType)
        {
            case OptionDefinition::INT:
            case OptionDefinition::DOUBLE:
                str1 += preferredSeparator;
                str1 += "<value>"; break;
            case OptionDefinition::STRING:
                str1 += preferredSeparator;
                str1 += "\"<value>\""; break;
            default:
                break;
        }
    }

    std::string str2 = GetDescription() + " ";
    switch (optionType)
    {
        case OptionDefinition::BOOL:
        {
            const bool defaultValue = std::get<OptionDefinition::BOOL>(GetDefaultValue());
            if (defaultValue)
            {
                //  Only print the default value if it is true
                str2 += "(Default: true)";
            }
            break;
        }
        case OptionDefinition::INT:
        {
            str2 += "(Default: ";
            str2 += std::to_string(std::get<OptionDefinition::INT>(GetDefaultValue()));
            str2 += ")";
            break;
        }
        case OptionDefinition::DOUBLE:
        {
            str2 += "(Default: ";
            str2 += std::to_string(std::get<OptionDefinition::DOUBLE>(GetDefaultValue()));
            str2 += ")";
            break;
        }
        case OptionDefinition::STRING:
        {
            const std::string defaultValue = std::get<OptionDefinition::STRING>(GetDefaultValue());
            if (!defaultValue.empty())
            {
                str2 += "(Default: ";
                if (useDoubleQuotes)
                    str2 += "\"" + defaultValue + "\"";
                else
                    str2 += defaultValue;
                str2 += ")";
            }
            break;
        }
        default:
            break;
    }

    std::cout << std::setw(30) << str1 << str2 << "\n";
}

// OptionDefinitionCollection

OptionDefinitionCollection::OptionDefinitionCollection(const OptionDefinitionCollection& other)
{
    m_IdOptionsMap = other.m_IdOptionsMap;
    for (auto& mapPair : m_IdOptionsMap)
    {
        OptionDefinition* moduleOption = &mapPair.second;

        const std::string name = moduleOption->GetName();
        m_NameOptionsMap[name] = moduleOption;
        
        const char shortName = moduleOption->GetShortName();
        m_ShortNameOptionsMap[shortName] = moduleOption;
    }
}

OptionDefinitionCollection::OptionDefinitionCollection(std::initializer_list<OptionDefinition> options)
{
    // Initialize collection + mappings
    m_IdOptionsMap.clear();
    m_NameOptionsMap.clear();
    m_ShortNameOptionsMap.clear();
    for (auto& option : options)
    {
        // Id mapping
        const int id = option.GetId();
        assert(m_IdOptionsMap.count(id) == 0 && "OptionDefinitionCollection(...): Duplicate option id found.");
        m_IdOptionsMap[id] = option; // Uses copy constructor

        // Name mapping
        if (option.HasName())
        {
            const std::string name = option.GetName();
            assert(m_NameOptionsMap.count(name) == 0 && "OptionDefinitionCollection(...): Duplicate option name found.");
            m_NameOptionsMap[name] = &m_IdOptionsMap[id];
        }

        // Short name mapping
        if (option.HasShortName())
        {
            const char shortName = option.GetShortName();
            assert(m_ShortNameOptionsMap.count(shortName) == 0 && "OptionDefinitionCollection(...): Duplicate option short name found.");
            m_ShortNameOptionsMap[shortName] = &m_IdOptionsMap[id];
        }
    }
}

size_t OptionDefinitionCollection::Count() const
{
    return m_IdOptionsMap.size();
}

const OptionDefinition* OptionDefinitionCollection::FindById(int id) const
{
    if (m_IdOptionsMap.count(id) == 0)
        return nullptr;
    return &m_IdOptionsMap.at(id);
}

const OptionDefinition* OptionDefinitionCollection::FindByName(const std::string& name) const
{
    if (m_NameOptionsMap.count(name) == 0)
        return nullptr;
    return m_NameOptionsMap.at(name);
}

const OptionDefinition* OptionDefinitionCollection::FindByShortName(char shortName) const
{
    if (m_ShortNameOptionsMap.count(shortName) == 0)
        return nullptr;
    return m_ShortNameOptionsMap.at(shortName);
}

int OptionDefinitionCollection::FindIdByName(const std::string& name) const
{
    const OptionDefinition* ptr = FindByName(name);
    if (!ptr)
        return npos;
    return ptr->GetId();
}

int OptionDefinitionCollection::FindIdByShortName(char shortName) const
{
    const OptionDefinition* ptr = FindByShortName(shortName);
    if (!ptr)
        return npos;
    return ptr->GetId();
}

void OptionDefinitionCollection::PrintHelp() const
{
    for (const auto& mapPair : m_IdOptionsMap)
    {
        const OptionDefinition& definition = mapPair.second;
        definition.PrintHelp();
    }
}

// Option

Option::Option(const std::shared_ptr<const OptionDefinitionCollection>& definitions, int id)
{
    assert(definitions.get() && "Option definition cannot be null.");
    m_Definitions = definitions;
    m_Id = id;
    m_ExplicitlyProvided = false;
    SetValueToDefault();
}

Option::Option(const std::shared_ptr<const OptionDefinitionCollection>& definitions, int id, value_t value)
{
    assert(definitions.get() && "Option definition cannot be null.");
    m_Definitions = definitions;
    m_Id = id;
    m_ExplicitlyProvided = false;
    SetValue(value);
}

void Option::SetValue(value_t& value)
{
    assert(GetDefinition()->IsValid(value) && "The value is not a valid type.");
    m_Value = value;
    if (GetDefinition()->UsesAcceptedValues())
    {
        const auto& acceptedValues = GetDefinition()->GetAcceptedValues();
        const int index = acceptedValues.at(m_Value);
        m_ValueIndex = index;
    }
}

void Option::SetValue(value_t&& value)
{
    assert(GetDefinition()->IsValid(value) && "The value is not a valid type.");
    m_Value = std::move(value);
    if (GetDefinition()->UsesAcceptedValues())
    {
        const auto& acceptedValues = GetDefinition()->GetAcceptedValues();
        const int index = acceptedValues.at(m_Value);
        m_ValueIndex = index;
    }
}

void Option::SetValueToDefault()
{
    const OptionDefinition* definition = GetDefinition();
    m_Value = definition->GetDefaultValue();
    if (GetDefinition()->UsesAcceptedValues())
    {
        const auto& acceptedValues = GetDefinition()->GetAcceptedValues();
        const int index = acceptedValues.at(m_Value);
        m_ValueIndex = index;
    }
}

const OptionDefinition* Option::GetDefinition() const
{
    assert(m_Definitions.get() && "Option definitions were null.");
    const OptionDefinition* definition = m_Definitions->FindById(m_Id);
    assert(definition && "Option definition was not found.");
    return definition;
}

// OptionCollection

OptionCollection::OptionCollection() : m_Definitions(std::make_shared<OptionDefinitionCollection>()), m_OptionsMap({}) {}

OptionCollection::OptionCollection(const std::shared_ptr<const OptionDefinitionCollection>& definitions)
{
    SetDefinitions(definitions);
}

void OptionCollection::SetDefinitions(const std::shared_ptr<const OptionDefinitionCollection>& definitions)
{
    m_Definitions = definitions;

    // Create options and set them to their default value
    m_OptionsMap.clear();
    if (definitions) // If no option definitions were given, this will be null
    {
        for (const auto& mapPair : definitions->GetIdMap())
        {
            const int id = mapPair.first;
            m_OptionsMap.try_emplace(id, definitions, id);
        }
    }
    else
    {
        // m_Definitions must always point to an OptionDefinitionCollection, even if it is empty.
        m_Definitions = std::make_shared<const OptionDefinitionCollection>();
    }
}

const Option& OptionCollection::GetOption(std::string name) const
{
    const int id = m_Definitions->FindIdByName(name);
    assert(id != OptionDefinitionCollection::npos && "Option with the given name wasn't found in the collection.");
    return GetOption(id);
}

Option& OptionCollection::GetOption(std::string name)
{
    const int id = m_Definitions->FindIdByName(name);
    assert(id != OptionDefinitionCollection::npos && "Option with the given name wasn't found in the collection.");
    return GetOption(id);
}

const Option& OptionCollection::GetOption(char shortName) const
{
    const int id = m_Definitions->FindIdByShortName(shortName);
    assert(id != OptionDefinitionCollection::npos && "Option with the given short name wasn't found in the collection.");
    return GetOption(id);
}

Option& OptionCollection::GetOption(char shortName)
{
    const int id = m_Definitions->FindIdByShortName(shortName);
    assert(id != OptionDefinitionCollection::npos && "Option with the given short name wasn't found in the collection.");
    return GetOption(id);
}

void OptionCollection::SetValuesToDefault()
{
    for (auto& mapPair : m_OptionsMap)
    {
        auto& option = mapPair.second;
        option.SetValueToDefault();
    }
}

bool OptionCollection::ParseArgs(std::vector<std::string>& args, bool ignoreUnknownArgs)
{
    /* 
     * Examples of command-line arguments that can be parsed:
     * --foo
     * --foo="bar 123"
     * --foo bar
     * --foo -123.0
     * -f
     * -f=bar
     * -f=true
     * -f 3
     * 
     * Command-line options passed in with with double-quotes around 
     *  them do not have double-quotes here.
     * Arguments are checked against option definitions for a match
     *  and to determine the type (bool, int, double, or string).
     * Valid arguments that do not match any option definition are
     * ignored and left in the args vector, while matching arguments
     * are "consumed" and removed from it.
     */

    std::unordered_set<int> optionsParsed;

    // Sets the value of an option given a value string
    auto SetValue = [this, &optionsParsed](const char* valueStr, const OptionDefinition* optionDef) -> bool
    {
        auto& option = m_OptionsMap[optionDef->GetId()];

        OptionDefinition::value_t valueTemp;
        if (ModuleOptionUtils::ConvertToValue(valueStr, optionDef->GetValueType(), valueTemp))
        {
            return true; // Error occurred
        }

        if (!optionDef->IsValid(valueTemp))
        {
            const std::string optionTypeStr = optionDef->GetOptionType() == OPTION ? "option" : "command";
            std::cerr << "ERROR: The value \"" << valueStr << "\" is not valid for the " << optionTypeStr << " \"" << optionDef->GetDisplayName() << "\".\n";
            return true; // The value is not valid for this option definition
        }

        option.SetValue(std::move(valueTemp));
        option.m_ExplicitlyProvided = true;
        optionsParsed.insert(optionDef->GetId());
        return false;
    };

    const OptionDefinition* handlingOption = nullptr;

    /*
     * If the previous argument is syntactically correct yet unrecognized (maybe it's a module option and
     *  we are reading global options now), the current argument in the following loop may be its value if
     *  the arguments were passed like: "--unrecognized value". Or it may be another option - recognized or not.
    */
    bool argMightBeValue = false;

    // Main loop
    for (int i = 0; i < static_cast<int>(args.size()); i++)
    {
        auto& arg = args[i];
        ModuleUtils::StringTrimBothEnds(arg);
        if (arg.empty())
        {
            args.erase(args.begin() + i);
            i--; // Adjust for item just erased
            continue;
        }

        const OptionDefinition* def = handlingOption;
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
            if (argMightBeValue)
            {
                // Hopefully this is just a value from the preceding unrecognized argument
                argMightBeValue = false;
                continue;
            }

            // Error: Invalid argument
            std::cerr << "ERROR: Invalid option: \"" << arg << "\"\n";
            return true;
        }

        const bool usingShortName = arg[1] != '-';
        if (usingShortName) // -f format argument (short name)
        {
            if (!isalpha(arg[1]))
            {
                if (argMightBeValue)
                {
                    // Hopefully this is just a value from the preceding unrecognized argument
                    argMightBeValue = false;
                    continue;
                }

                // Error: Short names must be alphabetic
                std::cerr << "ERROR: Invalid flag \"" << arg << "\": Flags must be comprised of only alphabetic characters.\n";
                return true;
            }

            equalsPos = arg.find_first_of('=');
            const bool usingEquals = equalsPos != std::string::npos;
            if (usingEquals) // Using the form: "-f=<value>"
            {
                if (equalsPos != 2)
                {
                    if (argMightBeValue)
                    {
                        // Hopefully this is just a value from the preceding unrecognized argument
                        argMightBeValue = false;
                        continue;
                    }

                    // Error: Short flags with an '=' must be of the form: "-f=<value>"
                    std::cerr << "ERROR: Invalid flag \"" << arg << "\": Unable to parse.\n";
                    return true;
                }

                // At this point, argument is deemed syntactically valid
                def = m_Definitions->FindByShortName(arg[1]);
            }
            else // Using the form: "-f", "-f <value>", or "-abcdef"
            {
                const bool usingSeveralShortArgs = arg.size() > 2;
                if (!usingSeveralShortArgs) // Using the form: "-f" or "-f <value>"
                {
                    if (!isalpha(arg[1]))
                    {
                        if (argMightBeValue)
                        {
                            // Hopefully this is just a value from the preceding unrecognized argument
                            argMightBeValue = false;
                            continue;
                        }

                        // Error: Short flags with an '=' must be of the form: "-f=<value>"
                        std::cerr << "ERROR: Invalid flag \"" << arg << "\": Unable to parse.\n";
                        return true;
                    }

                    // At this point, argument is deemed syntactically valid
                    def = m_Definitions->FindByShortName(arg[1]);
                }
                else // Using the form: "-abcdef" - this form cannot have a value after it
                {
                    for (unsigned j = 1; j < arg.size(); j++)
                    {
                        const char c = arg[j];
                        if (!isalpha(c))
                        {
                            if (argMightBeValue)
                            {
                                // Hopefully this is just a value from the preceding unrecognized argument
                                argMightBeValue = false;
                                break; // Break out of inner loop, then hit the continue
                            }

                            // Error: Short names must be alphabetic
                            std::cerr << "ERROR: Invalid flag '" << arg[j] << "': Flags must be comprised of only alphabetic characters.\n";
                            return true;
                        }

                        const OptionDefinition* tempDef = m_Definitions->FindByShortName(c);
                        
                        // Skip unrecognized options
                        if (!tempDef)
                            continue;
                        
                        if (tempDef->GetValueType() != OptionDefinition::BOOL)
                        {
                            // Error: When multiple short flags are strung together, all of them must be boolean-typed options
                            return true;
                        }

                        // Set the value
                        SetValue("1", tempDef);

                        arg.erase(j--, 1); // Remove this flag from argument, since it has been consumed
                    }

                    argMightBeValue = false; // Impossible for next arg to be a value

                    if (arg == "-")
                    {
                        // Erase argument since it has been consumed
                        args.erase(args.begin() + i);
                        i--; // Adjust for item just erased
                    }

                    // SetValue() was called here and does not need to be called below.
                    // Or this was a value from an unrecognized argument, which we are skipping.
                    continue;
                }
            }
        }
        else // --foo format argument
        {
            equalsPos = arg.find_first_of('=');
            std::string name = arg.substr(2, equalsPos - 2); // From start to '=' or end of string - whichever comes first
            def = m_Definitions->FindByName(name);
        }

        if (!def)
        {
            // Syntactically valid argument, yet unknown
            argMightBeValue = true; // Next arg may be this unrecognized option's value
            continue; // Skip
        }

        // At this point, the current argument is both syntactically valid and a recognized option.

        /*
         * This is a recognized option, not the preceding unrecognised option's value.
         * Therefore, the next argument cannot be an unrecognized argument's value:
         */
        argMightBeValue = false;

        if (optionsParsed.count(def->GetId()) > 0)
        {
            // ERROR: Setting the same option twice
            const std::string optionTypeStr = def->GetOptionType() == OPTION ? "option" : "command";
            if (usingShortName)
            {
                std::cerr << "ERROR: The " << optionTypeStr << " \"-" << def->GetShortName() << "\" is used more than once.\n";
            }
            else
            {
                std::cerr << "ERROR: The " << optionTypeStr << " \"--" << def->GetName() << "\" is used more than once.\n";
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
            if (def->GetValueType() != OptionDefinition::BOOL)
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
        const std::string optionTypeStr = handlingOption->GetOptionType() == OPTION ? "option" : "command";
        std::cerr << "ERROR: The " << optionTypeStr << " \"" << handlingOption->GetDisplayName() << "\" did not provide a value.\n";
        return true;
    }

    if (!ignoreUnknownArgs && !args.empty())
    {
        std::string errorStr = "ERROR: Unknown option(s): ";
        for (unsigned i = 0; i < args.size(); i++)
        {
            errorStr += args[i];
            if (i != args.size() - 1)
                errorStr += ", ";
        }
        std::cerr << errorStr << "\n";
        return true;
    }

    return false;
}

// ModuleOptionUtils

std::string ModuleOptionUtils::ConvertToString(const value_t& value)
{
    const OptionDefinition::Type type = static_cast<OptionDefinition::Type>(value.index());
    switch (type)
    {
        case OptionDefinition::BOOL:
            return std::get<bool>(value) ? "true" : "false";
        case OptionDefinition::INT:
            return std::to_string(std::get<int>(value));
        case OptionDefinition::DOUBLE:
            return std::to_string(std::get<double>(value));
        case OptionDefinition::STRING:
            return std::get<std::string>(value);
        default:
            return "ERROR";
    }
}

bool ModuleOptionUtils::ConvertToValue(const std::string& valueStr, OptionDefinition::Type type, value_t& returnVal)
{
    switch (type)
    {
        case OptionDefinition::BOOL:
        {
            std::string valueStrLower = valueStr;
            unsigned i = 0;
            while (i < valueStrLower.size())
            {
                valueStrLower[i] = tolower(valueStrLower[i]);
                ++i;
            }
            
            if (valueStrLower == "0")
                returnVal = false;
            else if (valueStrLower == "false")
                returnVal = false;
            else if (valueStrLower == "1")
                returnVal = true;
            else if (valueStrLower == "true")
                returnVal = true;
            else
            {
                // Error: Invalid value for boolean-typed option
                std::cerr << "ERROR: Invalid value \"" << valueStr << "\" for boolean-typed option.\n";
                return true;
            }
        } break;
        case OptionDefinition::INT:
        {
            returnVal = atoi(valueStr.c_str());
        } break;
        case OptionDefinition::DOUBLE:
        {
            returnVal = atof(valueStr.c_str());
        } break;
        case OptionDefinition::STRING:
        {
            returnVal = valueStr;
        } break;
    }

    return false;
}

bool ModuleOptionUtils::ConvertToValue(const char* valueStr, OptionDefinition::Type type, value_t& returnVal)
{
    std::string valueStrConst(valueStr);
    return ConvertToValue(valueStrConst, type, returnVal);
}
