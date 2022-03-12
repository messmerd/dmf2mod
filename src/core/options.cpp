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
    return m_OptionsMap.size();
}

ModuleOption* ModuleOptions::FindById(int id)
{
    if (m_OptionsMap.count(id) == 0)
        return nullptr;
    return &m_OptionsMap[id];
}

ModuleOption* ModuleOptions::FindByName(std::string name)
{
    for (auto& mapPair : m_OptionsMap)
    {
        auto& option = mapPair.second;
        if (option.GetName() == name)
        {
            return &option;
        }
    }
    return nullptr;
}

ModuleOption* ModuleOptions::FindByShortName(std::string shortName)
{
    for (auto& mapPair : m_OptionsMap)
    {
        auto& option = mapPair.second;
        if (option.GetShortName() == shortName)
        {
            return &option;
        }
    }
    return nullptr;
}

int ModuleOptions::FindIdByName(std::string name) const
{
    for (auto& mapPair : m_OptionsMap)
    {
        auto& option = mapPair.second;
        if (option.GetName() == name)
        {
            return option.GetId();
        }
    }
    return npos;
}

int ModuleOptions::FindIdByShortName(std::string shortName) const
{
    for (auto& mapPair : m_OptionsMap)
    {
        auto& option = mapPair.second;
        if (option.GetShortName() == shortName)
        {
            return option.GetId();
        }
    }
    return npos;
}
