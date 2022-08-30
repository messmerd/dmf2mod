/*
    registrar.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines the Registrar class, which registers all the available 
    modules at runtime, provides factory methods for creating Module and 
    ConversionOptions objects, and provides helper methods for retrieving 
    info about registered modules.

    All changes needed to add support for new module types are done within 
    this cpp file, its header file by the same name, and all_modules.h.

    Currently, modules supported by the Module factory are statically linked 
    with the dmf2mod core and dynamically registered through the 
    Registrar. With some minor changes, module libraries could be dynamically 
    linked as well. If I did that, I would no longer use the ModuleType enum and 
    instead Registrar would assign an integer ID to each loaded module library 
    during registration in this file.
    
    However, dynamic loading of module libraries seems a bit overkill and 
    the current setup fulfills the goal of allowing new module support to be 
    added with minimal changes to the dmf2mod core.
*/

#include "registrar.h"
#include "utils/utils.h"
#include "module_base.h"
#include "conversion_options_base.h"

#include "all_modules.h"

using namespace d2m;


// Initialize for the primary template
template<class T> std::map<ModuleType, BuilderBase const*> Factory<T>::m_Builders{};
template<class T> std::map<ModuleType, InfoBase*> Factory<T>::m_Info{};
template<class T> std::map<std::type_info, ModuleType> Factory<T>::m_TypeToEnum{};
template<class T> bool Factory<T>::m_Initialized = false;




/*
// Initialize module registration maps
std::map<ModuleType, const ModuleInfo*> Registrar::m_ModuleRegistrationMap = {};
std::map<ModuleType, const ConversionOptionsInfo*> Registrar::m_ConversionOptionsRegistrationMap = {};
std::map<std::string, ModuleType> Registrar::m_FileExtensionMap = {};

// Registers all modules by associating their ModuleType enum values with their corresponding module classes
void Registrar::RegisterModules()
{
    m_ModuleRegistrationMap.clear();
    m_ConversionOptionsRegistrationMap.clear();
    m_FileExtensionMap.clear();

    // Register all modules here:
    Register<DMF>();
    Register<MOD>();
}

template <class T, class>
void Registrar::Register()
{
    using OptionsType = typename T::options_t;
    const ModuleType moduleType = T::GetInfo().GetType();

    const ModuleInfo& moduleInfo = ModuleInterface<T, OptionsType>::GetInfo();
    const ConversionOptionsInfo& conversionOptionsInfo = ConversionOptionsInterface<OptionsType>::GetInfo();

    m_ModuleRegistrationMap[moduleType] = &moduleInfo;
    m_ConversionOptionsRegistrationMap[moduleType] = &conversionOptionsInfo;

    m_FileExtensionMap[moduleInfo.GetFileExtension()] = moduleType;
}

std::vector<ModuleType> Registrar::GetAvailableModules()
{
    std::vector<ModuleType> vec;
    for (const auto& mapPair : m_ModuleRegistrationMap)
    {
        vec.push_back(mapPair.first);
    }
    return vec;
}

ModuleType Registrar::GetTypeFromFilename(const std::string& filename)
{
    std::string ext = ModuleUtils::GetFileExtension(filename);
    if (ext.empty())
        return ModuleType::NONE;

    const auto iter = m_FileExtensionMap.find(ext);
    if (iter != m_FileExtensionMap.end())
        return iter->second;

    return ModuleType::NONE;
}

ModuleType Registrar::GetTypeFromFileExtension(const std::string& extension)
{
    if (extension.empty())
        return ModuleType::NONE;
    
    const auto iter = m_FileExtensionMap.find(extension);
    if (iter != m_FileExtensionMap.end())
        return iter->second;

    return ModuleType::NONE;
}

std::string Registrar::GetExtensionFromType(ModuleType moduleType)
{
    for (const auto& [tempFileExt, tempType] : m_FileExtensionMap)
    {
        if (tempType == moduleType)
            return tempFileExt;
    }
    return "";
}

std::shared_ptr<const OptionDefinitionCollection> Registrar::GetOptionDefinitions(ModuleType moduleType)
{
    if (m_ConversionOptionsRegistrationMap.count(moduleType) > 0)
        return m_ConversionOptionsRegistrationMap.at(moduleType)->GetDefinitions();
    return nullptr;
}

ModuleInfo const* Registrar::GetModuleInfo(ModuleType moduleType)
{
    return m_ModuleRegistrationMap.at(moduleType);
}

ConversionOptionsInfo const* Registrar::GetConversionOptionsInfo(ModuleType moduleType)
{
    return m_ConversionOptionsRegistrationMap.at(moduleType);
}
*/
