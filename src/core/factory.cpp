/*
 * factory.cpp
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Implements InitializeImpl for each factory.
 */

#include "core/factory.h"

#include "dmf2mod.h"

namespace d2m {

template<>
Factory<ConversionOptions>::InitializeImpl::InitializeImpl()
{
	using MODOptionEnum = MODConversionOptions::OptionEnum;
	auto mod_options = OptionDefinitionCollection{
		/* Type  / Option id                    / Full name    / Short / Default   / Possib. vals          / Description */
		{kOption, MODOptionEnum::kArpeggio,       "arp",         '\0',   false,                              "Allow arpeggio effects"},
		{kOption, MODOptionEnum::kPortamento,     "port",        '\0',   false,                              "Allow portamento up/down effects"},
		{kOption, MODOptionEnum::kPort2Note,      "port2note",   '\0',   false,                              "Allow portamento to note effects"},
		{kOption, MODOptionEnum::kVibrato,        "vib",         '\0',   false,                              "Allow vibrato effects"},
		{kOption, MODOptionEnum::kTempoType,      "tempo",       '\0',   "accuracy", {"accuracy", "compat"}, "Prioritize tempo accuracy or compatibility with effects"},
	};

	Register<ModuleType::kDMF, DMFConversionOptions>();
	Register<ModuleType::kMOD, MODConversionOptions>(std::move(mod_options));

#ifndef NDEBUG
	using DebugOptionEnum = DebugConversionOptions::OptionEnum;
	auto debug_options = OptionDefinitionCollection{
		/* Type  / Option id                    / Full name    / Short / Default   / Possib. vals          / Description */
		{kCommand, DebugOptionEnum::kDump,         "dump",       'd',    false,                              "Dump generated data"},
		{kOption,  DebugOptionEnum::kAppend,       "append",     'a',    true,                               "Append results to log file or overwrite"},
		{kOption,  DebugOptionEnum::kGenDataFlags, "gen",        'g',    0,                                  "Flags to use when generating data"}
	};
	Register<ModuleType::kDebug, DebugConversionOptions>(std::move(debug_options));
#endif
};

template<>
Factory<Module>::InitializeImpl::InitializeImpl()
{
	Register<ModuleType::kDMF, DMF>("Deflemask", "dmf", "dmf");
	Register<ModuleType::kMOD, MOD>("ProTracker", "mod", "mod");

#ifndef NDEBUG
	Register<ModuleType::kDebug, Debug>("Debug", "debug", "log");
#endif
};

} // namespace d2m
