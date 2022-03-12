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
        // Set the option values to their defaults
        m_ValuesMap.clear();
        const auto options = GetAvailableOptions();
        for (const auto& mapPair : options)
        {
            const int id = mapPair.first;
            const auto& option = mapPair.second;

            assert(m_ValuesMap.count(id) == 0 && "ConversionOptionsInterface(): Duplicate option id found.");

            m_ValuesMap[id] = option.GetDefaultValue();
        }
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

    ModuleOptions GetAvailableOptions() const override
    {
        return ConversionOptionsStatic<T>::GetAvailableOptionsStatic();
    }

    // Uses ModuleUtils::PrintHelp() by default, but can be overridden if desired
    virtual void PrintHelp() const override
    {
        ModuleUtils::PrintHelp(GetType());
    }
};

} // namespace d2m
