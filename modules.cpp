/*
    All changes needed to add support for new module types
    are done within this cpp file and its associated header file.
*/

#include "modules.h"

// Registers all modules by associating their ModuleType enum values with their corresponding module classes
void ModuleUtils::RegisterModules()
{
    RegistrationMap.clear();
    FileExtensionMap.clear();
    ConversionOptionsRegistrationMap.clear();

    Register<DMF>();
    Register<MOD>();
}
