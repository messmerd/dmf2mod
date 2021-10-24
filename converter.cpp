
//#include "converter.h"
#include "modules.h"

#include <map>
#include <functional>
#include <any>
#include <type_traits>
#include <cstring>
//#include <typeindex>

extern std::map<std::string, ModuleType> G_ExtensionModuleMap;

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

const char* GetFilenameExt(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
    {
        return nullptr;
    }
    return dot;
}

