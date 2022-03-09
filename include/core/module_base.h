/*
    module_base.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares classes that ModuleInterface inherits.
*/

#pragma once

#include "registrar.h"
#include "status.h"

#include <string>
#include <vector>

// CRTP so each class derived from Module can have its own static type variable and static creation
template<typename T>
class ModuleStatic
{
protected:
    friend class Registrar;

    // This class needs to be inherited
    ModuleStatic() = default;
    ModuleStatic(const ModuleStatic&) = default;
    ModuleStatic(ModuleStatic&&) = default;

    static ModuleBase* CreateStatic();

    static ModuleType GetTypeStatic()
    {
        return m_Type;
    }

    static std::string GetFileExtensionStatic();
    
private:
    const static ModuleType m_Type;
    const static std::string m_FileExtension; // Without dot
};


// Base class for all module types (DMF, MOD, XM, etc.)
class ModuleBase
{
public:
    virtual ~ModuleBase() = default;

    /*
     * Create a new module using the ModuleType enum to specify the desired module type
     * If the resulting Module object evaluates to false or Get() == nullptr, the module type is 
     * probably not registered
     */
    static ModulePtr Create(ModuleType type)
    {
        return Registrar::CreateModule(type);
    }

    /*
     * Create a new module of the desired module type
     */
    template <class T, 
        class = typename std::enable_if<std::is_base_of<ModuleInterface<T, typename T::OptionsType>, T>{}>::type>
    static ModulePtr Create()
    {
        return ModulePtr(new T);
    }

    /*
     * Create and import a new module given a filename. Module type is inferred from the file extension.
     * Returns pointer to the module or nullptr if a module registration error occurred.
     */
    static ModulePtr CreateAndImport(const std::string& filename)
    {
        const ModuleType type = Registrar::GetTypeFromFilename(filename);
        ModulePtr m = Module::Create(type);
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
        ModulePtr output = Module::Create(type);
        if (!output)
            return nullptr;

        output->m_Status.Reset(Status::Category::Convert);

        try
        {
            // Perform the conversion
            output->ConvertRaw(this, options);
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
     * Cast a Module pointer to a pointer of a derived type
     */
    template <class T, 
        class = typename std::enable_if<std::is_base_of<ModuleInterface<T, typename T::OptionsType>, T>{}>::type>
    const T* Cast() const
    {
        return reinterpret_cast<const T*>(this);
    }
    
    /*
     * Get a ModuleType enum value representing the type of the module
     */
    virtual ModuleType GetType() const = 0;

    /*
     * Get the file extension of the module (does not include dot)
     */
    virtual std::string GetFileExtension() const = 0;

    /*
     * Get the file extension of the module of the given type (does not include dot)
     */
    static std::string GetFileExtension(ModuleType moduleType)
    {
        return Registrar::GetExtensionFromType(moduleType);
    }

    /*
     * Get the available command-line options for this module
     */
    virtual ModuleOptions GetAvailableOptions() const = 0;

    /*
     * Get the available command-line options for the given module type
     */
    static ModuleOptions GetAvailableOptions(ModuleType moduleType)
    {
        return Registrar::GetAvailableOptions(moduleType);
    }

    /*
     * Get the name of the module
     */
    virtual std::string GetName() const = 0;

protected:
    // Import() and Export() and Convert() are wrappers for these methods:

    virtual void ImportRaw(const std::string& filename) = 0;
    virtual void ExportRaw(const std::string& filename) = 0;
    virtual void ConvertRaw(const Module* input, const ConversionOptionsPtr& options) = 0;

    Status m_Status;
};
