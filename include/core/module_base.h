/*
 * module_base.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines the base class for Modules.
 */

#pragma once

#include "core/factory.h"
#include "core/conversion_options.h"
#include "core/status.h"

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
    std::string friendly_name;
    std::string file_extension;
};

// Base class for all module types (DMF, MOD, XM, etc.)
class ModuleBase : public EnableReflection<ModuleBase>, public std::enable_shared_from_this<ModuleBase>
{
protected:

    ModuleBase() = default;

    // TODO: Use this
    enum class ExportState
    {
        kEmpty, kInvalid, kReady
    };

public:

    virtual ~ModuleBase() = default;

    /*
     * Cast ModulePtr to std::shared_ptr<T> where T is the derived Module class
     */
    template<class T, std::enable_if_t<std::is_base_of_v<ModuleBase, T>, bool> = true>
    [[nodiscard]] auto Cast() const -> std::shared_ptr<T>
    {
        return std::static_pointer_cast<T>(shared_from_this());
    }

    /*
     * Import the specified module file
     * Returns true upon failure
     */
    auto Import(const std::string& filename) -> bool;

    /*
     * Export module to the specified file
     * Returns true upon failure
     */
    auto Export(const std::string& filename) -> bool;

    /*
     * Converts the module to the specified type using the provided conversion options
     */
    auto Convert(ModuleType type, const ConversionOptionsPtr& options) -> ModulePtr;

    /*
     * Generates the generated data using optional data flags
     */
    [[nodiscard]] virtual auto GenerateData(size_t data_flags = 0) const -> size_t = 0;

    /*
     * Gets the Status object for the last import/export/convert
     */
    [[nodiscard]] auto GetStatus() const -> const Status& { return status_; }

    /*
     * Convenience wrapper for GetStatus().HandleResults()
     */
    auto HandleResults() const -> bool { return status_.HandleResults(); }

    /*
     * Get the title of the module
     */
    [[nodiscard]] virtual auto GetTitle() const -> const std::string& = 0;

    /*
     * Get the author of the module
     */
    [[nodiscard]] virtual auto GetAuthor() const -> const std::string& = 0;

protected:
    // Import() and Export() and Convert() are wrappers for these methods, which must be implemented by a module class:

    virtual void ImportImpl(const std::string& filename) = 0;
    virtual void ExportImpl(const std::string& filename) = 0;
    virtual void ConvertImpl(const ModulePtr& input) = 0;

    [[nodiscard]] auto GetOptions() const -> ConversionOptionsPtr { return options_; }

    Status status_;

private:

    ConversionOptionsPtr options_;
};

} // namespace d2m
