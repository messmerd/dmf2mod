/*
    module.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines an interface for modules.
    All module classes must inherit ModuleInterface.
*/

#pragma once

#include "module_base.h"
#include "options.h"

// All module classes must inherit this
template <typename T, typename O>
class ModuleInterface : public ModuleBase, public ModuleStatic<T>
{
public:
    using OptionsType = O;

protected:
    ModuleType GetType() const override
    {
        return ModuleStatic<T>::GetTypeStatic();
    }

    std::string GetFileExtension() const override
    {
        return ModuleStatic<T>::GetFileExtensionStatic();
    }

    std::vector<std::string> GetAvailableOptions() const override
    {
        return ConversionOptionsStatic<O>::GetAvailableOptionsStatic();
    }
};

/*
    Helper macro for declaring explicit instantiation of module and 
    conversion options.
    Must be called in a module's header file AFTER including module.h.
*/
#define MODULE_DECLARE(moduleClass, optionsClass) \
class moduleClass; \
class optionsClass; \
template<> ModuleBase* ModuleStatic<moduleClass>::CreateStatic(); \
template<> ConversionOptionsBase* ConversionOptionsStatic<optionsClass>::CreateStatic(); \
template<> std::string ModuleStatic<moduleClass>::GetFileExtensionStatic(); \
template<> std::vector<std::string> ConversionOptionsStatic<optionsClass>::GetAvailableOptionsStatic();

/*
    Helper macro for defining static data members for a template specialization 
    of module and conversion options.
    Must be called in a module's cpp file AFTER including the header.
*/
#define MODULE_DEFINE(moduleClass, optionsClass, enumType, fileExt, availOptions) \
template<> const ModuleType ModuleStatic<moduleClass>::m_Type = enumType; \
template<> const std::string ModuleStatic<moduleClass>::m_FileExtension = fileExt; \
template<> const ModuleType ConversionOptionsStatic<optionsClass>::m_Type = enumType; \
template<> const std::vector<std::string> ConversionOptionsStatic<optionsClass>::m_AvailableOptions = availOptions; \
template<> ModuleBase* ModuleStatic<moduleClass>::CreateStatic() { return new moduleClass; } \
template<> ConversionOptionsBase* ConversionOptionsStatic<optionsClass>::CreateStatic() { return new optionsClass; } \
template<> std::string ModuleStatic<moduleClass>::GetFileExtensionStatic() { return m_FileExtension; } \
template<> std::vector<std::string> ConversionOptionsStatic<optionsClass>::GetAvailableOptionsStatic() { return m_AvailableOptions; };
