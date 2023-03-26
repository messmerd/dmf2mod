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

    inline auto GetData() const -> const ModuleData<Derived>& { return data_; }
    inline auto GetGlobalData() const -> const ModuleGlobalData<Derived>& { return GetData().GlobalData(); }
    inline auto GetGeneratedData() const -> std::shared_ptr<const GeneratedData<Derived>> { return generated_data_; }

    [[nodiscard]] auto GetTitle() const -> const std::string& final { return GetGlobalData().title; }
    [[nodiscard]] auto GetAuthor() const -> const std::string& final { return GetGlobalData().author; }

    [[nodiscard]] auto GenerateData(size_t data_flags = 0) const -> size_t final
    {
        // If generated data has already been created using the same data_flags, just return that
        if (generated_data_->IsValid() && generated_data_->GetGenerated().value() == data_flags)
        {
            return generated_data_->GetStatus();
        }

        // Else, need to generate data
        generated_data_->ClearAll();
        const size_t status = GenerateDataImpl(data_flags);
        generated_data_->SetGenerated(data_flags);
        generated_data_->SetStatus(status);
        return status;
    }

protected:

    ModuleInterface() : generated_data_(std::make_shared<GeneratedData<Derived>>()) {}

    inline auto GetData() -> ModuleData<Derived>& { return data_; }
    inline auto GetGlobalData() -> ModuleGlobalData<Derived>& { return GetData().GlobalData(); }
    inline auto GetGeneratedDataMut() const -> std::shared_ptr<GeneratedData<Derived>> { return generated_data_; }

    // data_flags specifies what data was requested to be generated
    [[nodiscard]] virtual auto GenerateDataImpl(size_t data_flags) const -> size_t = 0;

private:

    // Song information for a particular module file
    ModuleData<Derived> data_;

    // Information about a module file which must be calculated.
    // Cannot be stored directly because other Modules need to modify its contents without modifying the Module
    const std::shared_ptr<GeneratedData<Derived>> generated_data_;
};

} // namespace d2m
