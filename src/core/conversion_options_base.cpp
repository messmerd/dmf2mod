/*
    conversion_options_base.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See conversion_options_base.h.
*/

#include "conversion_options_base.h"

#include <iostream>

using namespace d2m;

void ConversionOptionsBase::PrintHelp(ModuleType moduleType)
{
    const auto& definitions = GetDefinitions(moduleType);

    std::string name = Module::GetModuleInfo(moduleType)->m_FileExtension;
    if (name.empty())
        return;

    for (auto& c : name)
    {
        c = toupper(c);
    }

    if (!definitions || definitions->Count() == 0)
    {
        std::cout << name << " files have no conversion options.\n";
        return;
    }

    std::cout << name << " Options:\n";

    definitions->PrintHelp();
}
