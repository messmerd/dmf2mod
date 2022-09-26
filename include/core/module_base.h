/*
    module.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines the base class for Modules.
*/

#pragma once

#include "factory.h"
#include "conversion_options.h"
#include "status.h"

#include <string>
#include <memory>

namespace d2m {

// Forward declares
class ModuleBase;
class ConversionOptionsBase;
template<typename> class ModuleGeneratedData;

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

    ModuleBase() = default;

    // TODO: Use this
    enum class ExportState
    {
        Empty, Invalid, Ready
    };

public:

    virtual ~ModuleBase() = default;

    /*
     * Cast ModulePtr to std::shared_ptr<T> where T is the derived Module class
     */
    template<class T, std::enable_if_t<std::is_base_of_v<ModuleBase, T>, bool> = true>
    std::shared_ptr<T> Cast() const
    {
        return std::static_pointer_cast<T>(shared_from_this());
    }

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

protected:
    // Import() and Export() and Convert() are wrappers for these methods, which must be implemented by a module class:

    virtual void ImportImpl(const std::string& filename) = 0;
    virtual void ExportImpl(const std::string& filename) = 0;
    virtual void ConvertImpl(const ModulePtr& input) = 0;

    // Allow ModuleGeneratedData to call GenerateDataImpl
    template<typename> friend class ModuleGeneratedData;

    // dataFlags specifies what data was requested to be generated
    virtual size_t GenerateDataImpl(size_t dataFlags) const = 0;

    ConversionOptionsPtr GetOptions() const { return m_Options; }

    Status m_Status;

private:

    ConversionOptionsPtr m_Options;
};

} // namespace d2m
