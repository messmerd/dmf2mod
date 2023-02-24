/*
 * options.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Declares Option, OptionCollection, OptionDefinition, and OptionDefinitionCollection, 
 * which are used when working with command-line options.
 */

#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>
#include <variant>
#include <memory>
#include <cassert>

namespace d2m {

// Forward declares
class OptionDefinition;
class OptionDefinitionCollection;
class Option;
class OptionCollection;

enum OptionType
{
    kOption=0,
    kCommand=1
};

// Stores a definition for a single command-line option
class OptionDefinition
{
public:
    friend class OptionDefinitionCollection;

    // The values correspond to ValueType indices
    enum Type
    {
        kBool = 0,
        kInt = 1,
        kDouble = 2,
        kString = 3,
    };

    using ValueType = std::variant<bool, int, double, std::string>;

    // Constructors

    OptionDefinition() : option_type_(kOption), id_(-1), value_type_(Type::kBool), name_(""), short_name_('\0'), default_value_(false) {}

    // OptionDefinition without accepted values; The value can be anything allowed by the variant
    template<typename T, std::enable_if_t<std::is_integral<T>{} || (std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>), bool> = true>
    OptionDefinition(OptionType type, T id, const std::string& name, char short_name, const ValueType& default_value, const std::string& description)
        : option_type_(type), id_(static_cast<int>(id)), name_(name), short_name_(short_name), default_value_(default_value), description_(description)
    {
        for (char c : name)
        {
            if (!std::isalnum(c))
                assert(false && "In OptionDefinition constructor: name must only contain alphanumeric characters or be empty.");
        }

        assert((short_name == '\0' || std::isalpha(short_name)) && "In OptionDefinition constructor: short_name must be an alphabetic character or '\\0'.");

        value_type_ = static_cast<Type>(default_value.index());
    }

    // OptionDefinition with accepted values; Ensures that default_value and accepted_values are the same type and are a valid variant alternative
    template<typename T, typename U, std::enable_if_t<std::is_constructible_v<ValueType, U>, bool> = true>
    OptionDefinition(OptionType type, T id, const std::string& name, char short_name, const U& default_value, const std::initializer_list<U>& accepted_values, const std::string& description)
        : OptionDefinition(type, id, name, short_name, default_value, description)
    {
        accepted_values_contain_spaces_ = false;

        bool found = false;
        int i = 0;
        for (const U& val : accepted_values)
        {
            const auto& inserted_val = accepted_values_.emplace(ValueType(val), i++).first->first;
            accepted_values_ordered_.push_back(ValueType(val));

            // Check for spaces (used when printing help)
            if (inserted_val.index() == OptionDefinition::kString)
            {
                const std::string& str = std::get<std::string>(inserted_val);
                if (str.find(' ') != std::string::npos)
                    accepted_values_contain_spaces_ = true;
            }

            if (inserted_val == default_value_)
                found = true;
        }

        if (!found) // Avoid "unused variable" warning
            assert(false && "In OptionDefinition constructor: accepted_values must contain the default value.");
    }

    // Allows the use of string literals, which are converted to std::string
    template<typename T, std::enable_if_t<std::is_integral_v<T> || (std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>), bool> = true>
    OptionDefinition(OptionType type, T id, const std::string& name, char short_name, const char* default_value, const std::initializer_list<std::string>& accepted_values, const std::string& description)
        : OptionDefinition(type, id, name, short_name, std::string(default_value), accepted_values, description)
    {
    }

    // Allows custom accepted values text which is used when printing help for this option. accepted_values_ is empty.
    template<typename T, typename U, std::enable_if_t<std::is_constructible_v<ValueType, U>, bool> = true> /* U must be a valid variant alternative */
    OptionDefinition(OptionType type, T id, const std::string& name, char short_name, const U& default_value, const char* custom_accepted_values_text, const std::string& description)
        : OptionDefinition(type, id, name, short_name, default_value, description)
    {
        custom_accepted_values_text_ = custom_accepted_values_text;
    }

    // Getters and helpers

    OptionType GetOptionType() const { return option_type_; }
    int GetId() const { return id_; }
    Type GetValueType() const { return value_type_; }
    std::string GetName() const { return name_; };
    char GetShortName() const { return short_name_; }
    std::string GetDisplayName() const;
    ValueType GetDefaultValue() const { return default_value_; }
    std::map<ValueType, int> GetAcceptedValues() const { return accepted_values_; }
    std::vector<ValueType> GetAcceptedValuesOrdered() const { return accepted_values_ordered_; }
    std::string GetDescription() const { return description_; }

    bool HasName() const { return !name_.empty(); }
    bool HasShortName() const { return short_name_ != '\0'; }
    bool UsesAcceptedValues() const { return accepted_values_.size() > 0; }

    // Returns whether the given value can be assigned to an option with this option definition
    bool IsValid(const ValueType& value) const;

    // Prints help info
    void PrintHelp() const;

protected:

    OptionType option_type_;

    // Used for quickly accessing specific options in OptionDefinitionCollection collection
    int id_;

    Type value_type_;
    std::string name_;
    char short_name_;

    ValueType default_value_;

    std::map<ValueType, int> accepted_values_;
    std::vector<ValueType> accepted_values_ordered_; // Stored in the order they were provided
    bool accepted_values_contain_spaces_; // Whether double quotes are needed when printing

    std::string description_;

    // Only string-typed options can use custom accepted values text.
    // To use this feature, accepted values must take the form of: {"=<custom text here>"}
    std::string custom_accepted_values_text_;
};


// A collection of OptionDefinition objects
class OptionDefinitionCollection
{
public:
    static constexpr int kNotFound = -1;

    OptionDefinitionCollection() {};
    OptionDefinitionCollection(const OptionDefinitionCollection& other);
    OptionDefinitionCollection(const std::initializer_list<OptionDefinition>& options);

    size_t Count() const;

    // Access
    const std::map<int, OptionDefinition>& GetIdMap() const { return id_options_map_; }

    // Find methods
    const OptionDefinition* FindById(int id) const;
    const OptionDefinition* FindByName(const std::string& name) const;
    const OptionDefinition* FindByShortName(char short_name) const;
    int FindIdByName(const std::string& name) const;
    int FindIdByShortName(char short_name) const;

    // Other
    void PrintHelp() const;

private:
    std::map<int, OptionDefinition> id_options_map_;
    std::map<std::string, OptionDefinition*> name_options_map_;
    std::map<char, OptionDefinition*> short_name_options_map_;
};


// An OptionDefinition + option value
class Option
{
public:
    friend class OptionCollection;
    using ValueType = OptionDefinition::ValueType;

    Option() : definitions_(nullptr), id_(-1) {}

    // Construct with definitions defined elsewhere
    Option(OptionDefinitionCollection const* definitions, int id);

    // Construct with value. The definitions are defined elsewhere
    Option(OptionDefinitionCollection const* definitions, int id, ValueType value);

    void SetValue(ValueType& value);
    void SetValue(ValueType&& value);

    void SetValueToDefault();

    const ValueType& GetValue() const { return value_; }

    template<typename T>
    const T& GetValue() const
    {
        // Will throw an exception if T is the wrong type
        return std::get<T>(value_);
    }

    int GetValueAsIndex() const
    {
        assert(GetDefinition()->UsesAcceptedValues());
        return value_index_;
    }

    bool GetExplicitlyProvided() const
    {
        return explicitly_provided_;
    }

    OptionDefinition const* GetDefinition() const;

private:

    // Rather than making a copy of definition for each Option, it instead will point to definitions defined elsewhere + an id.
    // This will work well for both definitions from the Info struct and custom definitions used by frontends.
    // TODO: Use OptionDefinition instead? Use std::shared_ptr?
    OptionDefinitionCollection const* definitions_;
    int id_;

    ValueType value_;

    // If using accepted values, this stores the index of value_ within the accepted values list (enables better performance)
    int value_index_;

    // Whether the user explicitly provided the value for this option
    bool explicitly_provided_;
};


// A collection of Option objects
class OptionCollection
{
public:
    using ValueType = OptionDefinition::ValueType;

    OptionCollection();
    OptionCollection(OptionDefinitionCollection const* definitions);

    // Access to definitions

    void SetDefinitions(OptionDefinitionCollection const* definitions);
    OptionDefinitionCollection const* GetDefinitions() const { return definitions_; }

    // Access to collection

    const std::map<int, Option>& GetOptionsMap() const { return options_map_; }

    // Get options based on id, name, or short name

    const Option& GetOption(int id) const { return options_map_.at(id); }
    Option& GetOption(int id) { return options_map_[id]; }

    template<typename T, std::enable_if_t<std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>, bool> = true>
    const Option& GetOption(T id) const
    {
        return GetOption(static_cast<int>(id));
    }

    template<typename T, std::enable_if_t<std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>, bool> = true>
    Option& GetOption(T id)
    {
        return GetOption(static_cast<int>(id));
    }

    const Option& GetOption(std::string name) const;
    Option& GetOption(std::string name);
    const Option& GetOption(char short_name) const;
    Option& GetOption(char short_name);

    // Other

    bool ParseArgs(std::vector<std::string>& args, bool ignore_unknown_args = false);
    void SetValuesToDefault();

private:
    OptionDefinitionCollection const* definitions_;

    std::map<int, Option> options_map_;
};


// Provides option value conversion tools
class ModuleOptionUtils
{
public:
    using ValueType = OptionDefinition::ValueType;

    // Convert ValueType to a string
    static std::string ConvertToString(const ValueType& value);

    // Convert string + type to a ValueType
    static bool ConvertToValue(const std::string& value_str, OptionDefinition::Type type, ValueType& return_val);

    // Convert string + type to a ValueType
    static bool ConvertToValue(const char* value_str, OptionDefinition::Type type, ValueType& return_val);

private:

};

} // namespace d2m
