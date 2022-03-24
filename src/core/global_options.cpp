#include "global_options.h"

using namespace d2m;
using OptionEnum = GlobalOptions::OptionEnum;

static auto G_OptionDefinitions = CreateOptionDefinitions(
{
    {OPTION, OptionEnum::Force, "force", 'f', false, "Overwrite output file."},
    {COMMAND, OptionEnum::Help, "help", '\0', "", "[module type]", "Display this help message. Provide module type (i.e. mod) for module-specific options."},
    {OPTION, OptionEnum::Verbose, "verbose", '\0', false, "Print debug info to console in addition to errors and/or warnings."}
});

OptionCollection GlobalOptions::m_GlobalOptions(G_OptionDefinitions);
