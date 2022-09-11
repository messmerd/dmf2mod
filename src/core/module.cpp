/*
    module.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    See module.h
*/

#include "module.h"
#include "utils/utils.h"

using namespace d2m;

ModulePtr ModuleBase::CreateAndImport(const std::string& filename)
{
    const ModuleType type = Utils::GetTypeFromFilename(filename);
    if (type == ModuleType::NONE)
        return nullptr;

    ModulePtr m = Factory<ModuleBase>::Create(type);
    if (!m)
        return nullptr;

    try
    {
        m->Import(filename);
    }
    catch (ModuleException& e)
    {
        m->m_Status.AddError(std::move(e));
    }

    return m;
}

bool ModuleBase::Import(const std::string& filename)
{
    m_Status.Reset(Status::Category::Import);
    try
    {
        ImportRaw(filename);
        return false;
    }
    catch (ModuleException& e)
    {
        m_Status.AddError(std::move(e));
    }

    return true;
}

bool ModuleBase::Export(const std::string& filename)
{
    m_Status.Reset(Status::Category::Export);
    try
    {
        ExportRaw(filename);
        return false;
    }
    catch (ModuleException& e)
    {
        m_Status.AddError(std::move(e));
    }

    return true;
}

ModulePtr ModuleBase::Convert(ModuleType type, const ConversionOptionsPtr& options)
{
    ModuleBase* input = this; // For clarity
    input->m_Status.Reset(Status::Category::Convert);

    // Don't convert if the types are the same
    if (type == input->GetType())
        return nullptr;

    // Create new module object
    ModulePtr output = Factory<ModuleBase>::Create(type);
    if (!output)
        return nullptr;

    output->m_Status.Reset(Status::Category::Convert);
    output->m_Options = options;

    try
    {
        // Perform the conversion
        output->ConvertRaw(shared_from_this());
    }
    catch (ModuleException& e)
    {
        output->m_Status.AddError(std::move(e));
        input->m_Status.AddError(ModuleException(ModuleException::Category::Convert, ModuleException::ConvertError::Unsuccessful));
    }

    return output;
}
