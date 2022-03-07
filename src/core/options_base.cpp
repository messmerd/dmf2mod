/*
    options_base.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See options_base.h.
*/

#include "options_base.h"

// Non-specialized class template data member values:
template<typename T> const ModuleType ConversionOptionsStatic<T>::m_Type = ModuleType::NONE;
template<typename T> const std::vector<std::string> ConversionOptionsStatic<T>::m_AvailableOptions = {};
