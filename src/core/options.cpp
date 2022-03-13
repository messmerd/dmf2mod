/*
    options.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines ModuleOption and ModuleOptions which are used
    when working with command-line options.
*/

#include "options.h"
#include <iostream>
#include <cassert>

using namespace d2m;

// ModuleOption

std::string ModuleOption::GetDisplayName() const
{
    if (!m_Name.empty())
        return m_Name;
    return std::string(1, m_ShortName);
}

bool ModuleOption::IsValid(const value_t& value) const
{
    if (value.index() != GetType())
        return false;
    if (!UsesAcceptedValues())
        return true;
    return m_AcceptedValues.count(value) > 0;
}

// ModuleOptions

ModuleOptions::ModuleOptions(const ModuleOptions& other)
{
    m_IdOptionsMap = other.m_IdOptionsMap;
    for (auto& mapPair : m_IdOptionsMap)
    {
        ModuleOption* moduleOption = &mapPair.second;

        const std::string name = moduleOption->GetName();
        m_NameOptionsMap[name] = moduleOption;
        
        const char shortName = moduleOption->GetShortName();
        m_ShortNameOptionsMap[shortName] = moduleOption;
    }
}

ModuleOptions::ModuleOptions(std::initializer_list<ModuleOption> options)
{
    // Initialize collection + mappings
    m_IdOptionsMap.clear();
    m_NameOptionsMap.clear();
    m_ShortNameOptionsMap.clear();
    for (auto& option : options)
    {
        // Id mapping
        const int id = option.GetId();
        assert(m_IdOptionsMap.count(id) == 0 && "ModuleOptions(...): Duplicate option id found.");
        m_IdOptionsMap[id] = option;

        // Name mapping
        if (option.HasName())
        {
            const std::string name = option.GetName();
            assert(m_NameOptionsMap.count(name) == 0 && "ModuleOptions(...): Duplicate option name found.");
            m_NameOptionsMap[name] = &m_IdOptionsMap[id];
        }

        // Short name mapping
        if (option.HasShortName())
        {
            const char shortName = option.GetShortName();
            assert(m_ShortNameOptionsMap.count(shortName) == 0 && "ModuleOptions(...): Duplicate option short name found.");
            m_ShortNameOptionsMap[shortName] = &m_IdOptionsMap[id];
        }
    }
}

size_t ModuleOptions::Count() const
{
    return m_IdOptionsMap.size();
}

const ModuleOption* ModuleOptions::FindById(int id) const
{
    if (m_IdOptionsMap.count(id) == 0)
        return nullptr;
    return &m_IdOptionsMap.at(id);
}

const ModuleOption* ModuleOptions::FindByName(const std::string& name) const
{
    if (m_NameOptionsMap.count(name) == 0)
        return nullptr;
    return m_NameOptionsMap.at(name);
}

const ModuleOption* ModuleOptions::FindByShortName(char shortName) const
{
    if (m_ShortNameOptionsMap.count(shortName) == 0)
        return nullptr;
    return m_ShortNameOptionsMap.at(shortName);
}

int ModuleOptions::FindIdByName(const std::string& name) const
{
    const ModuleOption* ptr = FindByName(name);
    if (!ptr)
        return npos;
    return ptr->GetId();
}

int ModuleOptions::FindIdByShortName(char shortName) const
{
    const ModuleOption* ptr = FindByShortName(shortName);
    if (!ptr)
        return npos;
    return ptr->GetId();
}

// ModuleOptionUtils

ModuleOptions ModuleOptionUtils::m_GlobalOptions = {};
OptionValues ModuleOptionUtils::m_GlobalValuesMap = {};

void ModuleOptionUtils::SetToDefault(const ModuleOptions& optionDefs, OptionValues& valuesMap)
{
    // Set the option values to their defaults
    valuesMap.clear();
    for (const auto& mapPair : optionDefs)
    {
        const int id = mapPair.first;
        const auto& option = mapPair.second;

        assert(valuesMap.count(id) == 0 && "ModuleOptionUtils::SetToDefault(): Duplicate option id found.");

        valuesMap[id] = option.GetDefaultValue();
    }
}

std::string ModuleOptionUtils::ConvertToString(const value_t& value)
{
    const ModuleOption::Type type = static_cast<ModuleOption::Type>(value.index());
    switch (type)
    {
        case ModuleOption::BOOL:
            return std::get<bool>(value) ? "true" : "false";
        case ModuleOption::INT:
            return std::to_string(std::get<int>(value));
        case ModuleOption::DOUBLE:
            return std::to_string(std::get<double>(value));
        case ModuleOption::STRING:
            return std::get<std::string>(value);
        default:
            return "ERROR";
    }
}

bool ModuleOptionUtils::ConvertToValue(const std::string& valueStr, ModuleOption::Type type, value_t& returnVal)
{
    switch (type)
    {
        case ModuleOption::BOOL:
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
                // Error: Invalid value for bool-typed option
                std::cerr << "ERROR: Invalid value \"" << valueStr << "\" for bool-typed option.\n";
                return true;
            }
        } break;
        case ModuleOption::INT:
        {
            returnVal = atoi(valueStr.c_str());
        } break;
        case ModuleOption::DOUBLE:
        {
            returnVal = atof(valueStr.c_str());
        } break;
        case ModuleOption::STRING:
        {
            returnVal = valueStr;
        } break;
    }

    return false;
}

bool ModuleOptionUtils::ConvertToValue(const char* valueStr, ModuleOption::Type type, value_t& returnVal)
{
    std::string valueStrConst(valueStr);
    return ConvertToValue(valueStrConst, type, returnVal);
}

void ModuleOptionUtils::SetGlobalOptions(const ModuleOptions& globalOptionsDefs, const OptionValues& globalValuesMap)
{
    m_GlobalOptions = globalOptionsDefs;
    m_GlobalValuesMap = globalValuesMap;
}

void ModuleOptionUtils::SetGlobalOptionsDefinitions(const ModuleOptions& globalOptionsDefs)
{
    m_GlobalOptions = globalOptionsDefs;
}

const ModuleOptions& ModuleOptionUtils::GetGlobalOptionsDefinitions()
{
    return m_GlobalOptions;
}

const OptionValues& ModuleOptionUtils::GetGlobalOptionsValues()
{
    return m_GlobalValuesMap;
}
