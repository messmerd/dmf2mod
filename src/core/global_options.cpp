/*
    global_options.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See global_options.h
*/

#include "global_options.h"

using namespace d2m;
using OptionEnum = GlobalOptions::OptionEnum;

static const OptionDefinitionCollection kOptionDefinitions =
{
    {kOption, OptionEnum::kForce, "force", 'f', false, "Overwrite output file."},
    {kCommand, OptionEnum::kHelp, "help", '\0', "", "[module type]", "Display this help message. Provide module type (i.e. mod) for module-specific options."},
    {kOption, OptionEnum::kVerbose, "verbose", '\0', false, "Print debug info to console in addition to errors and/or warnings."},
    {kCommand, OptionEnum::kVersion, "version", 'v', false, "Display the dmf2mod version."}
};

OptionCollection GlobalOptions::global_options_(&kOptionDefinitions);
