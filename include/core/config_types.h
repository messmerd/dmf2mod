/*
    config_types.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines the main enum used by the factory
*/

#pragma once

namespace d2m {

// Add all supported modules to this enum
enum class ModuleType
{
    NONE=0,
    DMF,
    MOD
};

// Factory implementation expects a "TypeEnum"
using TypeEnum = ModuleType;
inline constexpr TypeEnum TypeInvalid = ModuleType::NONE;

} // namespace d2m
