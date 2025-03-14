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

#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

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
		kSuccess = 0
	};

	enum class ExportError
	{
		kSuccess  = 0,
		kFileOpen = -1
	};

	enum class ConvertError
	{
		kSuccess = 0,
		kUnsuccessful         = -1, // Applied to the input module
		kInvalidArgument      = -2,
		kUnsupportedInputType = -3
	};

	// The type of error
	enum class Category
	{
		kNone,
		kImport,
		kExport,
		kConvert
	};

	auto what() const noexcept -> const char* override
	{
		return error_message_.c_str();
	}

	auto str() const -> std::string_view
	{
		return error_message_;
	}

	~ModuleException() override = default;
	ModuleException(ModuleException&&) noexcept = default;
	auto operator=(ModuleException&&) -> ModuleException& = default;

public: // Should this be protected once DMF gets its own DMFException class?

	ModuleException() = default;
	ModuleException(const ModuleException&) = default;
	auto operator=(ModuleException&) -> ModuleException& = default;

	// Construct using an enum for an error code
	template<class T, std::enable_if_t<std::is_enum_v<T> && std::is_convertible_v<std::underlying_type_t<T>, int>, bool> = true>
	ModuleException(Category category, T error_code, std::string_view error_message = "")
		: ModuleException{category, static_cast<int>(error_code), error_message} {}

	// Construct using an integer for an error code
	ModuleException(Category category, int error_code, std::string_view error_message = "")
	{
		error_code_ = error_code;

		std::string_view category_str;
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
			error_message_ = "ERROR: ";
			error_message_ += category_str;
			error_message_ += error_message;
		}
		else
		{
			error_message_ = "ERROR: ";
			error_message_ += category_str;
			error_message_ += CreateCommonErrorMessage(category, error_code_, error_message);
		}
	}

protected:
	int error_code_;
	std::string error_message_;

private:
	auto CreateCommonErrorMessage(Category category, int error_code, std::string_view arg) -> std::string;
};


//! Provides warning information after module importing/converting/exporting
class Status
{
public:
	// The source of the error/warning
	using Category = ModuleException::Category;

	Status() { Clear(); }

	auto ErrorOccurred() const -> bool { return error_.get(); }
	auto WarningsIssued() const -> bool { return !warning_messages_.empty(); }

	void PrintError(bool use_std_err = true) const;
	void PrintWarnings(bool use_std_err = false) const;

	//! Prints error and warnings that occurred during the last action. Returns true if an error occurred.
	auto HandleResults() const -> bool;

	void Clear()
	{
		warning_messages_.clear();
		error_.reset();
	}

	void AddError(ModuleException&& error)
	{
		if (!error_) { error_ = std::make_unique<ModuleException>(std::move(error)); }
		else { *error_ = std::move(error); }
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
	Category category_ = Category::kNone;
};


//! NotImplementedException because I took exception to the standard library not implementing it
class NotImplementedException : public std::logic_error
{
public:
	NotImplementedException() : std::logic_error{"Function not yet implemented."} {}
};

} // namespace d2m
