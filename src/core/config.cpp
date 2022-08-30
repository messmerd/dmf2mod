// Implement initialize methods for each factory here

#include "config.h"

using namespace d2m;

template<>
void Factory<ConversionOptions>::InitializeImpl()
{
    Clear();
    Factory<ConversionOptions>::Register<ModuleType::DMF, DMFConversionOptions>();
    Factory<ConversionOptions>::Register<ModuleType::MOD, MODConversionOptions>();
}

template<>
void Factory<Module>::InitializeImpl()
{
    Clear();

    Info<Module> dmf { ModuleType::DMF, "Deflemask", "dmf" };
    Register<DMF>(dmf);

    Info<Module> mod { ModuleType::MOD, "ProTracker", "mod" };
    Register<MOD>(mod);
}
