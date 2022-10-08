/*
    status.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares Status and ModuleException, which are used
    for handling errors and warnings.

    Defines NotImplementedException.
*/

#pragma once

#include "factory.h"

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
        Success=0
    };

    enum class ExportError
    {
        Success=0,
        FileOpen=-1
    };
    
    enum class ConvertError
    {
        Success=0,
        Unsuccessful=-1, // Applied to the input module
        InvalidArgument=-2,
        UnsupportedInputType=-3
    };

    // The type of error
    enum class Category
    {
        None,
        Import,
        Export,
        Convert
    };

    const char* what() const throw() override
    {
        return m_ErrorMessage.c_str();
    }

    std::string str() const
    {
        return m_ErrorMessage;
    }

    ~ModuleException() = default;
    ModuleException(ModuleException&& other) noexcept = default;
    ModuleException& operator=(ModuleException&& other) = default;

public: // Should this be protected once DMF gets its own DMFException class?

    ModuleException() = default;
    ModuleException(const ModuleException& other) = default;
    ModuleException& operator=(ModuleException& other) = default;

    // Construct using an enum for an error code
    template <class T, class = std::enable_if_t<std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>>>
    ModuleException(Category category, T errorCode, const std::string& errorMessage = "")
        : ModuleException(category, static_cast<int>(errorCode), errorMessage) {}

    // Construct using an integer for an error code
    ModuleException(Category category, int errorCode, const std::string& errorMessage = "")
    {
        m_ErrorCode = errorCode;

        std::string categoryString;
        switch (category)
        {
            case Category::None:
                categoryString = "Init: "; break;
            case Category::Import:
                categoryString = "Import: "; break;
            case Category::Export:
                categoryString = "Export: "; break;
            case Category::Convert:
                categoryString = "Convert: "; break;
        }

        if (m_ErrorCode > 0)
        {
            m_ErrorMessage = "ERROR: " + categoryString + errorMessage;
        }
        else
        {
            m_ErrorMessage = "ERROR: " + categoryString + CreateCommonErrorMessage(category, m_ErrorCode, errorMessage);
        }
    }

protected:
    int m_ErrorCode;
    std::string m_ErrorMessage;

private:
    std::string CreateCommonErrorMessage(Category category, int errorCode, const std::string& arg);
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
        m_Category = Category::None;
    }

    bool ErrorOccurred() const { return m_Error.get(); }
    bool WarningsIssued() const { return !m_WarningMessages.empty(); }
    
    void PrintError(bool useStdErr = true) const;
    void PrintWarnings(bool useStdErr = false) const;

    // Prints error and warnings that occurred during the last action. Returns true if an error occurred.
    bool HandleResults() const;

    void Clear()
    {
        m_WarningMessages.clear();
        m_Error.reset();
    }

    void AddError(ModuleException&& error)
    {
        if (!m_Error)
            m_Error = std::make_unique<ModuleException>(std::move(error));
        else
            *m_Error = std::move(error);
    }

    void AddWarning(const std::string& warningMessage)
    {
        m_WarningMessages.push_back("WARNING: " + warningMessage);
    }

    void Reset(Category actionType)
    {
        Clear();
        m_Category = actionType;
    }

private:

    std::unique_ptr<ModuleException> m_Error;
    std::vector<std::string> m_WarningMessages;
    Category m_Category;
};


// NotImplementedException because I took exception to the standard library not implementing it
class NotImplementedException : public std::logic_error
{
public:
    NotImplementedException() : std::logic_error("Function not yet implemented.") {}
};

} // namespace d2m
