/*
    module.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines an interface for modules.
    All module classes must inherit ModuleInterface.
*/

#pragma once

#include "module_base.h"
#include "data.h"
#include "note.h"
#include "conversion_options.h"
#include "global_options.h"

namespace d2m {

// All module classes must derive from this using CRTP
template <class Derived>
class ModuleInterface : public EnableFactory<Derived, ModuleBase>
{
public:

    inline const ModuleData<Derived>& GetData() const { return m_Data; }
    inline const ModuleGlobalData<Derived>& GetGlobalData() const { return GetData().GlobalData(); }

    std::string GetTitle() const override { return GetGlobalData().title; }
    std::string GetAuthor() const override { return GetGlobalData().author; }

protected:

    inline ModuleData<Derived>& GetData() { return m_Data; }
    inline ModuleGlobalData<Derived>& GetGlobalData() { return GetData().GlobalData(); }

private:

    //static const ModuleInfo m_Info;     // Info inherent to every module of type Derived
    ModuleData<Derived> m_Data;         // Song information for a particular module file
};


/*
    Helper macro for writing explicit instantiation definitions for module and
    conversion options class templates and members.
    Add to a module's header file AFTER including module.h.
    This isn't needed in GCC, but Clang throws a fit without it.
*/
#define MODULE_DECLARE(moduleClass, optionsClass)\
class moduleClass; class optionsClass;\
template<> const ConversionOptionsInfo& d2m::ConversionOptionsInterface<optionsClass>::GetInfo();\
template<> const ModuleInfo& d2m::ModuleInterface<moduleClass>::GetInfo();

/*
    Helper macro for defining static data members for a template specialization of module and
    conversion options.
    Must be called in a module's cpp file AFTER including the header.
*/
#define MODULE_DEFINE(moduleClass, optionsClass, enumType, friendlyName, fileExt, optionDefinitions)\
template<> const d2m::ConversionOptionsInfo d2m::ConversionOptionsInterface<optionsClass>::m_Info = d2m::ConversionOptionsInfo::Create<optionsClass>(enumType, optionDefinitions);\
template<> const d2m::ModuleInfo d2m::ModuleInterface<moduleClass>::m_Info = d2m::ModuleInfo::Create<moduleClass>(enumType, friendlyName, fileExt);\
template<> const d2m::ConversionOptionsInfo& d2m::ConversionOptionsInterface<optionsClass>::GetInfo() { return m_Info; }\
template<> const d2m::ModuleInfo& d2m::ModuleInterface<moduleClass>::GetInfo() { return m_Info; }

} // namespace d2m
