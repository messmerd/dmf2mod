/*
    options.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines ModuleOption and ModuleOptions which are used
    when working with command-line options.
*/

#include "options.h"

using namespace d2m;

size_t ModuleOptions::Count() const
{
    return m_Options.size();
}

const ModuleOption& ModuleOptions::Item(unsigned index) const
{
    return m_Options.at(index);
}

ModuleOption* ModuleOptions::FindByName(std::string name)
{
    for (auto& option : m_Options)
    {
        if (option.GetName() == name)
        {
            return &option;
        }
    }
    return nullptr;
}

ModuleOption* ModuleOptions::FindByShortName(std::string shortName)
{
    for (auto& option : m_Options)
    {
        if (option.GetShortName() == shortName)
        {
            return &option;
        }
    }
    return nullptr;
}

int ModuleOptions::FindIndexByName(std::string name) const
{
    for (unsigned i = 0; i < m_Options.size(); i++)
    {
        if (m_Options[i].GetName() == name)
        {
            return i;
        }
    }
    return npos;
}

int ModuleOptions::FindIndexByShortName(std::string shortName) const
{
    for (unsigned i = 0; i < m_Options.size(); i++)
    {
        if (m_Options[i].GetShortName() == shortName)
        {
            return i;
        }
    }
    return npos;
}
