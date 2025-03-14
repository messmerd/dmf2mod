/*
 * webapp.cpp
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * A dmf2mod wrapper for WebAssembly.
 */

#include "dmf2mod.h"

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>

#include <iostream>

using namespace d2m;

// NOTE: When using std::cout, make sure you end it with a newline to flush output. Otherwise nothing will appear.

static ModulePtr kModule;
static std::string kInputFilename;

struct OptionDefinitionWrapper
{
	using Type = OptionDefinition::Type;
	int id;
	OptionType option_type;
	Type value_type;
	std::string name;
	std::string display_name;
	std::string default_value;
	std::vector<std::string> accepted_values;
	std::string description;
};

struct OptionWrapper
{
	std::string name;
	std::string value;
};

static auto WrapOptionDefinition(const OptionDefinition& definition) -> OptionDefinitionWrapper;
static auto UnwrapOptions(ConversionOptionsPtr& options, const std::vector<OptionWrapper>& options_wrapped) -> bool;
static void SetStatusType(bool is_error);

auto main() -> int
{
	// Initialize global options (for web app, user won't provide them)
	GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kForce).SetValue(true);
	GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kVerbose).SetValue(false);

	return 0;
}

//////////////////////////
//  Exported functions  //
//////////////////////////

/*
 * Returns a vector of ints representing the module type that are supported.
 * Int is used instead of ModuleType to avoid the need to redefine the ModuleType
 *  enum in the Emscripten binding.
 */
auto GetAvailableModulesWrapper() -> std::vector<int>
{
	std::vector<int> int_vec;
	const std::vector<ModuleType> modules = Factory<Module>::GetInitializedTypes();
	std::transform(modules.cbegin(), modules.cend(), std::back_inserter(int_vec), [](ModuleType m){ return static_cast<int>(m); });
	return int_vec;
}

/*
 * Returns the module file extension when given a module type
 */
auto GetExtensionFromTypeWrapper(int module_type) -> std::string
{
	return std::string{Utils::GetExtensionFromType(static_cast<ModuleType>(module_type))};
}

/*
 * Returns a vector of option definitions for the given module type
 */
auto GetOptionDefinitionsWrapper(int module_type) -> std::vector<OptionDefinitionWrapper>
{
	auto options = Factory<ConversionOptions>::GetInfo(static_cast<ModuleType>(module_type))->option_definitions;
	std::vector<OptionDefinitionWrapper> ret;

	if (options.Count() == 0) { return ret; }
	for (const auto& map_pair : options.GetIdMap())
	{
		ret.push_back(WrapOptionDefinition(map_pair.second));
	}

	return ret;
}

/*
 * Imports and stores module from specified filename
 * Returns true upon failure
 */
auto ModuleImport(std::string filename) -> bool
{
	SetStatusType(true);

	const ModuleType input_type = Utils::GetTypeFromFilename(filename);
	if (input_type == ModuleType::kNone)
	{
		std::cerr << "The input file is not recognized as a supported module type.\n\n";
		return true;
	}

	kInputFilename = filename;

	kModule = Factory<ModuleBase>::Create(input_type);
	if (!kModule)
	{
		std::cerr << "Error during import:\n";
		std::cerr << "ERROR: Not enough memory.\n";
		return true;
	}

	kModule->Import(filename);

	if (kModule->GetStatus().ErrorOccurred())
	{
		std::cerr << "Errors during import:\n";
		kModule->GetStatus().PrintError();
		return true;
	}

	if (kModule->GetStatus().WarningsIssued())
	{
		SetStatusType(false);
		std::cerr << "Warnings during import:\n";
		kModule->GetStatus().PrintWarnings(true);
	}

	return false;
}

/*
 * Converts the previously imported module to a module of the given file extension.
 * Returns true if an error occurred, or false if successful.
 */
auto ModuleConvert(std::string output_filename, const std::vector<OptionWrapper>& options_wrapped) -> bool
{
	if (!kModule) { return true; } // Need to import the module first
	if (output_filename.empty()) { return true; } // Invalid argument
	if (output_filename == kInputFilename) { return true; } // Same type; No conversion necessary

	SetStatusType(true);
	const auto module_type = Utils::GetTypeFromFilename(output_filename);
	if (module_type == ModuleType::kNone)
	{
		std::cerr << "The output file is not recognized as a supported module type.\n\n";
		return true;
	}

	// Create conversion options object
	ConversionOptionsPtr options = Factory<ConversionOptions>::Create(module_type);
	if (!options)
	{
		std::cerr << "Error occurred when creating ConversionOptions object. Likely a registration issue.\n\n";
		return true; // Registration issue
	}

	// Set options
	if (UnwrapOptions(options, options_wrapped)) { return true; } // Error unwrapping options

	ModulePtr output = kModule->Convert(module_type, options);
	if (!output) { return true; }

	if (output->GetStatus().ErrorOccurred())
	{
		SetStatusType(true);
		std::cerr << "Error during conversion:\n";
		output->GetStatus().PrintError();
		return true;
	}

	if (output->GetStatus().WarningsIssued())
	{
		SetStatusType(false);
		std::cerr << "Warning(s) during conversion:\n";
		output->GetStatus().PrintWarnings(true);
	}

	SetStatusType(true);

	if (output->Export(output_filename))
	{
		std::cerr << "Error during export:\n";
		output->GetStatus().PrintError();
		return true;
	}

	return false;
}


////////////////////////
//  Helper functions  //
////////////////////////

static auto WrapOptionDefinition(const OptionDefinition& definition) -> OptionDefinitionWrapper
{
	OptionDefinitionWrapper ret;
	ret.id = definition.GetId();
	ret.option_type = definition.GetOptionType();
	ret.value_type = definition.GetValueType();
	ret.name = definition.HasName() ? definition.GetName() : std::string(1, definition.GetShortName());
	ret.display_name = definition.GetDisplayName();
	ret.default_value = ModuleOptionUtils::ConvertToString(definition.GetDefaultValue());
	ret.description = definition.GetDescription();

	ret.accepted_values.clear();
	for (const auto& val : definition.GetAcceptedValuesOrdered())
	{
		ret.accepted_values.push_back(ModuleOptionUtils::ConvertToString(val));
	}

	return ret;
}

static auto UnwrapOptions(ConversionOptionsPtr& options, const std::vector<OptionWrapper>& options_wrapped) -> bool
{
	for (const auto& option_wrapped : options_wrapped)
	{
		auto& option = options->GetOption(option_wrapped.name);

		OptionDefinition::ValueType val;
		if (ModuleOptionUtils::ConvertToValue(option_wrapped.value, option.GetDefinition()->GetValueType(), val))
		{
			return true; // Error converting string to ValueType
		}

		option.SetValue(std::move(val));
	}
	return false;
}

static void SetStatusType(bool is_error)
{
	EM_ASM({
		statusMessageIsError = $0;
	}, is_error);
}


///////////////////////////
//  Emscripten Bindings  //
///////////////////////////

EMSCRIPTEN_BINDINGS(dmf2mod) {
	// Register functions
	emscripten::function("getAvailableModules", &GetAvailableModulesWrapper);
	emscripten::function("getExtensionFromType", &GetExtensionFromTypeWrapper);
	emscripten::function("getOptionDefinitions", &GetOptionDefinitionsWrapper);
	emscripten::function("moduleImport", &ModuleImport);
	emscripten::function("moduleConvert", &ModuleConvert);

	// Register vectors
	emscripten::register_vector<int>("VectorInt");
	emscripten::register_vector<std::string>("VectorString");
	emscripten::register_vector<OptionDefinitionWrapper>("VectorOptionDefinition");
	emscripten::register_vector<OptionWrapper>("VectorOption");

	// Register enums
	emscripten::enum_<OptionType>("OptionType")
		.value("OPTION", kOption)
		.value("COMMAND", kCommand);

	emscripten::enum_<OptionDefinitionWrapper::Type>("OptionValueType")
		.value("BOOL", OptionDefinitionWrapper::Type::kBool)
		.value("INT", OptionDefinitionWrapper::Type::kInt)
		.value("DOUBLE", OptionDefinitionWrapper::Type::kDouble)
		.value("STRING", OptionDefinitionWrapper::Type::kString);

	// Register structs
	emscripten::value_object<OptionDefinitionWrapper>("OptionDefinition")
		.field("id", &OptionDefinitionWrapper::id)
		.field("optionType", &OptionDefinitionWrapper::option_type)
		.field("valueType", &OptionDefinitionWrapper::value_type)
		.field("name", &OptionDefinitionWrapper::name)
		.field("displayName", &OptionDefinitionWrapper::display_name)
		.field("defaultValue", &OptionDefinitionWrapper::default_value)
		.field("acceptedValues", &OptionDefinitionWrapper::accepted_values)
		.field("description", &OptionDefinitionWrapper::description);

	emscripten::value_array<OptionWrapper>("Option")
		.element(&OptionWrapper::name)
		.element(&OptionWrapper::value);
}
