/*
    options.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares ModuleOption and ModuleOptions which are used
    when working with command-line options.
*/

#pragma once

#include <string>
#include <set>
#include <vector>
#include <variant>
#include <exception>

#include <iostream>

// Forward declares
class ModuleOptions;

// Stores a single command-line option
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
        : m_Type(Type::BOOL), m_DefaultValue(false), m_Value(false)
    {}

    // ModuleOption without accepted values; The value can be anything allowed by the variant
    ModuleOption(const std::string& name, const std::string& shortName, const value_t& defaultValue, const std::string& description)
        : m_Name(name), m_ShortName(shortName), m_DefaultValue(defaultValue), m_Value(defaultValue), m_AcceptedValues({}), m_Description(description)
    {
        m_Type = static_cast<Type>(defaultValue.index());
    }

    // ModuleOption with accepted values; Ensures that defaultValue and acceptedValues are the same type and are a valid variant alternative
    template <typename T, typename = std::enable_if_t<std::is_constructible<value_t, T>{}>>
    ModuleOption(const std::string& name, const std::string& shortName, const T& defaultValue, const std::initializer_list<T>& acceptedValues, const std::string& description)
        : m_Name(name), m_ShortName(shortName), m_DefaultValue(defaultValue), m_Value(defaultValue), m_Description(description)
    {
        bool found = false;
        for (const T& val : acceptedValues)
        {
            m_AcceptedValues.insert(value_t(val));

            if (val == defaultValue)
                found = true;
        }
        
        if (!found)
            throw std::invalid_argument("In ModuleOption constructor: acceptedValues must contain the default value.");
        
        m_Type = static_cast<Type>(m_DefaultValue.index());
    }

    // Allows the use of string literals, which are converted to std::string
    ModuleOption(const std::string& name, const std::string& shortName, const char* defaultValue, const std::initializer_list<std::string>& acceptedValues, const std::string& description)
        : ModuleOption(name, shortName, std::string(defaultValue), acceptedValues, description)
    {}

    // Getters and helpers

    Type GetType() const { return m_Type; }
    std::string GetName() const { return m_Name; }
    std::string GetShortName() const { return m_ShortName; }
    value_t GetDefaultValue() const { return m_DefaultValue; }
    value_t GetValue() const { return m_Value; }
    std::set<value_t> GetAcceptedValues() const { return m_AcceptedValues; }
    std::string GetDescription() const { return m_Description; }
    
    bool HasShortName() const { return m_ShortName.size() > 0; }
    bool UsesAcceptedValues() const { return m_AcceptedValues.size() > 0; }

public:

    Type m_Type;
    std::string m_Name;
    std::string m_ShortName;

    value_t m_DefaultValue;
    value_t m_Value;

    std::set<value_t> m_AcceptedValues;

    std::string m_Description;
};

class ModuleOptions
{
public:
    static constexpr int npos = -1;

    ModuleOptions() = default;
    ModuleOptions(std::initializer_list<ModuleOption> options)
        : m_Options(options)
    {}

    size_t Count() const;
    const ModuleOption& Item(unsigned index) const;

    ModuleOption* FindByName(std::string name);
    ModuleOption* FindByShortName(std::string shortName);
    int FindIndexByName(std::string name) const;
    int FindIndexByShortName(std::string shortName) const;

private:
    std::vector<ModuleOption> m_Options;
};
