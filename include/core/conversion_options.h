/*
    conversion_options.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines an interface for conversion options.
    All conversion options classes must inherit ConversionOptionsInterface.
*/

#pragma once

#include "conversion_options_base.h"

#include <iostream>
#include <string>
#include <vector>

namespace d2m {

// All conversion options classes must inherit this using CRTP
template <class Derived>
class ConversionOptionsInterface : public ConversionOptionsBase
{
protected:
    friend class Registrar;

    ConversionOptionsInterface()
    {
        SetDefinitions(GetDefinitions());
    }

    ConversionOptionsInterface(std::vector<std::string>& args)
    {
        // Parse arguments to set option values
        SetDefinitions(GetDefinitions());
        ParseArgs(args);
    }

    ConversionOptionsInterface(const ConversionOptionsInterface&) = default;
    ConversionOptionsInterface(ConversionOptionsInterface&&) = default;

    static const ConversionOptionsInfo& GetInfo();

    ModuleType GetType() const override
    {
        return m_Info.GetType();
    }

    const std::shared_ptr<const OptionDefinitionCollection>& GetDefinitions() const override
    {
        return m_Info.GetDefinitions();
    }

    void PrintHelp() const override
    {
        ConversionOptionsBase::PrintHelp(GetType());
    }

private:

    static const ConversionOptionsInfo m_Info;
};

} // namespace d2m