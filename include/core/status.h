/*
 * status.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Declares Status and ModuleException, which are used
 * for handling errors and warnings.
 *
 * Defines NotImplementedException.
 */

#pragma once

#include "core/factory.h"

#include <string>
#include <vector>
#include <map>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <memory>

namespace d2m {

// Forward declares
class ModuleException;
class Status;
class NotImplementedException;

// Used whenever an error occurs during import/converting/exporting. Can derive from this class.
class ModuleException : public std::exception
{
public:

    // Common error codes are defined here
    // Module-specific error codes can be implemented using positive values
    enum class ImportError
    {
        kSuccess=0
    };

    enum class ExportError
    {
        kSuccess=0,
        kFileOpen=-1
    };
    
    enum class ConvertError
    {
        kSuccess=0,
        kUnsuccessful=-1, // Applied to the input module
        kInvalidArgument=-2,
        kUnsupportedInputType=-3
    };

    // The type of error
    enum class Category
    {
        kNone,
        kImport,
        kExport,
        kConvert
    };

    const char* what() const throw() override
    {
        return error_message_.c_str();
    }

    std::string str() const
    {
        return error_message_;
    }

    ~ModuleException() = default;
    ModuleException(ModuleException&& other) noexcept = default;
    ModuleException& operator=(ModuleException&& other) = default;

public: // Should this be protected once DMF gets its own DMFException class?

    ModuleException() = default;
    ModuleException(const ModuleException& other) = default;
    ModuleException& operator=(ModuleException& other) = default;

    // Construct using an enum for an error code
    template<class T, std::enable_if_t<std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>, bool> = true>
    ModuleException(Category category, T error_code, const std::string& error_message = "")
        : ModuleException(category, static_cast<int>(error_code), error_message) {}

    // Construct using an integer for an error code
    ModuleException(Category category, int error_code, const std::string& error_message = "")
    {
        error_code_ = error_code;

        std::string category_str;
        switch (category)
        {
            case Category::kNone:
                category_str = "Init: "; break;
            case Category::kImport:
                category_str = "Import: "; break;
            case Category::kExport:
                category_str = "Export: "; break;
            case Category::kConvert:
                category_str = "Convert: "; break;
        }

        if (error_code_ > 0)
        {
            error_message_ = "ERROR: " + category_str + error_message;
        }
        else
        {
            error_message_ = "ERROR: " + category_str + CreateCommonErrorMessage(category, error_code_, error_message);
        }
    }

protected:
    int error_code_;
    std::string error_message_;

private:
    std::string CreateCommonErrorMessage(Category category, int error_code, const std::string& arg);
};


// Provides warning information after module importing/converting/exporting
class Status
{
public:
    // The source of the error/warning
    using Category = ModuleException::Category;

    Status()
    {
        Clear();
        category_ = Category::kNone;
    }

    bool ErrorOccurred() const { return error_.get(); }
    bool WarningsIssued() const { return !warning_messages_.empty(); }
    
    void PrintError(bool use_std_err = true) const;
    void PrintWarnings(bool use_std_err = false) const;

    // Prints error and warnings that occurred during the last action. Returns true if an error occurred.
    bool HandleResults() const;

    void Clear()
    {
        warning_messages_.clear();
        error_.reset();
    }

    void AddError(ModuleException&& error)
    {
        if (!error_)
            error_ = std::make_unique<ModuleException>(std::move(error));
        else
            *error_ = std::move(error);
    }

    void AddWarning(const std::string& warning_message)
    {
        warning_messages_.push_back("WARNING: " + warning_message);
    }

    void Reset(Category action_type)
    {
        Clear();
        category_ = action_type;
    }

private:

    std::unique_ptr<ModuleException> error_;
    std::vector<std::string> warning_messages_;
    Category category_;
};


// NotImplementedException because I took exception to the standard library not implementing it
class NotImplementedException : public std::logic_error
{
public:
    NotImplementedException() : std::logic_error("Function not yet implemented.") {}
};

} // namespace d2m
