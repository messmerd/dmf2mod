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

// All conversion options classes must inherit this
template <typename T>
class ConversionOptionsInterface : public ConversionOptionsBase, public ConversionOptionsStatic<T>
{
public:
    ConversionOptionsInterface()
    {
        // Set the option values to their defaults
        m_Values.clear();
        const auto options = GetAvailableOptions();
        for (unsigned i = 0; i < options.Count(); i++)
        {
            m_Values.push_back(options.Item(i).GetDefaultValue());
        }
    }

    ModuleOption::value_t& GetOptionRef(int index) override
    {
        return m_Values[index];
    }

    ModuleOption::value_t& GetOptionRef(const std::string& name) override
    {
        return m_Values[GetAvailableOptions().FindIndexByName(name)];
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
