/*
    global_options.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See global_options.h
*/

#include "global_options.h"

using namespace d2m;
using OptionEnum = GlobalOptions::OptionEnum;

static OptionDefinitionCollection G_OptionDefinitions =
{
    {OPTION, OptionEnum::Force, "force", 'f', false, "Overwrite output file."},
    {COMMAND, OptionEnum::Help, "help", '\0', "", "[module type]", "Display this help message. Provide module type (i.e. mod) for module-specific options."},
    {OPTION, OptionEnum::Verbose, "verbose", '\0', false, "Print debug info to console in addition to errors and/or warnings."},
    {COMMAND, OptionEnum::Version, "version", 'v', false, "Display the dmf2mod version."}
};

OptionCollection GlobalOptions::m_GlobalOptions(&G_OptionDefinitions);
