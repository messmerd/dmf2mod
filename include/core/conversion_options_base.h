/*
    conversion_options_base.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares ConversionOptionsBase which is inherited by ConversionOptionsInterface.
*/

#pragma once

#include "registrar.h"
#include "module_base.h"
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

// Base class for conversion options
class ConversionOptionsBase : public OptionCollection
{
public:
    ConversionOptionsBase() = default; // See ConversionOptionsInterface constructor
    virtual ~ConversionOptionsBase() = default;

    /*
     * Create a new ConversionOptions object for the desired module type
     */
    template <class moduleClass, 
        class = std::enable_if_t<std::is_base_of<ModuleInterface<moduleClass, class moduleClass::OptionsType>, moduleClass>{}>>
    static ConversionOptionsPtr Create()
    {
        return ModuleInterface<moduleClass, class moduleClass::OptionsType>::m_Info.m_CreateOptionsFunc();
    }

    /*
     * Create a new module using the ModuleType enum to specify the desired module type
     * If the resulting ConversionOptions object evaluates to false or Get() == nullptr, the module type 
     * is probably not registered
     */
    static ConversionOptionsPtr Create(ModuleType moduleType)
    {
        return Registrar::GetConversionOptionsInfo(moduleType)->m_CreateFunc();
    }

    /*
     * Cast an options pointer to a pointer of a derived type
     */
    template <class Derived, class = std::enable_if_t<std::is_base_of<ConversionOptionsInterface<Derived>, Derived>{}>>
    const Derived* Cast() const
    {
        return static_cast<const Derived*>(this);
    }

    /*
     * Get a ModuleType enum value representing the type of the conversion option's module
     */
    virtual ModuleType GetType() const = 0;

    /*
     * Returns a collection of option definitions which define the options available to modules of this type
     */
    virtual const std::shared_ptr<const OptionDefinitionCollection>& GetDefinitions() const = 0;

    /*
     * Returns a collection of option definitions which define the options available to modules of type moduleType
     */
    static const std::shared_ptr<const OptionDefinitionCollection>& GetDefinitions(ModuleType moduleType)
    {
        return Registrar::GetConversionOptionsInfo(moduleType)->GetDefinitions();
    }

    /*
     * Get the filename of the output file. Returns empty string if error occurred.
     */
    std::string GetOutputFilename() const { return m_OutputFile; }

    /*
     * Prints help message for this module's options
     */
    virtual void PrintHelp() const = 0;

    template <class optionsClass, class = std::enable_if_t<std::is_base_of<ConversionOptionsInterface<optionsClass>, optionsClass>{}>>
    static void PrintHelp()
    {
        PrintHelp(ConversionOptionsInterface<optionsClass>::GetTypeStatic());
    }

    /*
     * Prints help message for the options of the given module type
     */
    static void PrintHelp(ModuleType moduleType);

    /*
     * Get info about this type of conversion options
     */
    const ConversionOptionsInfo* GetConversionOptionsInfo() const
    {
        return Registrar::GetConversionOptionsInfo(GetType());
    }

protected:
    friend class Registrar;

    std::string m_OutputFile;
};

} // namespace d2m
