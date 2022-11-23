/*
    webapp.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    A dmf2mod wrapper for WebAssembly.
*/

#include "dmf2mod.h"
#include "utils.h"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>

using namespace d2m;

// NOTE: When using std::cout, make sure you end it with a newline to flush output. Otherwise nothing will appear.

static ModulePtr G_Module;
static std::string G_InputFilename;

struct OptionDefinitionWrapper
{
    using Type = OptionDefinition::Type;
    int id;
    OptionType optionType;
    Type valueType;
    std::string name;
    std::string displayName;
    std::string defaultValue;
    std::vector<std::string> acceptedValues;
    std::string description;
};

struct OptionWrapper
{
    std::string name;
    std::string value;
};

static OptionDefinitionWrapper WrapOptionDefinition(const OptionDefinition& definition);
static bool UnwrapOptions(ConversionOptionsPtr& options, const std::vector<OptionWrapper>& optionsWrapped);
static void SetStatusType(bool isError);

int main()
{
    Initialize();

    // Initialize global options (for web app, user won't provide them)
    GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Force).SetValue(true);
    GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Verbose).SetValue(false);

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
std::vector<int> GetAvailableModulesWrapper()
{
    std::vector<int> intVec;
    const std::vector<TypeEnum> enumVec = Factory<Module>::GetInitializedTypes();
    std::transform(enumVec.cbegin(), enumVec.cend(), std::back_inserter(intVec), [](TypeEnum m){ return static_cast<int>(m); });
    return intVec;
}

/*
 * Returns the module file extension when given a module type
 */
std::string GetExtensionFromTypeWrapper(int moduleType)
{
    return Utils::GetExtensionFromType(static_cast<ModuleType>(moduleType));
}

/*
 * Returns a vector of option definitions for the given module type
 */
std::vector<OptionDefinitionWrapper> GetOptionDefinitionsWrapper(int moduleType)
{
    auto options = Factory<ConversionOptions>::GetInfo(static_cast<ModuleType>(moduleType))->optionDefinitions;
    std::vector<OptionDefinitionWrapper> ret;

    if (options.Count() == 0)
        return ret;

    for (const auto& mapPair : options.GetIdMap())
    {
        ret.push_back(WrapOptionDefinition(mapPair.second));
    }

    return ret;
}

/*
 * Imports and stores module from specified filename
 * Returns true upon failure
 */
bool ModuleImport(std::string filename)
{
    SetStatusType(true);

    const ModuleType input_type = Utils::GetTypeFromFilename(filename);
    if (input_type == ModuleType::NONE)
    {
        std::cerr << "The input file is not recognized as a supported module type.\n\n";
        return true;
    }

    G_InputFilename = filename;

    G_Module = Factory<ModuleBase>::Create(input_type);
    if (!G_Module)
    {
        std::cerr << "Error during import:\n";
        std::cerr << "ERROR: Not enough memory.\n";
        return true;
    }

    G_Module->Import(filename);

    if (G_Module->GetStatus().ErrorOccurred())
    {
        std::cerr << "Errors during import:\n";
        G_Module->GetStatus().PrintError();
        return true;
    }

    if (G_Module->GetStatus().WarningsIssued())
    {
        SetStatusType(false);
        std::cerr << "Warnings during import:\n";
        G_Module->GetStatus().PrintWarnings(true);
    }

    return false;
}

/*
 * Converts the previously imported module to a module of the given file extension.
 * Returns true if an error occurred, or false if successful.
 */
bool ModuleConvert(std::string outputFilename, const std::vector<OptionWrapper>& optionsWrapped)
{
    if (!G_Module)
        return true; // Need to import the module first

    if (outputFilename.empty())
        return true; // Invalid argument

    if (outputFilename == G_InputFilename)
        return true; // Same type; No conversion necessary

    SetStatusType(true);
    const auto moduleType = Utils::GetTypeFromFilename(outputFilename);
    if (moduleType == ModuleType::NONE)
    {
        std::cerr << "The output file is not recognized as a supported module type.\n\n";
        return true;
    }

    // Create conversion options object
    ConversionOptionsPtr options = Factory<ConversionOptions>::Create(moduleType);
    if (!options)
    {
        std::cerr << "Error occurred when creating ConversionOptions object. Likely a registration issue.\n\n";
        return true; // Registration issue
    }

    // Set options
    if (UnwrapOptions(options, optionsWrapped))
        return true; // Error unwrapping options

    ModulePtr output = G_Module->Convert(moduleType, options);
    if (!output)
        return true;

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

    if (output->Export(outputFilename))
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

static OptionDefinitionWrapper WrapOptionDefinition(const OptionDefinition& definition)
{
    OptionDefinitionWrapper ret;
    ret.id = definition.GetId();
    ret.optionType = definition.GetOptionType();
    ret.valueType = definition.GetValueType();
    ret.name = definition.HasName() ? definition.GetName() : std::string(1, definition.GetShortName());
    ret.displayName = definition.GetDisplayName();
    ret.defaultValue = ModuleOptionUtils::ConvertToString(definition.GetDefaultValue());
    ret.description = definition.GetDescription();

    ret.acceptedValues.clear();
    for (const auto& val : definition.GetAcceptedValuesOrdered())
    {
        ret.acceptedValues.push_back(ModuleOptionUtils::ConvertToString(val));
    }

    return ret;
}

static bool UnwrapOptions(ConversionOptionsPtr& options, const std::vector<OptionWrapper>& optionsWrapped)
{
    for (const auto& optionWrapped : optionsWrapped)
    {
        auto& option = options->GetOption(optionWrapped.name);

        OptionDefinition::value_t val;
        if (ModuleOptionUtils::ConvertToValue(optionWrapped.value, option.GetDefinition()->GetValueType(), val))
            return true; // Error converting string to value_t

        option.SetValue(std::move(val));
    }
    return false;
}

static void SetStatusType(bool isError)
{
    EM_ASM({
        statusMessageIsError = $0;
    }, isError);
}


///////////////////////////
//  Emscripten Bindings  //
///////////////////////////

EMSCRIPTEN_BINDINGS(my_value_example) {
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
        .value("OPTION", OPTION)
        .value("COMMAND", COMMAND);

    emscripten::enum_<OptionDefinitionWrapper::Type>("OptionValueType")
        .value("BOOL", OptionDefinitionWrapper::Type::BOOL)
        .value("INT", OptionDefinitionWrapper::Type::INT)
        .value("DOUBLE", OptionDefinitionWrapper::Type::DOUBLE)
        .value("STRING", OptionDefinitionWrapper::Type::STRING);

    // Register structs
    emscripten::value_object<OptionDefinitionWrapper>("OptionDefinition")
        .field("id", &OptionDefinitionWrapper::id)
        .field("optionType", &OptionDefinitionWrapper::optionType)
        .field("valueType", &OptionDefinitionWrapper::valueType)
        .field("name", &OptionDefinitionWrapper::name)
        .field("displayName", &OptionDefinitionWrapper::displayName)
        .field("defaultValue", &OptionDefinitionWrapper::defaultValue)
        .field("acceptedValues", &OptionDefinitionWrapper::acceptedValues)
        .field("description", &OptionDefinitionWrapper::description);

    emscripten::value_array<OptionWrapper>("Option")
        .element(&OptionWrapper::name)
        .element(&OptionWrapper::value);
}
