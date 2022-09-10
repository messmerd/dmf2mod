/*
    conversion_options.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See conversion_options.h.
*/

#include "conversion_options.h"

#include <iostream>

using namespace d2m;

void ConversionOptionsBase::PrintHelp(ModuleType moduleType)
{
    const auto& definitions = Factory<ConversionOptions>::GetInfo(moduleType)->optionDefinitions;

    std::string name = Factory<Module>::GetInfo(moduleType)->fileExtension;
    if (name.empty())
        return;

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
