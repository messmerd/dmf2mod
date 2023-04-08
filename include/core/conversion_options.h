/*
 * conversion_options.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines an interface for conversion options.
 * All conversion options classes must inherit ConversionOptionsInterface.
 */

#pragma once

#include "core/factory.h"
#include "core/options.h"
#include "utils/utils.h"

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <type_traits>

namespace d2m {

// Forward declares
class ConversionOptionsBase;
template<typename T> class ConversionOptionsInterface;


// Specialized Info class for ConversionOptionsBase-derived classes
template<>
struct Info<ConversionOptionsBase> : public InfoBase
{
    OptionDefinitionCollection option_definitions;
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
    [[nodiscard]] auto Cast() const -> std::shared_ptr<const T>
    {
        return std::static_pointer_cast<const T>(shared_from_this());
    }

    /*
     * Get the filename of the output file. Returns empty string if error occurred.
     */
    auto GetOutputFilename() const -> std::string { return output_file_; }

    /*
     * Prints help message for this module's options
     */
    virtual void PrintHelp() const = 0;

    /*
     * Prints help message for the options of the given module type
     */
    static void PrintHelp(ModuleType module_type);

protected:

    std::string output_file_;
};


// All conversion options classes must inherit this using CRTP
template<class Derived>
class ConversionOptionsInterface : public EnableFactory<Derived, ConversionOptionsBase>
{
protected:

    ConversionOptionsInterface()
    {
        OptionCollection::SetDefinitions(&(this->GetInfo()->option_definitions));
    }

    void PrintHelp() const override
    {
        ConversionOptionsBase::PrintHelp(this->GetType());
    }

public:

    ConversionOptionsInterface(const ConversionOptionsInterface&) = delete;
    ConversionOptionsInterface(ConversionOptionsInterface&&) = delete;
    virtual ~ConversionOptionsInterface() = default;
};

} // namespace d2m
