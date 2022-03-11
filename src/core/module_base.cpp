/*
    module_base.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See module_base.h.
*/

#include "module_base.h"

using namespace d2m;

// Non-specialized class template data member values:
template<typename T> const ModuleType ModuleStatic<T>::m_Type = ModuleType::NONE;
template<typename T> const std::string ModuleStatic<T>::m_FileExtension = "";

template<typename T> ModuleBase* ModuleStatic<T>::CreateStatic()
{
    return nullptr;
}

template<typename T> std::string ModuleStatic<T>::GetFileExtensionStatic()
{
    return m_FileExtension;
}
