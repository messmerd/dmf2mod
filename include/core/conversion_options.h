/*
    conversion_options.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines an interface for conversion options.
    All conversion options classes must inherit ConversionOptionsInterface.
*/

#pragma once

#include "factory.h"
#include "module.h"
#include "utils.h"
#include "options.h"

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <type_traits>

namespace d2m {

// Forward declares
class ConversionOptionsBase;
template <typename T> class ConversionOptionsInterface;


// Specialized Info class for ConversionOptionsBase-derived classes
template<>
struct Info<ConversionOptionsBase> : public InfoBase
{
    OptionDefinitionCollection optionDefinitions;
};


// Base class for conversion options
class ConversionOptionsBase : public OptionCollection, public EnableReflection<ConversionOptionsBase>
{
protected:

    ConversionOptionsBase() = default; // See ConversionOptionsInterface constructor

public:

    virtual ~ConversionOptionsBase() = default;

    /*
     * Get the filename of the output file. Returns empty string if error occurred.
     */
    std::string GetOutputFilename() const { return m_OutputFile; }

    /*
     * Prints help message for this module's options
     */
    virtual void PrintHelp() const = 0;

    /*
     * Prints help message for the options of the given module type
     */
    static void PrintHelp(ModuleType moduleType);

protected:

    std::string m_OutputFile;
};


// All conversion options classes must inherit this using CRTP
template <class Derived>
class ConversionOptionsInterface : public EnableFactory<Derived, ConversionOptionsBase>
{
protected:

    ConversionOptionsInterface()
    {
        OptionCollection::SetDefinitions(&(this->GetInfo()->optionDefinitions));
    }

    ConversionOptionsInterface(const ConversionOptionsInterface&) = delete;
    ConversionOptionsInterface(ConversionOptionsInterface&&) = delete;

    void PrintHelp() const override
    {
        ConversionOptionsBase::PrintHelp(this->GetType());
    }

public:

    virtual ~ConversionOptionsInterface() = default;
};

} // namespace d2m
