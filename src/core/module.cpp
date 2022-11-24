/*
    module.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See module.h
*/

#include "module.h"
#include "utils/utils.h"

#include <cassert>

using namespace d2m;

bool ModuleBase::Import(const std::string& filename)
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

bool ModuleBase::Export(const std::string& filename)
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

ModulePtr ModuleBase::Convert(ModuleType type, const ConversionOptionsPtr& options)
{
    ModuleBase* input = this; // For clarity
    input->status_.Reset(Status::Category::kConvert);

    // Don't convert if the types are the same
    if (type == input->GetType())
        return nullptr;

    // Create new module object
    ModulePtr output = Factory<ModuleBase>::Create(type);
    if (!output)
        return nullptr;

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
