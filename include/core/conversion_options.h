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

// All conversion options classes must inherit this
template <typename T>
class ConversionOptionsInterface : public ConversionOptionsBase, public ConversionOptionsStatic<T>
{
public:
    ConversionOptionsInterface()
    {
        SetDefinitions(ConversionOptionsStatic<T>::GetDefinitionsStatic());
    }

    ConversionOptionsInterface(std::vector<std::string>& args)
    {
        // Parse arguments to set option values
        SetDefinitions(ConversionOptionsStatic<T>::GetDefinitionsStatic());
        ParseArgs(args);
    }

    /* 
     * Prints help message for this object's options
     */
    void PrintHelp() const override
    {
        ConversionOptionsBase::PrintHelp(ConversionOptionsStatic<T>::GetTypeStatic());
    }

protected:
    ModuleType GetType() const override
    {
        return ConversionOptionsStatic<T>::GetTypeStatic();
    }

    const std::shared_ptr<OptionDefinitionCollection>& GetDefinitions() const override
    {
        return ConversionOptionsStatic<T>::GetDefinitionsStatic();
    }
};

} // namespace d2m
