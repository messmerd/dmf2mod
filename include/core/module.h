/*
 * module.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines an interface for Modules.
 * All module classes must inherit ModuleInterface.
 */

#pragma once

#include "core/module_base.h"
#include "core/factory.h"
#include "core/conversion_options.h"
#include "core/status.h"
#include "core/data.h"
#include "core/generated_data.h"
#include "core/note.h"
#include "core/global_options.h"

#include <string>
#include <memory>

namespace d2m {

// All module classes must derive from this using CRTP
template<class Derived>
class ModuleInterface : public EnableFactory<Derived, ModuleBase>
{
public:

    virtual ~ModuleInterface() = default;

    inline const ModuleData<Derived>& GetData() const { return data_; }
    inline const ModuleGlobalData<Derived>& GetGlobalData() const { return GetData().GlobalData(); }
    inline std::shared_ptr<const GeneratedData<Derived>> GetGeneratedData() const { return generated_data_; }

    const std::string& GetTitle() const final override { return GetGlobalData().title; }
    const std::string& GetAuthor() const final override { return GetGlobalData().author; }

    size_t GenerateData(size_t data_flags = 0) const final override
    {
        // If generated data has already been created using the same data_flags, just return that
        if (generated_data_->IsValid() && generated_data_->GetGenerated().value() == data_flags)
            return generated_data_->GetStatus();

        // Else, need to generate data
        generated_data_->ClearAll();
        const size_t status = GenerateDataImpl(data_flags);
        generated_data_->SetGenerated(data_flags);
        generated_data_->SetStatus(status);
        return status;
    }

protected:

    ModuleInterface() : generated_data_(std::make_shared<GeneratedData<Derived>>()) {}

    inline ModuleData<Derived>& GetData() { return data_; }
    inline ModuleGlobalData<Derived>& GetGlobalData() { return GetData().GlobalData(); }
    inline std::shared_ptr<GeneratedData<Derived>> GetGeneratedDataMut() const { return generated_data_; }

    // data_flags specifies what data was requested to be generated
    virtual size_t GenerateDataImpl(size_t data_flags) const = 0;

private:

    // Song information for a particular module file
    ModuleData<Derived> data_;

    // Information about a module file which must be calculated.
    // Cannot be stored directly because other Modules need to modify its contents without modifying the Module
    const std::shared_ptr<GeneratedData<Derived>> generated_data_;
};

} // namespace d2m
