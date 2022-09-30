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
class ConversionOptionsBase : public OptionCollection, public EnableReflection<ConversionOptionsBase>, public std::enable_shared_from_this<ConversionOptionsBase>
{
protected:

    ConversionOptionsBase() = default; // See ConversionOptionsInterface constructor

public:

    virtual ~ConversionOptionsBase() = default;

    /*
     * Cast ConversionOptionsPtr to std::shared_ptr<T> where T is the derived ConversionOptions class
     */
    template<class T, std::enable_if_t<std::is_base_of_v<ConversionOptionsBase, T>, bool> = true>
    std::shared_ptr<T> Cast() const
    {
        return std::static_pointer_cast<T>(shared_from_this());
    }

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
