// Implement initialize methods for each factory here

#include "config.h"

using namespace d2m;

template<>
void Factory<ConversionOptionsBase>::InitializeImpl()
{
    Clear();
    Factory<ConversionOptionsBase>::Register<ModuleType::DMF, DMFConversionOptions>();
    Factory<ConversionOptionsBase>::Register<ModuleType::MOD, MODConversionOptions>();
}

template<>
void Factory<ModuleBase>::InitializeImpl()
{
    Clear();
    Register<ModuleType::DMF, DMF>();
    Register<ModuleType::MOD, MOD>();
}
