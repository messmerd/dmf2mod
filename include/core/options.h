/*
    options.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares ModuleOption and ModuleOptions which are used
    when working with command-line options.
*/

#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>
#include <variant>
#include <cassert>

namespace d2m {

// Forward declares
class ModuleOptions;

// Store definition for a single command-line option
class ModuleOption
{
public:
    friend class ModuleOptions;

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

    ModuleOption()
        : m_Id(-1), m_Type(Type::BOOL), m_Name(""), m_ShortName('\0'), m_DefaultValue(false)
    {}

    // ModuleOption without accepted values; The value can be anything allowed by the variant
    template<typename T, typename = std::enable_if_t<std::is_integral<T>{} || (std::is_enum<T>{} && std::is_convertible<std::underlying_type_t<T>, int>{})>>
    ModuleOption(T id, const std::string& name, char shortName, const value_t& defaultValue, const std::string& description, bool equalsPreferred)
        : m_Id(static_cast<int>(id)), m_Name(name), m_ShortName(shortName), m_DefaultValue(defaultValue), m_AcceptedValues({}), m_Description(description), m_EqualsPreferred(equalsPreferred)
    {
        #ifndef NDEBUG
        for (char c : name)
        {
            assert(std::isalpha(c) && "In ModuleOption constructor: name must only contain alphabetic characters or be empty.");
        }
        #endif

        assert((shortName == '\0' || std::isalpha(shortName)) && "In ModuleOption constructor: shortName must be an alphabetic character or '\\0'.");

        m_Type = static_cast<Type>(defaultValue.index());
    }

    // ModuleOption with accepted values; Ensures that defaultValue and acceptedValues are the same type and are a valid variant alternative
    template <typename T, typename U, 
        typename = std::enable_if_t<std::is_constructible<value_t, U>{} && /* U must be a valid variant alternative */
        (std::is_integral<T>{} || (std::is_enum<T>{} && std::is_convertible<std::underlying_type_t<T>, int>{}))>> /* T must be int or enum class with int underlying type */
    ModuleOption(T id, const std::string& name, char shortName, const U& defaultValue, const std::initializer_list<U>& acceptedValues, const std::string& description, bool equalsPreferred)
        : m_Id(static_cast<int>(id)), m_Name(name), m_ShortName(shortName), m_DefaultValue(defaultValue), m_Description(description), m_EqualsPreferred(equalsPreferred)
    {
        bool found = false;
        for (const U& val : acceptedValues)
        {
            m_AcceptedValues.insert(value_t(val));

            if (val == defaultValue)
                found = true;
        }
        
        if (!found) // Avoid "unused variable" warning
            assert(false && "In ModuleOption constructor: acceptedValues must contain the default value.");
        
        #ifndef NDEBUG
        for (char c : name)
        {
            assert(std::isalpha(c) && "In ModuleOption constructor: name must only contain alphabetic characters or be empty.");
        }
        #endif

        assert((shortName == '\0' || std::isalpha(shortName)) && "In ModuleOption constructor: shortName must be an alphabetic character or '\\0'.");

        m_Type = static_cast<Type>(m_DefaultValue.index());
    }

    // Allows the use of string literals, which are converted to std::string
    template<typename T, typename = std::enable_if_t<std::is_integral<T>{} || (std::is_enum<T>{} && std::is_convertible<std::underlying_type_t<T>, int>{})>>
    ModuleOption(T id, const std::string& name, char shortName, const char* defaultValue, const std::initializer_list<std::string>& acceptedValues, const std::string& description, bool equalsPreferred)
        : ModuleOption(id, name, shortName, std::string(defaultValue), acceptedValues, description, equalsPreferred)
    {}

    // Getters and helpers

    int GetId() const { return m_Id; }
    Type GetType() const { return m_Type; }
    std::string GetName() const;
    char GetShortName() const { return m_ShortName; }
    value_t GetDefaultValue() const { return m_DefaultValue; }
    std::set<value_t> GetAcceptedValues() const { return m_AcceptedValues; }
    std::string GetDescription() const { return m_Description; }
    bool IsEqualsPreferred() const { return m_EqualsPreferred; }
    
    bool HasName() const { return !m_Name.empty(); }
    bool HasShortName() const { return m_ShortName != '\0'; }
    bool UsesAcceptedValues() const { return m_AcceptedValues.size() > 0; }
    bool IsValid(const value_t& value) const;

private:

    // Used for quickly accessing specific options in ModuleOptions collection
    int m_Id;

    Type m_Type;
    std::string m_Name;
    char m_ShortName;

    value_t m_DefaultValue;

    std::set<value_t> m_AcceptedValues;

    std::string m_Description;

    // Whether "--foo=bar" is preferred over "--foo bar" for non-boolean options. Used when printing.
    bool m_EqualsPreferred;
};

class ModuleOptions
{
public:
    static constexpr int npos = -1;
    using iterator = std::map<int, ModuleOption>::iterator;
    using const_iterator = std::map<int, ModuleOption>::const_iterator;

    ModuleOptions() {};
    ModuleOptions(const ModuleOptions& other);
    ModuleOptions(std::initializer_list<ModuleOption> options);

    size_t Count() const;

    // Iteration
    iterator begin() { return m_IdOptionsMap.begin(); }
    iterator end() { return m_IdOptionsMap.end(); }
    const_iterator begin() const { return m_IdOptionsMap.begin(); }
    const_iterator end() const { return m_IdOptionsMap.end(); }

    // Find methods
    const ModuleOption* FindById(int id) const;
    const ModuleOption* FindByName(const std::string& name) const;
    const ModuleOption* FindByShortName(char shortName) const;
    int FindIdByName(const std::string& name) const;
    int FindIdByShortName(char shortName) const;

private:
    std::map<int, ModuleOption> m_IdOptionsMap;
    std::map<std::string, ModuleOption*> m_NameOptionsMap;
    std::map<char, ModuleOption*> m_ShortNameOptionsMap;
};


// Maps a ModuleOption id to that option's value
using OptionValues = std::map<int, ModuleOption::value_t>;


class ModuleOptionUtils
{
public:
    using value_t = ModuleOption::value_t;

    // Initializes values to their defaults found in optionDefs
    static void SetToDefault(const ModuleOptions& optionDefs, OptionValues& valuesMap);

    // Convert value_t to a string
    static std::string ConvertToString(const value_t& value);

    // Convert string + type to a value_t
    static bool ConvertToValue(const std::string& valueStr, ModuleOption::Type type, value_t& returnVal);

    // Convert string + type to a value_t
    static bool ConvertToValue(const char* valueStr, ModuleOption::Type type, value_t& returnVal);

    // Global options/values can be set/retrieved using these methods:

    static void SetGlobalOptions(const ModuleOptions& globalOptionsDefs, const OptionValues& globalValuesMap);
    static void SetGlobalOptionsDefinitions(const ModuleOptions& globalOptionsDefs);
    static const ModuleOptions& GetGlobalOptionsDefinitions();
    static const OptionValues& GetGlobalOptionsValues();
    
    template<typename T>
    static T GetGlobalOptionValue(int optionId)
    {
        // Will throw an exception if optionName doesn't exist
        return std::get<T>(ModuleOptionUtils::m_GlobalValuesMap.at(optionId));
    }
    
    template<typename T>
    static void SetGlobalOptionValue(int optionId, T value)
    {
        // Will throw an exception if optionName doesn't exist
        ModuleOptionUtils::m_GlobalValuesMap[optionId] = value;
    }

    template<typename T>
    static T GetGlobalOptionValue(const char* optionName)
    {
        // Will throw an exception if optionName doesn't exist
        const int optionId = ModuleOptionUtils::m_GlobalOptions.FindIdByName(optionName);
        return GetGlobalOptionValue<T>(optionId);
    }
    
    template<typename T>
    static void SetGlobalOptionValue(const char* optionName, T value)
    {
        // Will throw an exception if optionName doesn't exist
        const int optionId = ModuleOptionUtils::m_GlobalOptions.FindIdByName(optionName);
        SetGlobalOptionValue<T>(optionId, value);
    }

private:

    // Global options/values
    static ModuleOptions m_GlobalOptions;
    static OptionValues m_GlobalValuesMap;
};

} // namespace d2m
