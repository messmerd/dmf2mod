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
protected:

    ModuleInterface() : m_GeneratedData(std::make_shared<ModuleGeneratedData<Derived>>(static_cast<Derived const*>(this))) {}

    friend ModuleGeneratedDataMethods<Derived>;

public:

    virtual ~ModuleInterface() = default;

    inline const ModuleData<Derived>& GetData() const { return m_Data; }
    inline const ModuleGlobalData<Derived>& GetGlobalData() const { return GetData().GlobalData(); }
    inline const std::shared_ptr<ModuleGeneratedData<Derived>>& GetGeneratedData() const { return m_GeneratedData; }

    std::string GetTitle() const override { return GetGlobalData().title; }
    std::string GetAuthor() const override { return GetGlobalData().author; }

protected:

    inline ModuleData<Derived>& GetData() { return m_Data; }
    inline ModuleGlobalData<Derived>& GetGlobalData() { return GetData().GlobalData(); }
    inline std::shared_ptr<ModuleGeneratedData<Derived>>& GetGeneratedData() { return m_GeneratedData; }

private:

    // Song information for a particular module file
    ModuleData<Derived> m_Data;

    // Information about a module file which must be calculated.
    // Cannot be stored directly because other Modules need to modify its contents without modifying the Module
    std::shared_ptr<ModuleGeneratedData<Derived>> m_GeneratedData;
};

} // namespace d2m
