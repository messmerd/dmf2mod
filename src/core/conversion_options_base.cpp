/*
    conversion_options_base.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See conversion_options_base.h.
*/

#include "conversion_options_base.h"

using namespace d2m;

// Non-specialized class template data member values:
template<typename T> const ModuleType ConversionOptionsStatic<T>::m_Type = ModuleType::NONE;
template<typename T> const ModuleOptions ConversionOptionsStatic<T>::m_AvailableOptions = {};

template<typename T> ConversionOptionsBase* ConversionOptionsStatic<T>::CreateStatic()
{
    return nullptr;
}

template<typename T> ModuleOptions ConversionOptionsStatic<T>::GetAvailableOptionsStatic()
{
    return m_AvailableOptions;
}
