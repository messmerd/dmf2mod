/*
    modules.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Part of the dmf2mod core. All changes needed to add support 
    for new module types are done within this header file and 
    its cpp file by the same name.
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
class DMFConversionOptions;
class MODConversionOptions;

// Explicit specialization and setting static variables
REGISTER_MODULE(DMF, DMFConversionOptions, ModuleType::DMF, "dmf")
REGISTER_MODULE(MOD, MODConversionOptions, ModuleType::MOD, "mod")

// Include all supported modules here
#include "dmf.h"
#include "mod.h"
