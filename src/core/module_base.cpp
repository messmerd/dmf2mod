/*
    module_base.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See module_base.h.
*/

#include "module_base.h"

// Non-specialized class template data member values:
template<typename T> const ModuleType ModuleStatic<T>::m_Type = ModuleType::NONE;
template<typename T> const std::string ModuleStatic<T>::m_FileExtension = "";
