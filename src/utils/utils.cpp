/*
    utils.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines various utility methods used by dmf2mod.
*/

#include "utils/utils.h"

#include "core/factory.h"
#include "core/module.h"

#include <filesystem>

using namespace d2m;

// File utils

std::string Utils::GetBaseNameFromFilename(const std::string& filename)
{
    return std::filesystem::path{filename}.stem().string();
}

std::string Utils::ReplaceFileExtension(const std::string& filename, const std::string& new_file_extension)
{
    // new_file_extension may or may not contain a dot
    return std::filesystem::path{filename}.replace_extension(new_file_extension).string();
}

std::string Utils::GetFileExtension(const std::string& filename)
{
    auto ext = std::filesystem::path{filename}.extension();
    if (!ext.empty())
        return ext.string().substr(1); // Remove "."
    return "";
}

bool Utils::FileExists(const std::string& filename)
{
    return std::filesystem::exists(filename);
}

// File utils which require Factory initialization

ModuleType Utils::GetTypeFromFilename(const std::string& filename)
{
    const std::string ext = Utils::GetFileExtension(filename);
    return GetTypeFromFileExtension(ext);
}

ModuleType Utils::GetTypeFromFileExtension(const std::string& extension)
{
    if (extension.empty())
        return ModuleType::kNone;

    for (const auto& [type, info] : Factory<Module>::TypeInfo())
    {
        if (static_cast<Info<Module> const*>(info.get())->file_extension == extension)
            return type;
    }

    return ModuleType::kNone;
}

std::string Utils::GetExtensionFromType(ModuleType moduleType)
{
    return static_cast<Info<Module> const*>(Factory<Module>::GetInfo(moduleType))->file_extension;
}

// Command-line arguments and options utils

std::vector<std::string> Utils::GetArgsAsVector(int argc, char *argv[])
{
    std::vector<std::string> args(argc, "");
    for (int i = 0; i < argc; i++)
    {
        args[i] = argv[i];
    }
    return args;
}
