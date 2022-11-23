/*
    status.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines Status and ModuleException, which are used
    for handling errors and warnings.
*/

#include "status.h"

#include <iostream>

using namespace d2m;

void Status::PrintError(bool useStdErr) const
{
    if (!ErrorOccurred())
        return;

    if (useStdErr)
        std::cerr << m_Error->what() << "\n";
    else
        std::cout << m_Error->what() << "\n";
}

void Status::PrintWarnings(bool useStdErr) const
{
    if (!m_WarningMessages.empty())
    {
        for (const auto& message : m_WarningMessages)
        {
            if (useStdErr)
                std::cerr << message << "\n";
            else
                std::cout << message << "\n";
        }
    }
}

bool Status::HandleResults() const
{
    PrintError();

    /*
    std::string actionStr;
    switch (m_Category)
    {
        case Category::None:
            actionStr = "init"; break;
        case Category::Import:
            actionStr = "import"; break;
        case Category::Export:
            actionStr = "export"; break;
        case Category::Convert:
            actionStr = "conversion"; break;
    }
    */

    if (WarningsIssued())
    {
        if (ErrorOccurred())
            std::cerr << "\n";

        //const std::string plural = m_WarningMessages.size() > 1 ? "s" : "";
        //std::cout << "Warning" << plural << " issued during " << actionStr << ":\n";
        
        PrintWarnings();
    }
    return ErrorOccurred();
}

std::string ModuleException::CreateCommonErrorMessage(Category category, int errorCode, const std::string& arg)
{
    switch (category)
    {
        case Category::None:
            return "";
        case Category::Import:
            switch (errorCode)
            {
                case (int)ImportError::Success:
                    return "No error.";
                default:
                    return "";
            }
            break;
        case Category::Export:
            switch (errorCode)
            {
                case (int)ExportError::Success:
                    return "No error.";
                case (int)ExportError::FileOpen:
                    return "Failed to open file for writing.";
                default:
                    return "";
            }
            break;
        case Category::Convert:
            switch (errorCode)
            {
                case (int)ConvertError::Success:
                    return "No error.";
                case (int)ConvertError::Unsuccessful: // This is the only convert error applied to the input module.
                    return "Module conversion was unsuccessful. See the output module's status for more information.";
                case (int)ConvertError::InvalidArgument:
                    return "Invalid argument.";
                case (int)ConvertError::UnsupportedInputType:
                    return "Input type '" + arg + "' is unsupported for this module.";
                default:
                    return "";
            }
            break;
    }
    return "";
}
