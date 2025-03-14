/*
 * module.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines an interface for Modules.
 * All module classes must inherit ModuleInterface.
 */

#pragma once

#include "core/conversion_options.h"
#include "core/data.h"
#include "core/factory.h"
#include "core/generated_data.h"
#include "core/global_options.h"
#include "core/module_base.h"
#include "core/note.h"
#include "core/status.h"

#include <memory>
#include <string>

namespace d2m {

// All module classes must derive from this using CRTP
template<class Derived>
class ModuleInterface : public EnableFactory<Derived, ModuleBase>
{
public:
	virtual ~ModuleInterface() = default;

	auto GetData() const -> const ModuleData<Derived>& { return data_; }
	auto GetGlobalData() const -> const ModuleGlobalData<Derived>& { return GetData().GlobalData(); }
	auto GetGeneratedData() const -> std::shared_ptr<const GeneratedData<Derived>> { return generated_data_; }

	auto GetTitle() const -> std::string_view final { return GetGlobalData().title; }
	auto GetAuthor() const -> std::string_view final { return GetGlobalData().author; }

	auto GenerateData(std::size_t data_flags = 0) const -> std::size_t final
	{
		// If generated data has already been created using the same data_flags, just return that
		if (generated_data_->IsValid() && generated_data_->GetGenerated().value() == data_flags)
		{
			return generated_data_->GetStatus();
		}

		// Else, need to generate data
		generated_data_->ClearAll();
		const std::size_t status = GenerateDataImpl(data_flags);
		generated_data_->SetGenerated(data_flags);
		generated_data_->SetStatus(status);
		return status;
	}

protected:
	ModuleInterface() = default;

	auto GetData() -> ModuleData<Derived>& { return data_; }
	auto GetGlobalData() -> ModuleGlobalData<Derived>& { return GetData().GlobalData(); }
	auto GetGeneratedDataMut() const -> std::shared_ptr<GeneratedData<Derived>> { return generated_data_; }

	// data_flags specifies what data was requested to be generated
	virtual auto GenerateDataImpl(std::size_t data_flags) const -> std::size_t = 0;

private:
	// Song information for a particular module file
	ModuleData<Derived> data_;

	// Information about a module file which must be calculated.
	// Cannot be stored directly because other Modules need to modify its contents without modifying the Module
	const std::shared_ptr<GeneratedData<Derived>> generated_data_ = std::make_shared<GeneratedData<Derived>>();
};

} // namespace d2m
