/*
    All changes needed to add support for new module types
    are done within this header file and its associated cpp file.
*/

#pragma once

#include "core.h"

// Add all supported modules to this enum
enum class ModuleType
{
    NONE=0,
    DMF,
    MOD
};

// Forward defines
class DMF;
class MOD;

// Include all supported modules here
#include "dmf.h"
#include "mod.h"
