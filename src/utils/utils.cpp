/*
    utils.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines various utility methods used by dmf2mod.
*/

#include "utils.h"

#include "config.h"

#include <fstream>
//#include <filesystem>

using namespace d2m;

// File utils

std::string ModuleUtils::GetBaseNameFromFilename(const std::string& filename)
{
    // Filename must contain base name, a dot, then the extension
    if (filename.size() <= 2)
        return "";

    const size_t slashPos = filename.find_first_of("\\/");
    // If file separator is at the end:
    if (slashPos != std::string::npos && slashPos >= filename.size() - 2)
        return "";

    const size_t startPos = slashPos == std::string::npos ? 0 : slashPos + 1;

    const size_t dotPos = filename.rfind('.');
    // If dot is at start, not found, or at end:
    if (dotPos == 0 || dotPos == std::string::npos || dotPos + 1 >= filename.size())
        return "";

    // The dot should come after the start position
    if (startPos >= dotPos)
        return "";

    return filename.substr(startPos, dotPos - startPos);
}

std::string ModuleUtils::ReplaceFileExtension(const std::string& filename, const std::string& newFileExtension)
{
    // filename must contain a file extension
    // newFileExtension should not contain a dot

    const size_t dotPos = filename.rfind('.');
    // If dot is at start, not found, or at end:
    if (dotPos == 0 || dotPos == std::string::npos || dotPos + 1 >= filename.size())
        return "";

    return filename.substr(0, dotPos + 1) + newFileExtension;
}

std::string ModuleUtils::GetFileExtension(const std::string& filename)
{
    const size_t dotPos = filename.rfind('.');
    // If dot is at start, not found, or at end:
    if (dotPos == 0 || dotPos == std::string::npos || dotPos + 1 >= filename.size())
        return "";

    return filename.substr(dotPos + 1);
}

bool ModuleUtils::FileExists(const std::string& filename)
{
    std::ifstream file(filename);
    return file.is_open();
}

// File utils which require Factory initialization

ModuleType ModuleUtils::GetTypeFromFilename(const std::string& filename)
{
    std::string ext = ModuleUtils::GetFileExtension(filename);
    return GetTypeFromFileExtension(ext);
}

ModuleType ModuleUtils::GetTypeFromFileExtension(const std::string& extension)
{
    if (extension.empty())
        return ModuleType::NONE;

    for (const auto& [type, info] : Factory<Module>::TypeInfo())
    {
        if (static_cast<Info<Module> const*>(info)->fileExtension == extension)
            return type;
    }

    return ModuleType::NONE;
}

std::string ModuleUtils::GetExtensionFromType(ModuleType moduleType)
{
    return static_cast<Info<Module> const*>(Factory<Module>::GetInfo(moduleType))->fileExtension;
}

// Command-line arguments and options utils

std::vector<std::string> ModuleUtils::GetArgsAsVector(int argc, char *argv[])
{
    std::vector<std::string> args(argc, "");
    for (int i = 0; i < argc; i++)
    {
        args[i] = argv[i];
    }
    return args;
}
