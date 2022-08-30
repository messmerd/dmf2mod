// Defines the main enum used by the factory

#pragma once

#include <memory>

namespace d2m {

// Add all supported modules to this enum
enum class ModuleType
{
    NONE=0,
    DMF,
    MOD
};

// Forward declares
class ModuleBase;
class ConversionOptionsBase;
template <typename T /*, typename O*/> class ModuleInterface;
template <typename T> class ConversionOptionsInterface;

// Type aliases to make usage easier
using Module = ModuleBase;
using ModulePtr = std::shared_ptr<Module>;
using ConversionOptions = ConversionOptionsBase;
using ConversionOptionsPtr = std::shared_ptr<ConversionOptions>;

} // namespace d2m
