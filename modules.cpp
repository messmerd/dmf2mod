#include "modules.h"

#include <map>
#include <functional>
//#include <typeindex>

Module* CreateModule(ModuleType type)
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

// File extension to ModuleType map
std::map<std::string, ModuleType> G_ExtensionModuleMap =
{
    {".dmf", ModuleType::DMF},
    {".mod", ModuleType::MOD}
};





/*
// Type index to ModuleType map
std::map<std::type_index, ModuleType> G_TypeIndexModuleTypeMap =
{
    {std::type_index(typeid(DMF)), ModuleType::DMF},
    {std::type_index(typeid(MOD)), ModuleType::MOD}
};
*/
