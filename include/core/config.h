
#pragma once

#include "registrar.h" // not needed?

#include "conversion_options.h" // not needed?
#include "module.h" // not needed?

#include "dmf.h"
#include "mod.h"

namespace d2m {

void Initialize();

// ConversionOptionsBase factory
template class Factory<ConversionOptionsBase>; // Explicit instantiation
template<> std::map<ModuleType, std::unique_ptr<const BuilderBase<ConversionOptionsBase>>> Factory<ConversionOptionsBase>::m_Builders{};
template<> std::map<ModuleType, std::unique_ptr<const Info<ConversionOptionsBase>>> Factory<ConversionOptionsBase>::m_Info{};
template<> std::map<std::type_index, TypeEnum> Factory<ConversionOptionsBase>::m_TypeToEnum{};
template<> bool Factory<ConversionOptionsBase>::m_Initialized = false;
//template<> void Factory<ConversionOptionsBase>::InitializeImpl();

// ModuleBase factory
template class Factory<ModuleBase>; // Explicit instantiation
template<> std::map<ModuleType, std::unique_ptr<const BuilderBase<ModuleBase>>> Factory<ModuleBase>::m_Builders{};
template<> std::map<ModuleType, std::unique_ptr<const Info<ModuleBase>>> Factory<ModuleBase>::m_Info{};
template<> std::map<std::type_index, TypeEnum> Factory<ModuleBase>::m_TypeToEnum{};
template<> bool Factory<ModuleBase>::m_Initialized = false;
//template<> void Factory<ModuleBase>::InitializeImpl();

using TopLevelFactory = Factory<ModuleBase>;

} // namespace d2m
