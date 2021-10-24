
#include "converter.h"
#include "modules.h"

#include <map>
#include <functional>
#include <any>
#include <type_traits>
#include <cstring>

// Initialize module registration maps
std::map<ModuleType, std::function<Module*(void)>> ModuleUtils::RegistrationMap = {};
std::map<std::string, ModuleType> ModuleUtils::FileExtensionMap = {};


ModuleType ModuleUtils::GetType(const char* filename)
{
    const char* ext = GetFilenameExt(filename);
    if (!ext)
        return ModuleType::NONE;
    
    const auto iter = ModuleUtils::FileExtensionMap.find(ext);
    if (iter != ModuleUtils::FileExtensionMap.end())
        return iter->second;

    return ModuleType::NONE;
}

const char* GetFilenameExt(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
    {
        return nullptr;
    }
    return dot + 1;
}

