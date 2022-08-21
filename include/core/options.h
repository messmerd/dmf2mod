/*
    options.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares Option, OptionCollection, OptionDefinition, and OptionDefinitionCollection, 
    which are used when working with command-line options.
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
    OPTION=0,
    COMMAND=1
};

// Stores a definition for a single command-line option
class OptionDefinition
{
public:
    friend class OptionDefinitionCollection;

    // The values correspond to value_t indices
    enum Type
    {
        BOOL = 0,
        INT = 1,
        DOUBLE = 2,
        STRING = 3,
    };

    using value_t = std::variant<bool, int, double, std::string>;

    // Constructors

    OptionDefinition() : m_OptionType(OPTION), m_Id(-1), m_ValueType(Type::BOOL), m_Name(""), m_ShortName('\0'), m_DefaultValue(false) {}

    // OptionDefinition without accepted values; The value can be anything allowed by the variant
    template<typename T, typename = std::enable_if_t<std::is_integral<T>{} || (std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>)>>
    OptionDefinition(OptionType type, T id, const std::string& name, char shortName, const value_t& defaultValue, const std::string& description)
        : m_OptionType(type), m_Id(static_cast<int>(id)), m_Name(name), m_ShortName(shortName), m_DefaultValue(defaultValue), m_AcceptedValues({}), m_AcceptedValuesOrdered({}), m_Description(description)
    {
        for (char c : name)
        {
            if (!std::isalnum(c))
                assert(false && "In OptionDefinition constructor: name must only contain alphanumeric characters or be empty.");
        }

        assert((shortName == '\0' || std::isalpha(shortName)) && "In OptionDefinition constructor: shortName must be an alphabetic character or '\\0'.");

        m_ValueType = static_cast<Type>(defaultValue.index());
    }

    // OptionDefinition with accepted values; Ensures that defaultValue and acceptedValues are the same type and are a valid variant alternative
    template <typename T, typename U, 
        typename = std::enable_if_t<std::is_constructible_v<value_t, U> && /* U must be a valid variant alternative */
        (std::is_integral_v<T> || (std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>))>> /* T must be int or enum class with int underlying type */
    OptionDefinition(OptionType type, T id, const std::string& name, char shortName, const U& defaultValue, const std::initializer_list<U>& acceptedValues, const std::string& description)
        : m_OptionType(type), m_Id(static_cast<int>(id)), m_Name(name), m_ShortName(shortName), m_DefaultValue(defaultValue), m_Description(description)
    {
        m_AcceptedValuesContainSpaces = false;

        bool found = false;
        int i = 0;
        for (const U& val : acceptedValues)
        {
            const auto& insertedVal = m_AcceptedValues.emplace(value_t(val), i++).first->first;
            m_AcceptedValuesOrdered.push_back(value_t(val));

            // Check for spaces (used when printing help)
            if (insertedVal.index() == OptionDefinition::STRING)
            {
                const std::string& str = std::get<std::string>(insertedVal);
                if (str.find(' ') != std::string::npos)
                    m_AcceptedValuesContainSpaces = true;
            }

            if (insertedVal == m_DefaultValue)
                found = true;
        }

        if (!found) // Avoid "unused variable" warning
            assert(false && "In OptionDefinition constructor: acceptedValues must contain the default value.");

        for (char c : name)
        {
            if (!std::isalnum(c))
                assert(false && "In OptionDefinition constructor: name must only contain alphanumeric characters or be empty.");
        }

        assert((shortName == '\0' || std::isalpha(shortName)) && "In OptionDefinition constructor: shortName must be an alphabetic character or '\\0'.");

        m_ValueType = static_cast<Type>(m_DefaultValue.index());
    }

    // Allows the use of string literals, which are converted to std::string
    template<typename T, typename = std::enable_if_t<std::is_integral_v<T> || (std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>)>>
    OptionDefinition(OptionType type, T id, const std::string& name, char shortName, const char* defaultValue, const std::initializer_list<std::string>& acceptedValues, const std::string& description)
        : OptionDefinition(type, id, name, shortName, std::string(defaultValue), acceptedValues, description) {}

    // Allows custom accepted values text which is used when printing help for this option. m_AcceptedValues is empty.
    template<typename T, typename U, typename = std::enable_if_t<std::is_constructible_v<value_t, U> && /* U must be a valid variant alternative */
    (std::is_integral_v<T> || (std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>))>> /* T must be int or enum class with int underlying type */
    OptionDefinition(OptionType type, T id, const std::string& name, char shortName, const U& defaultValue, const char* customAcceptedValuesText, const std::string& description)
        : OptionDefinition(type, id, name, shortName, defaultValue, description)
    {
        m_CustomAcceptedValuesText = customAcceptedValuesText;
    }

    // Getters and helpers

    OptionType GetOptionType() const { return m_OptionType; }
    int GetId() const { return m_Id; }
    Type GetValueType() const { return m_ValueType; }
    std::string GetName() const { return m_Name; };
    char GetShortName() const { return m_ShortName; }
    std::string GetDisplayName() const;
    value_t GetDefaultValue() const { return m_DefaultValue; }
    std::map<value_t, int> GetAcceptedValues() const { return m_AcceptedValues; }
    std::vector<value_t> GetAcceptedValuesOrdered() const { return m_AcceptedValuesOrdered; }
    std::string GetDescription() const { return m_Description; }

    bool HasName() const { return !m_Name.empty(); }
    bool HasShortName() const { return m_ShortName != '\0'; }
    bool UsesAcceptedValues() const { return m_AcceptedValues.size() > 0; }

    // Returns whether the given value can be assigned to an option with this option definition
    bool IsValid(const value_t& value) const;

    // Prints help info
    void PrintHelp() const;

protected:

    OptionType m_OptionType;

    // Used for quickly accessing specific options in OptionDefinitionCollection collection
    int m_Id;

    Type m_ValueType;
    std::string m_Name;
    char m_ShortName;

    value_t m_DefaultValue;

    std::map<value_t, int> m_AcceptedValues;
    std::vector<value_t> m_AcceptedValuesOrdered; // Stored in the order they were provided
    bool m_AcceptedValuesContainSpaces; // Whether double quotes are needed when printing

    std::string m_Description;

    // Only string-typed options can use custom accepted values text.
    // To use this feature, accepted values must take the form of: {"=<custom text here>"}
    std::string m_CustomAcceptedValuesText;
};


// A collection of OptionDefinition objects
class OptionDefinitionCollection
{
public:
    static constexpr int npos = -1;

    OptionDefinitionCollection() {};
    OptionDefinitionCollection(const OptionDefinitionCollection& other);
    OptionDefinitionCollection(std::initializer_list<OptionDefinition> options);

    size_t Count() const;

    // Access
    const std::map<int, OptionDefinition>& GetIdMap() const { return m_IdOptionsMap; }

    // Find methods
    const OptionDefinition* FindById(int id) const;
    const OptionDefinition* FindByName(const std::string& name) const;
    const OptionDefinition* FindByShortName(char shortName) const;
    int FindIdByName(const std::string& name) const;
    int FindIdByShortName(char shortName) const;

    // Other
    void PrintHelp() const;

private:
    std::map<int, OptionDefinition> m_IdOptionsMap;
    std::map<std::string, OptionDefinition*> m_NameOptionsMap;
    std::map<char, OptionDefinition*> m_ShortNameOptionsMap;
};


// An OptionDefinition + option value
class Option
{
public:
    friend class OptionCollection;
    using value_t = OptionDefinition::value_t;

    Option() : m_Definitions(nullptr), m_Id(-1) {}

    // Construct with definitions defined elsewhere
    Option(const std::shared_ptr<const OptionDefinitionCollection>& definitions, int id);

    // Construct with value. The definitions are defined elsewhere
    Option(const std::shared_ptr<const OptionDefinitionCollection>& definitions, int id, value_t value);

    void SetValue(value_t& value);
    void SetValue(value_t&& value);

    void SetValueToDefault();

    const value_t& GetValue() const { return m_Value; }

    template<typename T>
    const T& GetValue() const
    {
        // Will throw an exception if T is the wrong type
        return std::get<T>(m_Value);
    }

    int GetValueAsIndex() const
    {
        assert(GetDefinition()->UsesAcceptedValues());
        return m_ValueIndex;
    }

    bool GetExplicitlyProvided() const
    {
        return m_ExplicitlyProvided;
    }

    const OptionDefinition* GetDefinition() const;

private:

    // Rather than making a copy of definition for each Option, it instead will point to definitions defined elsewhere + an id.
    // This will work well for both definitions from the ConversionOptionsStatic class and custom definitions used by frontends.
    // std::shared_ptr<OptionDefinition> is not used to avoid the complications of having to use shared_ptr with each individual 
    // OptionDefinition.
    std::shared_ptr<const OptionDefinitionCollection> m_Definitions;
    int m_Id;

    value_t m_Value;

    // If using accepted values, this stores the index of m_Value within the accepted values list (enables better performance)
    int m_ValueIndex;

    // Whether the user explicitly provided the value for this option
    bool m_ExplicitlyProvided;
};


// A collection of Option objects
class OptionCollection
{
public:
    using value_t = OptionDefinition::value_t;

    OptionCollection();
    OptionCollection(const std::shared_ptr<const OptionDefinitionCollection>& definitions);

    // Access to definitions

    void SetDefinitions(const std::shared_ptr<const OptionDefinitionCollection>& definitions);
    const std::shared_ptr<const OptionDefinitionCollection>& GetDefinitions() const { return m_Definitions; }

    // Access to collection

    const std::map<int, Option>& GetOptionsMap() const { return m_OptionsMap; }

    // Get options based on id, name, or short name

    const Option& GetOption(int id) const { return m_OptionsMap.at(id); }
    Option& GetOption(int id) { return m_OptionsMap[id]; }

    template<typename T, class = std::enable_if_t<std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>>>
    const Option& GetOption(T id) const
    {
        return GetOption(static_cast<int>(id));
    }

    template<typename T, class = std::enable_if_t<std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>>>
    Option& GetOption(T id)
    {
        return GetOption(static_cast<int>(id));
    }

    const Option& GetOption(std::string name) const;
    Option& GetOption(std::string name);
    const Option& GetOption(char shortName) const;
    Option& GetOption(char shortName);

    // Other

    bool ParseArgs(std::vector<std::string>& args, bool ignoreUnknownArgs = false);
    void SetValuesToDefault();

private:
    std::shared_ptr<const OptionDefinitionCollection> m_Definitions;

    std::map<int, Option> m_OptionsMap;
};


// Provides option value conversion tools
class ModuleOptionUtils
{
public:
    using value_t = OptionDefinition::value_t;

    // Convert value_t to a string
    static std::string ConvertToString(const value_t& value);

    // Convert string + type to a value_t
    static bool ConvertToValue(const std::string& valueStr, OptionDefinition::Type type, value_t& returnVal);

    // Convert string + type to a value_t
    static bool ConvertToValue(const char* valueStr, OptionDefinition::Type type, value_t& returnVal);

private:

};

/*
    Helper function that allows an OptionDefinitionCollection to be easily created and passed to MODULE_DEFINE
*/
inline const std::shared_ptr<OptionDefinitionCollection> CreateOptionDefinitions(const std::initializer_list<OptionDefinition>& optionsDefinitions)
{
    return std::make_shared<OptionDefinitionCollection>(optionsDefinitions);
}

} // namespace d2m
