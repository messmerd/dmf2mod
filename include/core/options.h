/*
    options.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines an interface for conversion options.
    All conversion options classes must inherit ConversionOptionsInterface.
*/

#pragma once

#include "options_base.h"

#include <string>
#include <vector>

// All conversion options classes must inherit this
template <typename T>
class ConversionOptionsInterface : public ConversionOptionsBase, public ConversionOptionsStatic<T>
{
protected:
    ModuleType GetType() const override
    {
        return ConversionOptionsStatic<T>::GetTypeStatic();
    }

    std::vector<std::string> GetAvailableOptions() const override
    {
        return ConversionOptionsStatic<T>::GetAvailableOptionsStatic();
    }
};
