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
     * Get the filename of the output file. Returns empty string if error occurred.
     */
    std::string GetOutputFilename() const { return m_OutputFile; }

    /*
     * Prints help message for this module's options
     */
    virtual void PrintHelp() const = 0;

    template <class optionsClass /*, class = std::enable_if_t<detail::is_conversion_options_impl_v<optionsClass>>*/>
    static void PrintHelp()
    {
        PrintHelp(ConversionOptionsInterface<optionsClass>::GetTypeStatic());
    }

    /*
     * Prints help message for the options of the given module type
     */
    static void PrintHelp(ModuleType moduleType);

protected:
    //friend class Registrar;

    std::string m_OutputFile;
};

} // namespace d2m
