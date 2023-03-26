/*
 * utils.cpp
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines various utility methods used by dmf2mod.
 */

#include "utils/utils.h"

#include "core/factory.h"
#include "core/module.h"

#include <filesystem>

using namespace d2m;

// File utils

auto Utils::GetBaseNameFromFilename(std::string_view filename) -> std::string
{
    return std::filesystem::path{filename}.stem().string();
}

auto Utils::ReplaceFileExtension(std::string_view filename, std::string_view new_file_extension) -> std::string
{
    // new_file_extension may or may not contain a dot
    return std::filesystem::path{filename}.replace_extension(new_file_extension).string();
}

auto Utils::GetFileExtension(std::string_view filename) -> std::string_view
{
    std::string_view ext = std::filesystem::path{filename}.extension().c_str();
    if (!ext.empty()) { return ext.substr(1); } // Remove "."
    return "";
}

auto Utils::FileExists(std::string_view filename) -> bool
{
    return std::filesystem::exists(filename);
}

// File utils which require Factory initialization

auto Utils::GetTypeFromFilename(std::string_view filename) -> ModuleType
{
    const auto ext = Utils::GetFileExtension(filename);
    return GetTypeFromFileExtension(ext);
}

auto Utils::GetTypeFromFileExtension(std::string_view extension) -> ModuleType
{
    if (extension.empty()) { return ModuleType::kNone; }

    for (const auto& [type, info] : Factory<Module>::TypeInfo())
    {
        if (static_cast<Info<Module> const*>(info.get())->file_extension == extension)
        {
            return type;
        }
    }

    return ModuleType::kNone;
}

auto Utils::GetExtensionFromType(ModuleType moduleType) -> std::string
{
    return static_cast<Info<Module> const*>(Factory<Module>::GetInfo(moduleType))->file_extension;
}

// Command-line arguments and options utils

auto Utils::GetArgsAsVector(int argc, char** argv) -> std::vector<std::string>
{
    std::vector<std::string> args(argc, "");
    for (int i = 0; i < argc; i++)
    {
        args[i] = argv[i];
    }
    return args;
}
