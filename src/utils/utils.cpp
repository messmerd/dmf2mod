/*
    utils.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines various utility methods used by dmf2mod.
*/

#include "utils.h"

#include "factory.h"
#include "module.h"

#include <filesystem>

using namespace d2m;

// File utils

std::string Utils::GetBaseNameFromFilename(const std::string& filename)
{
    return std::filesystem::path{filename}.stem();
}

std::string Utils::ReplaceFileExtension(const std::string& filename, const std::string& newFileExtension)
{
    // newFileExtension may or may not contain a dot
    return std::filesystem::path{filename}.replace_extension(newFileExtension);
}

std::string Utils::GetFileExtension(const std::string& filename)
{
    auto temp = std::filesystem::path{filename}.extension();
    if (!temp.empty())
        return temp.string().substr(1); // Remove "."
    return "";
}

bool Utils::FileExists(const std::string& filename)
{
    return std::filesystem::exists(filename);
}

// File utils which require Factory initialization

ModuleType Utils::GetTypeFromFilename(const std::string& filename)
{
    std::string ext = Utils::GetFileExtension(filename);
    return GetTypeFromFileExtension(ext);
}

ModuleType Utils::GetTypeFromFileExtension(const std::string& extension)
{
    if (extension.empty())
        return ModuleType::NONE;

    for (const auto& [type, info] : Factory<Module>::TypeInfo())
    {
        if (static_cast<Info<Module> const*>(info.get())->fileExtension == extension)
            return type;
    }

    return ModuleType::NONE;
}

std::string Utils::GetExtensionFromType(ModuleType moduleType)
{
    return static_cast<Info<Module> const*>(Factory<Module>::GetInfo(moduleType))->fileExtension;
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
