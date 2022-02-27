/*
    modules.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Part of the dmf2mod core. All changes needed to add support 
    for new module types are done within this cpp file and its 
    header file by the same name.

    Currently, modules supported by the Module factory are statically linked 
    with the dmf2mod core and dynamically registered. With some minor changes, 
    module libraries could be dynamically linked as well.
    If I did that, I would no longer use the ModuleType enum and instead 
    ModuleUtils would assign an integer ID to each loaded module library 
    during registration in this file.
    
    However, dynamic loading of module libraries seems a bit overkill and 
    the current setup fulfills the goal of allowing new module support to be 
    added with minimal changes to the dmf2mod core.
*/

#include "modules.h"

// Registers all modules by associating their ModuleType enum values with their corresponding module classes
void ModuleUtils::RegisterModules()
{
    RegistrationMap.clear();
    FileExtensionMap.clear();
    ConversionOptionsRegistrationMap.clear();

    // Register all modules here:
    Register<DMF>();
    Register<MOD>();
}
