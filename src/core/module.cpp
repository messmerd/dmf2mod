/*
 * module.cpp
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * See module.h
 */

#include "core/module.h"
#include "utils/utils.h"

#include <cassert>

using namespace d2m;

auto ModuleBase::Import(const std::string& filename) -> bool
{
    status_.Reset(Status::Category::kImport);
    try
    {
        ImportImpl(filename);
        return false;
    }
    catch (ModuleException& e)
    {
        status_.AddError(std::move(e));
    }

    return true;
}

auto ModuleBase::Export(const std::string& filename) -> bool
{
    status_.Reset(Status::Category::kExport);
    try
    {
        ExportImpl(filename);
        return false;
    }
    catch (ModuleException& e)
    {
        status_.AddError(std::move(e));
    }

    return true;
}

auto ModuleBase::Convert(ModuleType type, const ConversionOptionsPtr& options) -> ModulePtr
{
    ModuleBase* input = this; // For clarity
    input->status_.Reset(Status::Category::kConvert);

    // Don't convert if the types are the same
    if (type == input->GetType()) { return nullptr; }

    // Create new module object
    ModulePtr output = Factory<ModuleBase>::Create(type);
    if (!output) { return nullptr; }

    output->status_.Reset(Status::Category::kConvert);
    output->options_ = options;

    try
    {
        // Perform the conversion
        assert(shared_from_this() != nullptr);
        output->ConvertImpl(shared_from_this());
    }
    catch (ModuleException& e)
    {
        output->status_.AddError(std::move(e));
        input->status_.AddError(ModuleException(ModuleException::Category::kConvert, ModuleException::ConvertError::kUnsuccessful));
    }

    return output;
}
