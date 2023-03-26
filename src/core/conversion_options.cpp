/*
 * conversion_options.cpp
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * See conversion_options.h
 */

#include "core/conversion_options.h"
#include "core/module_base.h"

#include <iostream>

using namespace d2m;

void ConversionOptionsBase::PrintHelp(ModuleType module_type)
{
    const auto& definitions = Factory<ConversionOptionsBase>::GetInfo(module_type)->option_definitions;

    std::string name = Factory<Module>::GetInfo(module_type)->file_extension;
    if (name.empty()) { return; }

    for (auto& c : name)
    {
        c = toupper(c);
    }

    if (definitions.Count() == 0)
    {
        std::cout << name << " files have no conversion options.\n";
        return;
    }

    std::cout << name << " Options:\n";

    definitions.PrintHelp();
}
