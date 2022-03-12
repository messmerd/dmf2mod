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
        : m_Id(-1), m_Type(Type::BOOL), m_DefaultValue(false)
    {}

    // ModuleOption without accepted values; The value can be anything allowed by the variant
    template<typename T, typename = std::enable_if_t<std::is_integral<T>{} || (std::is_enum<T>{} && std::is_convertible<std::underlying_type_t<T>, int>{})>>
    ModuleOption(T id, const std::string& name, const std::string& shortName, const value_t& defaultValue, const std::string& description)
        : m_Id(static_cast<int>(id)), m_Name(name), m_ShortName(shortName), m_DefaultValue(defaultValue), m_AcceptedValues({}), m_Description(description)
    {
        m_Type = static_cast<Type>(defaultValue.index());
    }

    // ModuleOption with accepted values; Ensures that defaultValue and acceptedValues are the same type and are a valid variant alternative
    template <typename T, typename U, 
        typename = std::enable_if_t<std::is_constructible<value_t, U>{} && /* U must be a valid variant alternative */
        (std::is_integral<T>{} || (std::is_enum<T>{} && std::is_convertible<std::underlying_type_t<T>, int>{}))>> /* T must be int or enum class with int underlying type */
    ModuleOption(T id, const std::string& name, const std::string& shortName, const U& defaultValue, const std::initializer_list<U>& acceptedValues, const std::string& description)
        : m_Id(static_cast<int>(id)), m_Name(name), m_ShortName(shortName), m_DefaultValue(defaultValue), m_Description(description)
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
        
        m_Type = static_cast<Type>(m_DefaultValue.index());
    }

    // Allows the use of string literals, which are converted to std::string
    template<typename T, typename = std::enable_if_t<std::is_integral<T>{} || (std::is_enum<T>{} && std::is_convertible<std::underlying_type_t<T>, int>{})>>
    ModuleOption(T id, const std::string& name, const std::string& shortName, const char* defaultValue, const std::initializer_list<std::string>& acceptedValues, const std::string& description)
        : ModuleOption(id, name, shortName, std::string(defaultValue), acceptedValues, description)
    {}

    // Getters and helpers

    int GetId() const { return m_Id; }
    Type GetType() const { return m_Type; }
    std::string GetName() const { return m_Name; }
    std::string GetShortName() const { return m_ShortName; }
    value_t GetDefaultValue() const { return m_DefaultValue; }
    std::set<value_t> GetAcceptedValues() const { return m_AcceptedValues; }
    std::string GetDescription() const { return m_Description; }
    
    bool HasShortName() const { return m_ShortName.size() > 0; }
    bool UsesAcceptedValues() const { return m_AcceptedValues.size() > 0; }

public:

    int m_Id; // Used for quickly accessing specific options in ModuleOptions collection
    Type m_Type;
    std::string m_Name;
    std::string m_ShortName;

    value_t m_DefaultValue;

    std::set<value_t> m_AcceptedValues;

    std::string m_Description;
};

class ModuleOptions
{
public:
    static constexpr int npos = -1;
    using iterator = std::map<int, ModuleOption>::iterator;
    using const_iterator = std::map<int, ModuleOption>::const_iterator;

    ModuleOptions() = default;
    ModuleOptions(std::initializer_list<ModuleOption> options)
    {
        // Initialize collection and id --> option mapping
        m_OptionsMap.clear();
        for (auto& option : options)
        {
            const int id = option.GetId();
            assert(m_OptionsMap.count(id) == 0 && "ModuleOptions(...): Duplicate option id found.");
            m_OptionsMap[id] = option;
        }
    }

    size_t Count() const;

    // Iteration
    iterator begin() { return m_OptionsMap.begin(); }
    iterator end() { return m_OptionsMap.end(); }
    const_iterator begin() const { return m_OptionsMap.begin(); }
    const_iterator end() const { return m_OptionsMap.end(); }

    // Find methods
    ModuleOption* FindById(int id);
    ModuleOption* FindByName(std::string name);
    ModuleOption* FindByShortName(std::string shortName);
    int FindIdByName(std::string name) const;
    int FindIdByShortName(std::string shortName) const;

private:
    std::map<int, ModuleOption> m_OptionsMap;
};

} // namespace d2m
