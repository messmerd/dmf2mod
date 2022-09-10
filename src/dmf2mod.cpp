/*
    dmf2mod.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Implements initialize methods for each factory.
*/

#include "dmf2mod.h"

using namespace d2m;

using MODOptionEnum = MODConversionOptions::OptionEnum;
static auto MODOptions = OptionDefinitionCollection
{
    /* Type  / Option id                    / Full name    / Short / Default   / Possib. vals          / Description */
    {OPTION, MODOptionEnum::AmigaFilter,    "amiga",       '\0',   false,                              "Enables the Amiga filter"},
    {OPTION, MODOptionEnum::Arpeggio,       "arp",         '\0',   false,                              "Allow arpeggio effects"},
    {OPTION, MODOptionEnum::Portamento,     "port",        '\0',   false,                              "Allow portamento up/down effects"},
    {OPTION, MODOptionEnum::Port2Note,      "port2note",   '\0',   false,                              "Allow portamento to note effects"},
    {OPTION, MODOptionEnum::Vibrato,        "vib",         '\0',   false,                              "Allow vibrato effects"},
    {OPTION, MODOptionEnum::TempoType,      "tempo",       '\0',   "accuracy", {"accuracy", "compat"}, "Prioritize tempo accuracy or compatibility with effects"},
};

template<>
void Factory<ConversionOptions>::InitializeImpl()
{
    Clear();

    Register<ModuleType::DMF, DMFConversionOptions>();
    Register<MODConversionOptions>(ModuleType::MOD, std::move(MODOptions));
}

template<>
void Factory<Module>::InitializeImpl()
{
    Clear();

    Register<DMF>(ModuleType::DMF, "Deflemask", "dmf");
    Register<MOD>(ModuleType::MOD, "ProTracker", "mod");
}

void d2m::Initialize()
{
    Factory<Module>::Initialize();
    Factory<ConversionOptions>::Initialize();
}
