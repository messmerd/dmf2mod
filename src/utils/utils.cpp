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

namespace d2m {

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

auto Utils::GetFileExtension(std::string_view filename) -> std::string
{
	const auto ext = std::filesystem::path{filename}.extension().string();
	if (!ext.empty()) { return ext.substr(1); } // Remove "."
	return std::string{};
}

auto Utils::FileExists(std::string_view filename) -> bool
{
	std::error_code ec;
	return std::filesystem::is_regular_file(filename, ec);
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
		if (static_cast<const Info<Module>*>(info.get())->file_extension == extension)
		{
			return type;
		}
	}

	return ModuleType::kNone;
}

auto Utils::GetTypeFromCommandName(std::string_view command_name) -> ModuleType
{
	if (command_name.empty()) { return ModuleType::kNone; }

	for (const auto& [type, info] : Factory<Module>::TypeInfo())
	{
		if (static_cast<const Info<Module>*>(info.get())->command_name == command_name)
		{
			return type;
		}
	}

	return ModuleType::kNone;
}

auto Utils::GetExtensionFromType(ModuleType moduleType) -> std::string_view
{
	return static_cast<const Info<Module>*>(Factory<Module>::GetInfo(moduleType))->file_extension;
}

// Command-line arguments and options utils

auto Utils::GetArgsAsVector(int argc, char** argv) -> std::vector<std::string>
{
	auto args = std::vector<std::string>(argc, "");
	for (int i = 0; i < argc; i++)
	{
		args[i] = argv[i];
	}
	return args;
}

} // namespace d2m
