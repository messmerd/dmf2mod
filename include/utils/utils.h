/*
    utils.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares various utility methods used by dmf2mod.
*/

#pragma once

#include <string>
#include <vector>
#include <algorithm>

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

    // String utils (borrowed from Stack Overflow)
    static inline void StringTrimLeft(std::string &str)
    {
        // Trim string from start (in place)
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
    }

    static inline void StringTrimRight(std::string &str)
    {
        // Trim string from end (in place)
        str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), str.end());
    }

    static inline void StringTrimBothEnds(std::string &str)
    {
        // Trim string from both ends (in place)
        StringTrimLeft(str);
        StringTrimRight(str);
    }
};

} // namespace d2m
