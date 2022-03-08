/*
    options_base.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares classes that ConversionOptionsInterface inherits.
*/

#pragma once

#include "registrar.h"
#include "module_base.h"

#include <string>
#include <vector>

// Forward declares
template <typename T> class ConversionOptionsInterface;


// CRTP so each class derived from ConversionOptions can have its own static creation
template <typename T>
class ConversionOptionsStatic
{
protected:
    friend class Registrar;
    template<class A, class B> friend class ModuleInterface;

    // This class needs to be inherited
    ConversionOptionsStatic() = default;
    ConversionOptionsStatic(const ConversionOptionsStatic&) = default;
    ConversionOptionsStatic(ConversionOptionsStatic&&) = default;

    static ConversionOptionsBase* CreateStatic();

    // Returns a list of strings of the format: "-o, --option=[min,max]" or "-a" or "--flag" or "--flag=[]" etc.
    //  representing the command-line options for this module and their acceptable values
    static std::vector<std::string> GetAvailableOptionsStatic();

    // The output module type
    static ModuleType GetTypeStatic()
    {
        return m_Type;
    }

private:
    static const ModuleType m_Type;
    static const std::vector<std::string> m_AvailableOptions;
};


// Base class for conversion options
class ConversionOptionsBase
{
public:
    ConversionOptionsBase() { m_OutputFile.clear(); }
    virtual ~ConversionOptionsBase() = default;

    /*
     * Create a new ConversionOptions object for the desired module type
     */
    template <class moduleClass, 
        class = typename std::enable_if<std::is_base_of<ModuleInterface<moduleClass, typename moduleClass::OptionsType>, moduleClass>{}>::type>
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
    template <class optionsClass, 
        class = typename std::enable_if<std::is_base_of<ConversionOptionsInterface<optionsClass>, optionsClass>{}>::type>
    const optionsClass* Cast() const
    {
        return reinterpret_cast<const optionsClass*>(this);
    }

    /*
     * Get a ModuleType enum value representing the type of the conversion option's module
     */
    virtual ModuleType GetType() const = 0;

    /*
     * Returns a list of strings of the format: "-o, --option=[min,max]" or "-a" or "--flag" or "--flag=[]" etc.
     *  representing the command-line options for this module and their acceptable values
     */
    virtual std::vector<std::string> GetAvailableOptions() const = 0;

    /*
     * Returns a list of strings of the format: "-o, --option=[min,max]" or "-a" or "--flag" or "--flag=[]" etc.
     *  representing the command-line options and their acceptable values for the given module type
     */
    static std::vector<std::string> GetAvailableOptions(ModuleType moduleType)
    {
        return Registrar::GetAvailableOptions(moduleType);
    }

    /*
     * Get the filename of the output file. Returns empty string if error occurred.
     */
    std::string GetOutputFilename() const { return m_OutputFile; }

    /*
     * Fills in this object's command-line arguments from a list of arguments.
     * Arguments are removed from the list if they are successfully parsed.
     */
    virtual bool ParseArgs(std::vector<std::string>& args) = 0;

    virtual void PrintHelp() = 0;

protected:
    friend class Registrar;

    std::string m_OutputFile;
};
