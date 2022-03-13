/*
    conversion_options.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines an interface for conversion options.
    All conversion options classes must inherit ConversionOptionsInterface.
*/

#pragma once

#include "conversion_options_base.h"

#include <string>
#include <vector>

namespace d2m {

// All conversion options classes must inherit this
template <typename T>
class ConversionOptionsInterface : public ConversionOptionsBase, public ConversionOptionsStatic<T>
{
public:
    ConversionOptionsInterface()
    {
        SetToDefault();
    }

    ConversionOptionsInterface(std::vector<std::string>& args)
    {
        // Parse arguments to set option values
        SetToDefault();
        ModuleUtils::ParseArgs(args, GetAvailableOptions(), m_ValuesMap);
    }

    void SetToDefault() override
    {
        // Set the option values to their defaults
        ModuleOptionUtils::SetToDefault(GetAvailableOptions(), m_ValuesMap);
    }

    ModuleOption::value_t& GetValueRef(int id) override
    {
        return m_ValuesMap[id];
    }

    const ModuleOption::value_t& GetValueRef(int id) const override
    {
        return m_ValuesMap.at(id);
    }

    ModuleOption::value_t& GetValueRef(const std::string& name) override
    {
        return m_ValuesMap[GetAvailableOptions().FindIdByName(name)];
    }

    const ModuleOption::value_t& GetValueRef(const std::string& name) const override
    {
        return m_ValuesMap.at(GetAvailableOptions().FindIdByName(name));
    }

protected:
    ModuleType GetType() const override
    {
        return ConversionOptionsStatic<T>::GetTypeStatic();
    }

    const ModuleOptions& GetAvailableOptions() const override
    {
        return ConversionOptionsStatic<T>::GetAvailableOptionsStatic();
    }

    // Uses ModuleUtils::ParseArgs(...) by default, but can be overridden if desired
    virtual bool ParseArgs(std::vector<std::string>& args) override
    {
        return ModuleUtils::ParseArgs(args, GetAvailableOptions(), m_ValuesMap);
    }

    // Uses ModuleUtils::PrintHelp() by default, but can be overridden if desired
    virtual void PrintHelp() const override
    {
        ModuleUtils::PrintHelp(GetType());
    }
};

} // namespace d2m
