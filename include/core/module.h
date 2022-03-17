/*
    module.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines an interface for modules.
    All module classes must inherit ModuleInterface.
*/

#pragma once

#include "module_base.h"
#include "conversion_options.h"
#include "global_options.h"

namespace d2m {

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

    const std::shared_ptr<OptionDefinitionCollection>& GetOptionDefinitions() const override
    {
        return ConversionOptionsStatic<O>::GetDefinitionsStatic();
    }

    std::shared_ptr<OptionsType> GetOptions()
    {
        return std::reinterpret_pointer_cast<OptionsType>(m_Options);
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
template<> ModuleBase* d2m::ModuleStatic<moduleClass>::CreateStatic(); \
template<> ConversionOptionsBase* d2m::ConversionOptionsStatic<optionsClass>::CreateStatic(); \
template<> std::string d2m::ModuleStatic<moduleClass>::GetFileExtensionStatic(); \
template<> const std::shared_ptr<OptionDefinitionCollection>& d2m::ConversionOptionsStatic<optionsClass>::GetDefinitionsStatic();

/*
    Helper macro for defining static data members for a template specialization 
    of module and conversion options.
    Must be called in a module's cpp file AFTER including the header.
*/
#define MODULE_DEFINE(moduleClass, optionsClass, enumType, fileExt, optionDefinitionsPtr) \
template<> const ModuleType d2m::ModuleStatic<moduleClass>::m_Type = enumType; \
template<> const std::string d2m::ModuleStatic<moduleClass>::m_FileExtension = fileExt; \
template<> const ModuleType d2m::ConversionOptionsStatic<optionsClass>::m_Type = enumType; \
template<> const std::shared_ptr<OptionDefinitionCollection> d2m::ConversionOptionsStatic<optionsClass>::m_OptionDefinitions = optionDefinitionsPtr; \
template<> ModuleBase* d2m::ModuleStatic<moduleClass>::CreateStatic() { return new moduleClass; } \
template<> ConversionOptionsBase* d2m::ConversionOptionsStatic<optionsClass>::CreateStatic() { return new optionsClass; } \
template<> std::string d2m::ModuleStatic<moduleClass>::GetFileExtensionStatic() { return m_FileExtension; } \
template<> const std::shared_ptr<OptionDefinitionCollection>& d2m::ConversionOptionsStatic<optionsClass>::GetDefinitionsStatic() { return m_OptionDefinitions; };

} // namespace d2m
