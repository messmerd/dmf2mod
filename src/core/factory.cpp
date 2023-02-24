/*
 * factory.cpp
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Implements InitializeImpl for each factory.
 */

#include "core/factory.h"
#include "dmf2mod.h"

using namespace d2m;

using MODOptionEnum = MODConversionOptions::OptionEnum;

template<>
Factory<ConversionOptions>::InitializeImpl::InitializeImpl()
{
    auto mod_options = OptionDefinitionCollection
    {
        /* Type  / Option id                    / Full name    / Short / Default   / Possib. vals          / Description */
        {kOption, MODOptionEnum::kArpeggio,       "arp",         '\0',   false,                              "Allow arpeggio effects"},
        {kOption, MODOptionEnum::kPortamento,     "port",        '\0',   false,                              "Allow portamento up/down effects"},
        {kOption, MODOptionEnum::kPort2Note,      "port2note",   '\0',   false,                              "Allow portamento to note effects"},
        {kOption, MODOptionEnum::kVibrato,        "vib",         '\0',   false,                              "Allow vibrato effects"},
        {kOption, MODOptionEnum::kTempoType,      "tempo",       '\0',   "accuracy", {"accuracy", "compat"}, "Prioritize tempo accuracy or compatibility with effects"},
    };

    Register<ModuleType::kDMF, DMFConversionOptions>();
    Register<ModuleType::kMOD, MODConversionOptions>(std::move(mod_options));
};

template<>
Factory<Module>::InitializeImpl::InitializeImpl()
{
    Register<ModuleType::kDMF, DMF>("Deflemask", "dmf");
    Register<ModuleType::kMOD, MOD>("ProTracker", "mod");
};
