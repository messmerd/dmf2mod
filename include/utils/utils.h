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

// Class containing miscellaneous Module-related static methods
class ModuleUtils
{
public:
    // File utils
    static std::string GetBaseNameFromFilename(const std::string& filename);
    static std::string ReplaceFileExtension(const std::string& filename, const std::string& newFileExtension);
    static std::string GetFileExtension(const std::string& filename);
    static bool FileExists(const std::string& filename);

    // Command-line arguments and options utils
    static std::vector<std::string> GetArgsAsVector(int argc, char *argv[]);
    static bool ParseArgs(std::vector<std::string>& args, const ModuleOptions& optionDefinitions, OptionValues& values);
    static void PrintHelp(ModuleType moduleType);
    static void PrintHelp(const ModuleOptions& options);
    
    // String utils (borrowed from Stack Overflow)
    static inline void StringTrimLeft(std::string &str);
    static inline void StringTrimRight(std::string &str);
    static inline void StringTrimBothEnds(std::string &str);
};

} // namespace d2m
