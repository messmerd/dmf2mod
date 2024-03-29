/*
 * utils.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Declares various utility methods used by dmf2mod.
 */

#pragma once

#include "core/config_types.h"

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

namespace d2m {

// Class containing miscellaneous helpful static methods
class Utils
{
public:
    // File utils
    [[nodiscard]] static auto GetBaseNameFromFilename(std::string_view filename) -> std::string;
    [[nodiscard]] static auto ReplaceFileExtension(std::string_view filename, std::string_view new_file_extension) -> std::string;
    [[nodiscard]] static auto GetFileExtension(std::string_view filename) -> std::string;
    [[nodiscard]] static auto FileExists(std::string_view filename) -> bool;

    // File utils which require Factory initialization
    [[nodiscard]] static auto GetTypeFromFilename(std::string_view filename) -> ModuleType;
    [[nodiscard]] static auto GetTypeFromFileExtension(std::string_view extension) -> ModuleType;
    [[nodiscard]] static auto GetTypeFromCommandName(std::string_view command_name) -> ModuleType;
    [[nodiscard]] static auto GetExtensionFromType(ModuleType module_type) -> std::string_view;

    // Command-line arguments and options utils
    [[nodiscard]] static auto GetArgsAsVector(int argc, char** argv) -> std::vector<std::string>;

    // String utils (borrowed from Stack Overflow)
    static inline void StringTrimLeft(std::string& str)
    {
        // Trim string from start (in place)
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
    }

    static inline void StringTrimRight(std::string& str)
    {
        // Trim string from end (in place)
        str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), str.end());
    }

    static inline void StringTrimBothEnds(std::string& str)
    {
        // Trim string from both ends (in place)
        StringTrimLeft(str);
        StringTrimRight(str);
    }
};

} // namespace d2m
