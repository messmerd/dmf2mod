/*
 * options.cpp
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines Option, OptionCollection, OptionDefinition, and OptionDefinitionCollection,
 * which are used when working with command-line options.
 */

#include "core/options.h"
#include "utils/utils.h"

#include <unordered_set>
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace d2m;

// OptionDefinition

std::string OptionDefinition::GetDisplayName() const
{
    if (!name_.empty())
        return "--" + name_;
    return "-" + std::string(1, short_name_);
}

bool OptionDefinition::IsValid(const ValueType& value) const
{
    if (value.index() != GetValueType())
        return false;
    if (!UsesAcceptedValues())
        return true;
    return accepted_values_.count(value) > 0;
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

    const bool use_double_quotes = accepted_values_contain_spaces_;
    const std::string preferred_separator = GetOptionType() == kCommand ? " " : "=";

    const OptionDefinition::Type option_type = GetValueType();
    if (UsesAcceptedValues() && option_type != OptionDefinition::kBool)
    {
        str1 += preferred_separator + "[";

        unsigned i = 0;
        const size_t total = GetAcceptedValuesOrdered().size();
        for (const auto& val : GetAcceptedValuesOrdered())
        {
            switch (option_type)
            {
                case OptionDefinition::kInt:
                    str1 += std::to_string(std::get<OptionDefinition::kInt>(val)); break;
                case OptionDefinition::kDouble:
                    str1 += std::to_string(std::get<OptionDefinition::kDouble>(val)); break;
                case OptionDefinition::kString:
                    if (use_double_quotes)
                        str1 += "\"" + std::get<OptionDefinition::kString>(val) + "\"";
                    else
                        str1 += std::get<OptionDefinition::kString>(val);
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
    else if (!custom_accepted_values_text_.empty()) // If it uses custom accepted values text
    {
        str1 += preferred_separator;
        str1 += custom_accepted_values_text_;
    }
    else
    {
        switch (option_type)
        {
            case OptionDefinition::kInt:
            case OptionDefinition::kDouble:
                str1 += preferred_separator;
                str1 += "<value>"; break;
            case OptionDefinition::kString:
                str1 += preferred_separator;
                str1 += "\"<value>\""; break;
            default:
                break;
        }
    }

    std::string str2 = GetDescription() + " ";
    switch (option_type)
    {
        case OptionDefinition::kBool:
        {
            const bool default_value = std::get<OptionDefinition::kBool>(GetDefaultValue());
            if (default_value)
            {
                //  Only print the default value if it is true
                str2 += "(Default: true)";
            }
            break;
        }
        case OptionDefinition::kInt:
        {
            str2 += "(Default: ";
            str2 += std::to_string(std::get<OptionDefinition::kInt>(GetDefaultValue()));
            str2 += ")";
            break;
        }
        case OptionDefinition::kDouble:
        {
            str2 += "(Default: ";
            str2 += std::to_string(std::get<OptionDefinition::kDouble>(GetDefaultValue()));
            str2 += ")";
            break;
        }
        case OptionDefinition::kString:
        {
            const std::string default_value = std::get<OptionDefinition::kString>(GetDefaultValue());
            if (!default_value.empty())
            {
                str2 += "(Default: ";
                if (use_double_quotes)
                    str2 += "\"" + default_value + "\"";
                else
                    str2 += default_value;
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
    id_options_map_ = other.id_options_map_;
    for (auto& map_pair : id_options_map_)
    {
        OptionDefinition* module_option = &map_pair.second;

        const std::string name = module_option->GetName();
        name_options_map_[name] = module_option;

        const char short_name = module_option->GetShortName();
        short_name_options_map_[short_name] = module_option;
    }
}

OptionDefinitionCollection::OptionDefinitionCollection(const std::initializer_list<OptionDefinition>& options)
{
    // Initialize collection + mappings
    id_options_map_.clear();
    name_options_map_.clear();
    short_name_options_map_.clear();
    for (auto& option : options)
    {
        // Id mapping
        const int id = option.GetId();
        assert(id_options_map_.count(id) == 0 && "OptionDefinitionCollection(...): Duplicate option id found.");
        id_options_map_[id] = option; // Uses copy constructor

        // Name mapping
        if (option.HasName())
        {
            const std::string name = option.GetName();
            assert(name_options_map_.count(name) == 0 && "OptionDefinitionCollection(...): Duplicate option name found.");
            name_options_map_[name] = &id_options_map_[id];
        }

        // Short name mapping
        if (option.HasShortName())
        {
            const char short_name = option.GetShortName();
            assert(short_name_options_map_.count(short_name) == 0 && "OptionDefinitionCollection(...): Duplicate option short name found.");
            short_name_options_map_[short_name] = &id_options_map_[id];
        }
    }
}

size_t OptionDefinitionCollection::Count() const
{
    return id_options_map_.size();
}

const OptionDefinition* OptionDefinitionCollection::FindById(int id) const
{
    if (id_options_map_.count(id) == 0)
        return nullptr;
    return &id_options_map_.at(id);
}

const OptionDefinition* OptionDefinitionCollection::FindByName(const std::string& name) const
{
    if (name_options_map_.count(name) == 0)
        return nullptr;
    return name_options_map_.at(name);
}

const OptionDefinition* OptionDefinitionCollection::FindByShortName(char short_name) const
{
    if (short_name_options_map_.count(short_name) == 0)
        return nullptr;
    return short_name_options_map_.at(short_name);
}

int OptionDefinitionCollection::FindIdByName(const std::string& name) const
{
    const OptionDefinition* ptr = FindByName(name);
    if (!ptr)
        return kNotFound;
    return ptr->GetId();
}

int OptionDefinitionCollection::FindIdByShortName(char short_name) const
{
    const OptionDefinition* ptr = FindByShortName(short_name);
    if (!ptr)
        return kNotFound;
    return ptr->GetId();
}

void OptionDefinitionCollection::PrintHelp() const
{
    for (const auto& map_pair : id_options_map_)
    {
        const OptionDefinition& definition = map_pair.second;
        definition.PrintHelp();
    }
}

// Option

Option::Option(OptionDefinitionCollection const* definitions, int id)
{
    assert(definitions && "Option definition cannot be null.");
    definitions_ = definitions;
    id_ = id;
    explicitly_provided_ = false;
    SetValueToDefault();
}

Option::Option(OptionDefinitionCollection const* definitions, int id, ValueType value)
{
    assert(definitions && "Option definition cannot be null.");
    definitions_ = definitions;
    id_ = id;
    explicitly_provided_ = false;
    SetValue(value);
}

void Option::SetValue(ValueType& value)
{
    assert(GetDefinition()->IsValid(value) && "The value is not a valid type.");
    value_ = value;
    if (GetDefinition()->UsesAcceptedValues())
    {
        const auto& accepted_values = GetDefinition()->GetAcceptedValues();
        const int index = accepted_values.at(value_);
        value_index_ = index;
    }
}

void Option::SetValue(ValueType&& value)
{
    assert(GetDefinition()->IsValid(value) && "The value is not a valid type.");
    value_ = std::move(value);
    if (GetDefinition()->UsesAcceptedValues())
    {
        const auto& accepted_values = GetDefinition()->GetAcceptedValues();
        const int index = accepted_values.at(value_);
        value_index_ = index;
    }
}

void Option::SetValueToDefault()
{
    OptionDefinition const* definition = GetDefinition();
    value_ = definition->GetDefaultValue();
    if (GetDefinition()->UsesAcceptedValues())
    {
        const auto& accepted_values = GetDefinition()->GetAcceptedValues();
        const int index = accepted_values.at(value_);
        value_index_ = index;
    }
}

OptionDefinition const* Option::GetDefinition() const
{
    assert(definitions_ && "Option definitions were null.");
    const OptionDefinition* definition = definitions_->FindById(id_);
    assert(definition && "Option definition was not found.");
    return definition;
}

// OptionCollection

OptionCollection::OptionCollection() : definitions_(nullptr), options_map_({}) {}

OptionCollection::OptionCollection(OptionDefinitionCollection const* definitions)
{
    SetDefinitions(definitions);
}

void OptionCollection::SetDefinitions(OptionDefinitionCollection const* definitions)
{
    definitions_ = definitions;

    // Create options and set them to their default value
    options_map_.clear();
    if (definitions) // If no option definitions were given, this will be null
    {
        for (const auto& map_pair : definitions->GetIdMap())
        {
            const int id = map_pair.first;
            options_map_.try_emplace(id, definitions, id);
        }
    }
    else
    {
        // definitions_ must always point to an OptionDefinitionCollection, even if it is empty. TODO: Not always?
        definitions_ = nullptr;
    }
}

const Option& OptionCollection::GetOption(std::string name) const
{
    const int id = definitions_->FindIdByName(name);
    assert(id != OptionDefinitionCollection::kNotFound && "Option with the given name wasn't found in the collection.");
    return GetOption(id);
}

Option& OptionCollection::GetOption(std::string name)
{
    const int id = definitions_->FindIdByName(name);
    assert(id != OptionDefinitionCollection::kNotFound && "Option with the given name wasn't found in the collection.");
    return GetOption(id);
}

const Option& OptionCollection::GetOption(char short_name) const
{
    const int id = definitions_->FindIdByShortName(short_name);
    assert(id != OptionDefinitionCollection::kNotFound && "Option with the given short name wasn't found in the collection.");
    return GetOption(id);
}

Option& OptionCollection::GetOption(char short_name)
{
    const int id = definitions_->FindIdByShortName(short_name);
    assert(id != OptionDefinitionCollection::kNotFound && "Option with the given short name wasn't found in the collection.");
    return GetOption(id);
}

void OptionCollection::SetValuesToDefault()
{
    for (auto& map_pair : options_map_)
    {
        auto& option = map_pair.second;
        option.SetValueToDefault();
    }
}

bool OptionCollection::ParseArgs(std::vector<std::string>& args, bool ignore_unknown_args)
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

    std::unordered_set<int> options_parsed;

    // Sets the value of an option given a value string
    auto SetValue = [this, &options_parsed](const char* value_str, const OptionDefinition* option_def) -> bool
    {
        auto& option = options_map_[option_def->GetId()];

        OptionDefinition::ValueType value_temp;
        if (ModuleOptionUtils::ConvertToValue(value_str, option_def->GetValueType(), value_temp))
        {
            return true; // Error occurred
        }

        if (!option_def->IsValid(value_temp))
        {
            const std::string option_type_str = option_def->GetOptionType() == kOption ? "option" : "command";
            std::cerr << "ERROR: The value \"" << value_str << "\" is not valid for the " << option_type_str << " \"" << option_def->GetDisplayName() << "\".\n";
            return true; // The value is not valid for this option definition
        }

        option.SetValue(std::move(value_temp));
        option.explicitly_provided_ = true;
        options_parsed.insert(option_def->GetId());
        return false;
    };

    const OptionDefinition* handling_option = nullptr;

    /*
     * If the previous argument is syntactically correct yet unrecognized (maybe it's a module option and
     *  we are reading global options now), the current argument in the following loop may be its value if
     *  the arguments were passed like: "--unrecognized value". Or it may be another option - recognized or not.
    */
    bool arg_might_be_value = false;

    // Main loop
    for (int i = 0; i < static_cast<int>(args.size()); i++)
    {
        auto& arg = args[i];
        Utils::StringTrimBothEnds(arg);
        if (arg.empty())
        {
            args.erase(args.begin() + i);
            i--; // Adjust for item just erased
            continue;
        }

        const OptionDefinition* def = handling_option;
        size_t equals_pos = std::string::npos;

        const bool this_arg_is_value = handling_option != nullptr;
        if (this_arg_is_value)
        {
            handling_option = nullptr;

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
            if (arg_might_be_value)
            {
                // Hopefully this is just a value from the preceding unrecognized argument
                arg_might_be_value = false;
                continue;
            }

            // Error: Invalid argument
            std::cerr << "ERROR: Invalid option: \"" << arg << "\"\n";
            return true;
        }

        const bool using_short_name = arg[1] != '-';
        if (using_short_name) // -f format argument (short name)
        {
            if (!isalpha(arg[1]))
            {
                if (arg_might_be_value)
                {
                    // Hopefully this is just a value from the preceding unrecognized argument
                    arg_might_be_value = false;
                    continue;
                }

                // Error: Short names must be alphabetic
                std::cerr << "ERROR: Invalid flag \"" << arg << "\": Flags must be comprised of only alphabetic characters.\n";
                return true;
            }

            equals_pos = arg.find_first_of('=');
            const bool using_equals = equals_pos != std::string::npos;
            if (using_equals) // Using the form: "-f=<value>"
            {
                if (equals_pos != 2)
                {
                    if (arg_might_be_value)
                    {
                        // Hopefully this is just a value from the preceding unrecognized argument
                        arg_might_be_value = false;
                        continue;
                    }

                    // Error: Short flags with an '=' must be of the form: "-f=<value>"
                    std::cerr << "ERROR: Invalid flag \"" << arg << "\": Unable to parse.\n";
                    return true;
                }

                // At this point, argument is deemed syntactically valid
                def = definitions_->FindByShortName(arg[1]);
            }
            else // Using the form: "-f", "-f <value>", or "-abcdef"
            {
                const bool using_several_short_args = arg.size() > 2;
                if (!using_several_short_args) // Using the form: "-f" or "-f <value>"
                {
                    if (!isalpha(arg[1]))
                    {
                        if (arg_might_be_value)
                        {
                            // Hopefully this is just a value from the preceding unrecognized argument
                            arg_might_be_value = false;
                            continue;
                        }

                        // Error: Short flags with an '=' must be of the form: "-f=<value>"
                        std::cerr << "ERROR: Invalid flag \"" << arg << "\": Unable to parse.\n";
                        return true;
                    }

                    // At this point, argument is deemed syntactically valid
                    def = definitions_->FindByShortName(arg[1]);
                }
                else // Using the form: "-abcdef" - this form cannot have a value after it
                {
                    for (unsigned j = 1; j < arg.size(); j++)
                    {
                        const char c = arg[j];
                        if (!isalpha(c))
                        {
                            if (arg_might_be_value)
                            {
                                // Hopefully this is just a value from the preceding unrecognized argument
                                arg_might_be_value = false;
                                break; // Break out of inner loop, then hit the continue
                            }

                            // Error: Short names must be alphabetic
                            std::cerr << "ERROR: Invalid flag '" << arg[j] << "': Flags must be comprised of only alphabetic characters.\n";
                            return true;
                        }

                        const OptionDefinition* temp_def = definitions_->FindByShortName(c);
                        
                        // Skip unrecognized options
                        if (!temp_def)
                            continue;

                        if (temp_def->GetValueType() != OptionDefinition::kBool)
                        {
                            // Error: When multiple short flags are strung together, all of them must be boolean-typed options
                            return true;
                        }

                        // Set the value
                        SetValue("1", temp_def);

                        arg.erase(j--, 1); // Remove this flag from argument, since it has been consumed
                    }

                    arg_might_be_value = false; // Impossible for next arg to be a value

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
            equals_pos = arg.find_first_of('=');
            std::string name = arg.substr(2, equals_pos - 2); // From start to '=' or end of string - whichever comes first
            def = definitions_->FindByName(name);
        }

        if (!def)
        {
            // Syntactically valid argument, yet unknown
            arg_might_be_value = true; // Next arg may be this unrecognized option's value
            continue; // Skip
        }

        // At this point, the current argument is both syntactically valid and a recognized option.

        /*
         * This is a recognized option, not the preceding unrecognised option's value.
         * Therefore, the next argument cannot be an unrecognized argument's value:
         */
        arg_might_be_value = false;

        if (options_parsed.count(def->GetId()) > 0)
        {
            // ERROR: Setting the same option twice
            const std::string option_type_str = def->GetOptionType() == kOption ? "option" : "command";
            if (using_short_name)
            {
                std::cerr << "ERROR: The " << option_type_str << " \"-" << def->GetShortName() << "\" is used more than once.\n";
            }
            else
            {
                std::cerr << "ERROR: The " << option_type_str << " \"--" << def->GetName() << "\" is used more than once.\n";
            }
            
            return true;
        }

        std::string value_str;
        if (equals_pos != std::string::npos) // Value is after the '='
        {
            value_str = arg.substr(equals_pos + 1);

            // Set the value
            if (SetValue(value_str.c_str(), def))
                return true; // Error occurred

            // Erase argument since it has been consumed
            args.erase(args.begin() + i);
            i--; // Adjust for item just erased
        }
        else // No '=' present
        {
            if (def->GetValueType() != OptionDefinition::kBool)
            {
                // The next argument will be the value
                handling_option = def;
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

    if (handling_option)
    {
        const std::string option_type_str = handling_option->GetOptionType() == kOption ? "option" : "command";
        std::cerr << "ERROR: The " << option_type_str << " \"" << handling_option->GetDisplayName() << "\" did not provide a value.\n";
        return true;
    }

    if (!ignore_unknown_args && !args.empty())
    {
        std::string error_str = "ERROR: Unknown option(s): ";
        for (unsigned i = 0; i < args.size(); i++)
        {
            error_str += args[i];
            if (i != args.size() - 1)
                error_str += ", ";
        }
        std::cerr << error_str << "\n";
        return true;
    }

    return false;
}

// ModuleOptionUtils

std::string ModuleOptionUtils::ConvertToString(const ValueType& value)
{
    const OptionDefinition::Type type = static_cast<OptionDefinition::Type>(value.index());
    switch (type)
    {
        case OptionDefinition::kBool:
            return std::get<bool>(value) ? "true" : "false";
        case OptionDefinition::kInt:
            return std::to_string(std::get<int>(value));
        case OptionDefinition::kDouble:
            return std::to_string(std::get<double>(value));
        case OptionDefinition::kString:
            return std::get<std::string>(value);
        default:
            return "ERROR";
    }
}

bool ModuleOptionUtils::ConvertToValue(const std::string& value_str, OptionDefinition::Type type, ValueType& return_val)
{
    switch (type)
    {
        case OptionDefinition::kBool:
        {
            std::string value_str_lower = value_str;
            unsigned i = 0;
            while (i < value_str_lower.size())
            {
                value_str_lower[i] = tolower(value_str_lower[i]);
                ++i;
            }

            if (value_str_lower == "0")
                return_val = false;
            else if (value_str_lower == "false")
                return_val = false;
            else if (value_str_lower == "1")
                return_val = true;
            else if (value_str_lower == "true")
                return_val = true;
            else
            {
                // Error: Invalid value for boolean-typed option
                std::cerr << "ERROR: Invalid value \"" << value_str << "\" for boolean-typed option.\n";
                return true;
            }
        } break;
        case OptionDefinition::kInt:
        {
            return_val = std::stoi(value_str);
        } break;
        case OptionDefinition::kDouble:
        {
            return_val = std::stof(value_str);
        } break;
        case OptionDefinition::kString:
        {
            return_val = value_str;
        } break;
    }

    return false;
}

bool ModuleOptionUtils::ConvertToValue(const char* value_str, OptionDefinition::Type type, ValueType& return_val)
{
    std::string value_str_const(value_str);
    return ConvertToValue(value_str_const, type, return_val);
}
