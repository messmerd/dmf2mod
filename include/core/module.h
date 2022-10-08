/*
    module.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines an interface for Modules.
    All module classes must inherit ModuleInterface.
*/

#pragma once

#include "module_base.h"
#include "factory.h"
#include "conversion_options.h"
#include "status.h"
#include "data.h"
#include "generated_data.h"
#include "note.h"
#include "global_options.h"

#include <string>
#include <memory>

namespace d2m {

// All module classes must derive from this using CRTP
template<class Derived>
class ModuleInterface : public EnableFactory<Derived, ModuleBase>
{
public:

    virtual ~ModuleInterface() = default;

    inline const ModuleData<Derived>& GetData() const { return m_data; }
    inline const ModuleGlobalData<Derived>& GetGlobalData() const { return GetData().GlobalData(); }
    inline const std::shared_ptr<GeneratedData<Derived>>& GetGeneratedData() const { return m_generated_data; }

    const std::string& GetTitle() const override { return GetGlobalData().title; }
    const std::string& GetAuthor() const override { return GetGlobalData().author; }

protected:

    ModuleInterface() : m_generated_data(std::make_shared<GeneratedData<Derived>>()) {}

    inline ModuleData<Derived>& GetData() { return m_data; }
    inline ModuleGlobalData<Derived>& GetGlobalData() { return GetData().GlobalData(); }
    inline std::shared_ptr<GeneratedData<Derived>>& GetGeneratedData() { return m_generated_data; }

private:

    // Song information for a particular module file
    ModuleData<Derived> m_data;

    // Information about a module file which must be calculated.
    // Cannot be stored directly because other Modules need to modify its contents without modifying the Module
    std::shared_ptr<GeneratedData<Derived>> m_generated_data;
};

} // namespace d2m
