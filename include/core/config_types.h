/*
 * config_types.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines the main enum used by the factory
 */

#pragma once

namespace d2m {

// Add all supported modules to this enum
enum class ModuleType
{
	kNone=0,
	kDMF,
	kMOD,
#ifndef NDEBUG
	kDebug,
#endif
};

} // namespace d2m
