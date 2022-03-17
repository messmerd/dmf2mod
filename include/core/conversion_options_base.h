/*
    conversion_options_base.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares classes that ConversionOptionsInterface inherits.
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

namespace d2m {

// Forward declares
class ConversionOptionsBase;
template <typename T> class ConversionOptionsInterface;

// CRTP so each class derived from ConversionOptions can have its own static creation
template <typename T>
class ConversionOptionsStatic
{
protected:
    friend class Registrar;
    friend class ConversionOptionsBase;
    template<class A, class B> friend class ModuleInterface;

    // This class needs to be inherited
    ConversionOptionsStatic() = default;
    ConversionOptionsStatic(const ConversionOptionsStatic&) = default;
    ConversionOptionsStatic(ConversionOptionsStatic&&) = default;

    static ConversionOptionsBase* CreateStatic();

    // Returns a list of strings of the format: "-o, --option=[min,max]" or "-a" or "--flag" or "--flag=[]" etc.
    //  representing the command-line options for this module and their acceptable values
    static const std::shared_ptr<OptionDefinitionCollection>& GetDefinitionsStatic();

    // The output module type
    static ModuleType GetTypeStatic()
    {
        return m_Type;
    }

private:
    static const ModuleType m_Type;
    static const std::shared_ptr<OptionDefinitionCollection> m_OptionDefinitions;
};


// Base class for conversion options
class ConversionOptionsBase : public OptionCollection
{
public:
    ConversionOptionsBase() = default; // See ConversionOptionsInterface constructor
    ConversionOptionsBase(std::vector<std::string>& args); // See ConversionOptionsInterface constructor
    virtual ~ConversionOptionsBase() = default;

    /*
     * Create a new ConversionOptions object for the desired module type
     */
    template <class moduleClass, 
        class = std::enable_if_t<std::is_base_of<ModuleInterface<moduleClass, typename moduleClass::OptionsType>, moduleClass>{}>>
    static ConversionOptionsPtr Create()
    {
        return ConversionOptionsPtr(ModuleStatic<moduleClass>::m_CreateConversionOptionsStatic());
    }

    /*
     * Create a new module using the ModuleType enum to specify the desired module type
     * If the resulting ConversionOptions object evaluates to false or Get() == nullptr, the module type 
     * is probably not registered
     */
    static ConversionOptionsPtr Create(ModuleType type)
    {
        return Registrar::CreateConversionOptions(type);
    }

    /*
     * Cast an options pointer to a pointer of a derived type
     */
    template <class optionsClass, class = std::enable_if_t<std::is_base_of<ConversionOptionsInterface<optionsClass>, optionsClass>{}>>
    const optionsClass* Cast() const
    {
        return reinterpret_cast<const optionsClass*>(this);
    }

    /*
     * Get a ModuleType enum value representing the type of the conversion option's module
     */
    virtual ModuleType GetType() const = 0;

    /*
     * Returns a collection of option definitions which define the options available to modules of this type
     */
    virtual const std::shared_ptr<OptionDefinitionCollection>& GetDefinitions() const = 0;

    /*
     * Returns a collection of option definitions which define the options available to modules of type moduleType
     */
    static const std::shared_ptr<OptionDefinitionCollection>& GetDefinitions(ModuleType moduleType)
    {
        return Registrar::GetOptionDefinitions(moduleType);
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
        PrintHelp(ConversionOptionsStatic<optionsClass>::GetTypeStatic());
    }

    /*
     * Prints help message for the options of the given module type
     */
    static void PrintHelp(ModuleType moduleType);

protected:
    friend class Registrar;

    std::string m_OutputFile;
};

} // namespace d2m
