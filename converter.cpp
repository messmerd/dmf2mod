
#include "converter.h"
#include "modules.h"

#include <map>
#include <functional>
#include <any>
//#include <typeindex>
#include <type_traits>
#include <cstring>

extern std::map<std::string, ModuleType> G_ExtensionModuleMap;
//extern std::map<std::type_index, ModuleType> G_TypeIndexModuleTypeMap;

extern Module* CreateModule(ModuleType type);



// TODO: Need a way to register all modules in this map at compile time. Want to reduce number of places to edit
//      in order to add new modules.
// std::function<T*(void)>()
//template<typename T>
std::map<ModuleType, std::any> G_ModuleTypeCreateFunctionMap
{
    { ModuleType::DMF, &ModuleStatic<DMF>::CreateStatic },
    { ModuleType::MOD, &ModuleStatic<MOD>::CreateStatic }
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

template<typename T>
T* Module::Create(ModuleType type)
{
    //return CreateModule(type);

    const auto iter = G_ModuleTypeCreateFunctionMap.find(type);
    if (iter != G_ModuleTypeCreateFunctionMap.end())
    {
        std::function<T*(void)> func = std::any_cast<std::function<T*(void)>>(iter->second);
        return func();
    }
    return nullptr;
}

template <class T, class>
T* Module::Create()
{
    // Ensure that only child classes of Module are used:
    //static_assert(std::is_base_of<Module, T>::value);

    return new T;
}

template <class T, class>
ModuleType Module::Type()
{
    return ModuleStatic<T>::_Type;
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

