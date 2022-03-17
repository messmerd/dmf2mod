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

// Initialize module registration maps
std::map<ModuleType, std::function<ModuleBase*(void)>> Registrar::m_RegistrationMap = {};
std::map<std::string, ModuleType> Registrar::m_FileExtensionMap = {};
std::map<ModuleType, std::function<ConversionOptionsBase*(void)>> Registrar::m_ConversionOptionsRegistrationMap = {};
std::map<ModuleType, std::shared_ptr<OptionDefinitionCollection>> Registrar::m_OptionDefinitionsMap = {};

// Registers all modules by associating their ModuleType enum values with their corresponding module classes
void Registrar::RegisterModules()
{
    m_RegistrationMap.clear();
    m_FileExtensionMap.clear();
    m_ConversionOptionsRegistrationMap.clear();
    m_OptionDefinitionsMap.clear();
    m_OptionDefinitionsMap[ModuleType::NONE] = std::make_shared<OptionDefinitionCollection>();

    // Register all modules here:
    Register<DMF>();
    Register<MOD>();
}

template <class T, class>
void Registrar::Register()
{
    const ModuleType moduleType = T::GetTypeStatic();
    const std::string fileExtension = T::GetFileExtensionStatic();

    // TODO: Check for file extension clashes here.
    // In order to make modules fully dynamically loaded, would need to make ModuleType an int and 
    // assign it to the module here rather than let them choose their own ModuleType.
    m_RegistrationMap[moduleType] = &T::CreateStatic;
    m_FileExtensionMap[fileExtension] = moduleType;

    m_ConversionOptionsRegistrationMap[moduleType] = &T::OptionsType::CreateStatic;
    
    //using OPT = T::OptionsType;
    m_OptionDefinitionsMap[moduleType] = T::OptionsType::GetDefinitionsStatic();

    // TODO: Is this a good idea?
    if (!m_OptionDefinitionsMap[moduleType])
        m_OptionDefinitionsMap[moduleType] = std::make_shared<OptionDefinitionCollection>();
}

ModulePtr Registrar::CreateModule(ModuleType moduleType)
{
    const auto iter = m_RegistrationMap.find(moduleType);
    if (iter != m_RegistrationMap.end())
    {
        // Call ModuleStatic<T>::CreateStatic()
        return ModulePtr(iter->second());
    }
    return nullptr;
}

ConversionOptionsPtr Registrar::CreateConversionOptions(ModuleType moduleType)
{
    const auto iter = m_ConversionOptionsRegistrationMap.find(moduleType);
    if (iter != m_ConversionOptionsRegistrationMap.end())
    {
        // Call ConversionOptionsStatic<T>::CreateStatic()
        return ConversionOptionsPtr(iter->second());
    }
    return nullptr;
}

std::vector<std::string> Registrar::GetAvailableModules()
{
    // TODO: Create a ModuleInfo class that contains OptionDefinitionCollection, module type, friendly name, file extension, etc.
    //          Then that object could be created in mod.cpp and dmf.cpp and used to register module info.

    std::vector<std::string> vec;
    for (const auto& mapPair : m_FileExtensionMap)
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
    for (const auto& mapPair : m_FileExtensionMap)
    {
        if (mapPair.second == moduleType)
            return mapPair.first;
    }
    return "";
}

const std::shared_ptr<OptionDefinitionCollection>& Registrar::GetOptionDefinitions(ModuleType moduleType)
{
    if (m_OptionDefinitionsMap.count(moduleType) > 0)
        return m_OptionDefinitionsMap.at(moduleType);
    return m_OptionDefinitionsMap.at(ModuleType::NONE); // Return empty OptionDefinitionCollection
}
