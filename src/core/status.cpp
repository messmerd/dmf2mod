/*
 * status.cpp
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines Status and ModuleException, which are used
 * for handling errors and warnings.
 */

#include "core/status.h"

#include <iostream>

using namespace d2m;

void Status::PrintError(bool use_std_err) const
{
    if (!ErrorOccurred()) { return; }

    if (use_std_err) { std::cerr << error_->what() << "\n"; }
    else { std::cout << error_->what() << "\n"; }
}

void Status::PrintWarnings(bool use_std_err) const
{
    if (!warning_messages_.empty())
    {
        for (const auto& message : warning_messages_)
        {
            if (use_std_err) { std::cerr << message << "\n"; }
            else { std::cout << message << "\n"; }
        }
    }
}

auto Status::HandleResults() const -> bool
{
    PrintError();

    /*
    std::string action_str;
    switch (category_)
    {
        case Category::kNone:
            action_str = "init"; break;
        case Category::kImport:
            action_str = "import"; break;
        case Category::kExport:
            action_str = "export"; break;
        case Category::kConvert:
            action_str = "conversion"; break;
    }
    */

    if (WarningsIssued())
    {
        if (ErrorOccurred()) { std::cerr << "\n"; }

        //const std::string plural = warning_messages_.size() > 1 ? "s" : "";
        //std::cout << "Warning" << plural << " issued during " << action_str << ":\n";
        
        PrintWarnings();
    }
    return ErrorOccurred();
}

auto ModuleException::CreateCommonErrorMessage(Category category, int error_code, std::string_view arg) -> std::string
{
    switch (category)
    {
        case Category::kNone:
            return "";
        case Category::kImport:
            switch (error_code)
            {
                case (int)ImportError::kSuccess:
                    return "No error.";
                default:
                    return "";
            }
            break;
        case Category::kExport:
            switch (error_code)
            {
                case (int)ExportError::kSuccess:
                    return "No error.";
                case (int)ExportError::kFileOpen:
                    return "Failed to open file for writing.";
                default:
                    return "";
            }
            break;
        case Category::kConvert:
            switch (error_code)
            {
                case (int)ConvertError::kSuccess:
                    return "No error.";
                case (int)ConvertError::kUnsuccessful: // This is the only convert error applied to the input module.
                    return "Module conversion was unsuccessful. See the output module's status for more information.";
                case (int)ConvertError::kInvalidArgument:
                    return "Invalid argument.";
                case (int)ConvertError::kUnsupportedInputType:
                    return "Input type '" + std::string{arg} + "' is unsupported for this module.";
                default:
                    return "";
            }
            break;
    }
    return "";
}
