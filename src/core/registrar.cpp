/*
    registrar.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines the Registrar class, which registers all the available 
    modules at runtime, provides factory methods for creating Module and 
    ConversionOptions objects, and provides helper methods for retrieving 
    info about registered modules.

    All changes needed to add support for new module types are done within 
    this cpp file, its header file by the same name, and all_modules.h.

    Currently, modules supported by the Module factory are statically linked 
    with the dmf2mod core and dynamically registered through the 
    Registrar. With some minor changes, module libraries could be dynamically 
    linked as well. If I did that, I would no longer use the ModuleType enum and 
    instead Registrar would assign an integer ID to each loaded module library 
    during registration in this file.
    
    However, dynamic loading of module libraries seems a bit overkill and 
    the current setup fulfills the goal of allowing new module support to be 
    added with minimal changes to the dmf2mod core.
*/

#include "registrar.h"

using namespace d2m;

// Initialize for the primary template
template<class Base> std::map<ModuleType, std::unique_ptr<const BuilderBase<Base>>> Factory<Base>::m_Builders{};
template<class Base> std::map<ModuleType, std::unique_ptr<const Info<Base>>> Factory<Base>::m_Info{};
template<class Base> std::map<std::type_index, ModuleType> Factory<Base>::m_TypeToEnum{};
template<class Base> bool Factory<Base>::m_Initialized = false;
