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

// ModuleStatic explicit specialization
template class ModuleStatic<DMF>;
template class ModuleStatic<MOD>;

// ConversionOptionsStatic explicit specialization
template class ConversionOptionsStatic<DMFConversionOptions>;
template class ConversionOptionsStatic<MODConversionOptions>;

// ModuleInterface explicit specialization
template class ModuleInterface<DMF>;
template class ModuleInterface<MOD>;

// Include all supported modules here
#include "dmf.h"
#include "mod.h"
