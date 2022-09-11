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

// Forward declares
class ModuleBase;
class ConversionOptionsBase;

// Type aliases to make usage easier
using Module = ModuleBase;
using ModulePtr = std::shared_ptr<Module>;
using ConversionOptions = ConversionOptionsBase;
using ConversionOptionsPtr = std::shared_ptr<ConversionOptions>;

// Specialized Info class for Modules
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

    // TODO: Use this
    enum class ExportState
    {
        Empty, Invalid, Ready
    };

public:

    /*
     * Create and import a new module given a filename. Module type is inferred from the file extension.
     * Returns pointer to the module or nullptr if a module registration error occurred.
     */
    static ModulePtr CreateAndImport(const std::string& filename);

    /*
     * Import the specified module file
     * Returns true upon failure
     */
    bool Import(const std::string& filename);

    /*
     * Export module to the specified file
     * Returns true upon failure
     */
    bool Export(const std::string& filename);

    /*
     * Converts the module to the specified type using the provided conversion options
     */
    ModulePtr Convert(ModuleType type, const ConversionOptionsPtr& options);

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
