/*
    conversion_options_base.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See conversion_options_base.h.
*/

#include "conversion_options_base.h"

#include <iostream>

using namespace d2m;

// Non-specialized class template data member values:
template<typename T> const ModuleType ConversionOptionsStatic<T>::m_Type = ModuleType::NONE;
template<typename T> const std::shared_ptr<OptionDefinitionCollection> ConversionOptionsStatic<T>::m_OptionDefinitions = nullptr;

template<typename T>
ConversionOptionsBase* ConversionOptionsStatic<T>::CreateStatic()
{
    return nullptr;
}

template<typename T>
const std::shared_ptr<OptionDefinitionCollection>& ConversionOptionsStatic<T>::GetDefinitionsStatic()
{
    return m_OptionDefinitions;
}

void ConversionOptionsBase::PrintHelp(ModuleType moduleType)
{
    const auto& definitions = Registrar::GetOptionDefinitions(moduleType);

    std::string name = Registrar::GetExtensionFromType(moduleType);
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
