
#pragma once

#include "registrar.h" // not needed?

#include "conversion_options.h" // not needed?
#include "module.h" // not needed?

#include "dmf.h"
#include "mod.h"

namespace d2m {

// ConversionOptionsBase factory
template class Factory<ConversionOptionsBase>; // Explicit instantiation
template<> std::map<ModuleType, BuilderBase const*> Factory<ConversionOptionsBase>::m_Builders{};
template<> std::map<ModuleType, InfoBase*> Factory<ConversionOptionsBase>::m_Info{};

// Top-level factory (For classes that inherit from ModuleBase)
template class Factory<ModuleBase>; // Explicit instantiation
template<> std::map<ModuleType, BuilderBase const*> Factory<ModuleBase>::m_Builders{};
template<> std::map<ModuleType, InfoBase*> Factory<ModuleBase>::m_Info{};

using TopLevelFactory = Factory<ModuleBase>;

} // namespace d2m
