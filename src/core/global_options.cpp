#include "global_options.h"

using namespace d2m;
using OptionEnum = GlobalOptions::OptionEnum;

static auto G_OptionDefinitions = CreateOptionDefinitions(
{
    {OptionEnum::Force, "force", 'f', false, "Overwrite output file.", true},
    {OptionEnum::Help, "help", '\0', "", "[module type]", "Display this help message. Provide module type (i.e. mod) for module-specific options.", false},
    {OptionEnum::Silence, "silent", 's', false, "Print nothing to console except errors and/or warnings.", true}
});

OptionCollection GlobalOptions::m_GlobalOptions(G_OptionDefinitions);
