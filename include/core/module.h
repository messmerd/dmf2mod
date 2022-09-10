/*
    module.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines an interface for modules.
    All module classes must inherit ModuleInterface.
*/

#pragma once

#include "factory.h"
#include "conversion_options.h"
#include "status.h"
#include "data.h"
#include "note.h"
#include "global_options.h"

#include <string>
#include <memory>

namespace d2m {

// Specialized Info class for Modules
class ModuleBase;
template<>
struct Info<ModuleBase> : public InfoBase
{
    std::string friendlyName{};
    std::string fileExtension{};
};


// Base class for all module types (DMF, MOD, XM, etc.)
class ModuleBase : public EnableReflection<ModuleBase>, public std::enable_shared_from_this<ModuleBase>
{
protected:

    friend struct BuilderBase<ModuleBase>;

    ModuleBase() = default;
    virtual ~ModuleBase() = default;

    enum class ExportState
    {
        Empty, Invalid, Ready
    };

public:

    /*
     * Create and import a new module given a filename. Module type is inferred from the file extension.
     * Returns pointer to the module or nullptr if a module registration error occurred.
     */
    static ModulePtr CreateAndImport(const std::string& filename)
    {
        const ModuleType type = ModuleType::NONE; // TODO: Registrar::GetTypeFromFilename(filename);
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

    /*
     * Import the specified module file
     * Returns true upon failure
     */
    bool Import(const std::string& filename)
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

    /*
     * Export module to the specified file
     * Returns true upon failure
     */
    bool Export(const std::string& filename)
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

    /*
     * Converts the module to the specified type using the provided conversion options
     */
    ModulePtr Convert(ModuleType type, const ConversionOptionsPtr& options)
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

    /*
     * Gets the Status object for the last import/export/convert
     */
    const Status& GetStatus() const { return m_Status; }

    /*
     * Convenience wrapper for GetStatus().HandleResults()
     */
    bool HandleResults() const { return m_Status.HandleResults(); }

    /*
     * Get the title of the module
     */
    virtual std::string GetTitle() const = 0;

    /*
     * Get the author of the module
     */
    virtual std::string GetAuthor() const = 0;

    ////////////////////////////////////////////////////////
    // Methods for getting static info about module type  //
    ////////////////////////////////////////////////////////

protected:
    // Import() and Export() and Convert() are wrappers for these methods, which must be implemented by a module class:

    virtual void ImportRaw(const std::string& filename) = 0;
    virtual void ExportRaw(const std::string& filename) = 0;
    virtual void ConvertRaw(const ModulePtr& input) = 0;

    template<class T, std::enable_if_t<std::is_base_of_v<ModuleBase, T>, bool> = true>
    std::shared_ptr<T> Cast() const
    {
        return std::static_pointer_cast<T>(shared_from_this());
    }

    ConversionOptionsPtr GetOptions() const { return m_Options; }

    Status m_Status;

private:

    ConversionOptionsPtr m_Options;
};


// All module classes must derive from this using CRTP
template <class Derived>
class ModuleInterface : public EnableFactory<Derived, ModuleBase>
{
public:

    inline const ModuleData<Derived>& GetData() const { return m_Data; }
    inline const ModuleGlobalData<Derived>& GetGlobalData() const { return GetData().GlobalData(); }

    std::string GetTitle() const override { return GetGlobalData().title; }
    std::string GetAuthor() const override { return GetGlobalData().author; }

protected:

    inline ModuleData<Derived>& GetData() { return m_Data; }
    inline ModuleGlobalData<Derived>& GetGlobalData() { return GetData().GlobalData(); }

private:

    //static const ModuleInfo m_Info;     // Info inherent to every module of type Derived
    ModuleData<Derived> m_Data;         // Song information for a particular module file
};

} // namespace d2m
