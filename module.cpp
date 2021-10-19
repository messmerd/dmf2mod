/*
    Edit this file and modules.h to add support for new modules
    Search for [[MODULES]] to find locations that need to be edited.

*/

#include "module.h"
#include "modules.h"

#include <string>
#include <map>
#include <cstring>
#include <typeindex>

// [[MODULES]]
static std::map<std::string, ModuleType> G_ExtensionModuleMap =
{
    {".dmf", ModuleType::DMF},
    {".mod", ModuleType::MOD}
};

// [[MODULES]]
static std::map<std::type_index, ModuleType> G_TypeIndexModuleTypeMap =
{
    {std::type_index(typeid(DMF)), ModuleType::DMF},
    {std::type_index(typeid(MOD)), ModuleType::MOD}
};

ModuleType ModuleUtils::GetType(const char* filename)
{
    const char* ext = GetFilenameExt(filename);
    if (!ext)
        return ModuleType::NONE;
    
    const auto iter = G_ExtensionModuleMap.find(ext);
    if (iter != G_ExtensionModuleMap.end())
        return iter->second;

    return ModuleType::NONE;
}

// [[MODULES]]
Module* Module::Create(ModuleType type)
{
    switch (type)
    {
        case ModuleType::DMF:
            return new DMF;
        case ModuleType::MOD:
            return new MOD;
        
        default:
            return nullptr;
    }
}

template<typename T> static T* Create()
{
    const auto iter = G_TypeIndexModuleTypeMap.find(std::type_index(typeid(T)));
    if (iter != G_TypeIndexModuleTypeMap.end())
        return Module::Create(iter->second);
    
    return nullptr;
}

const char* GetFilenameExt(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
    {
        return nullptr;
    }
    return dot;
}

