/*
    All changes needed to add support for new module types
    are done within this cpp file and its associated header file.
*/

#include "modules.h"

#include <map>
#include <functional>
//#include <typeindex>

// File extension to ModuleType map
std::map<std::string, ModuleType> G_ExtensionModuleMap =
{
    {".dmf", ModuleType::DMF},
    {".mod", ModuleType::MOD}
};

// Map which registers a module type enum value with the static create function associated with that module.
// This is being done statically here, but it could also be done at runtime if I wanted to load support for modules dynamically.
std::map<ModuleType, std::function<Module*(void)>> G_ModuleTypeCreateFunctionMap
{
    { ModuleType::DMF, &ModuleStatic<DMF>::CreateStatic },
    { ModuleType::MOD, &ModuleStatic<MOD>::CreateStatic }
};


// Map the module types to the ModuleType enum values
// TODO: This is nice right now, but if modules will be loaded at runtime, a map might be better

template<>
const ModuleType ModuleStatic<DMF>::_Type = ModuleType::DMF;

template<>
const ModuleType ModuleStatic<MOD>::_Type = ModuleType::MOD;

/*
// Type index to ModuleType map
std::map<std::type_index, ModuleType> G_TypeIndexModuleTypeMap =
{
    {std::type_index(typeid(DMF)), ModuleType::DMF},
    {std::type_index(typeid(MOD)), ModuleType::MOD}
};
*/
