/*
    utils.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares various utility methods used by dmf2mod.
*/

#pragma once

#include "options.h"
#include "registrar.h"
#include "status.h"

#include <string>
#include <vector>
#include <map>

namespace d2m {

// Command-line options that are supported regardless of which modules are supported
struct CommonFlags
{
    bool force = false;
    bool silent = false;
    // More to be added later
};

// Used for returning input/output info when parsing command-line arguments
struct InputOutput
{
    std::string InputFile;
    ModuleType InputType;
    std::string OutputFile;
    ModuleType OutputType;
};

// Class containing miscellaneous Module-related static methods
class ModuleUtils
{
public:
    static bool ParseArgs(int argc, char *argv[], InputOutput& inputOutputInfo, ConversionOptionsPtr& options);
    static CommonFlags GetCoreOptions() { return m_CoreOptions; }
    static void SetCoreOptions(CommonFlags& options) { m_CoreOptions = options; }

    
    static std::string GetBaseNameFromFilename(const std::string& filename);
    static std::string ReplaceFileExtension(const std::string& filename, const std::string& newFileExtension);
    static std::string GetFileExtension(const std::string& filename);
    static bool FileExists(const std::string& filename);

    static void PrintHelp(ModuleType moduleType);
    
private:
    static bool PrintHelp(const std::string& executable, ModuleType moduleType);

    // Core conversion options
    static CommonFlags m_CoreOptions;
};

} // namespace d2m
