#include "modules.h"
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>

// NOTE: When using std::cout, make sure you end it with a newline to flush output. Otherwise nothing will appear.

static ModulePtr G_Module;
static std::string G_InputFilename;
static CommonFlags G_CoreOptions;

int main()
{
    ModuleUtils::RegisterModules();

    // Initialize core options (for web app, user won't provide them)
    G_CoreOptions.force = true;
    G_CoreOptions.silent = false;
    ModuleUtils::SetCoreOptions(G_CoreOptions);

    return 0;
}

/*
 * Returns a comma-delimited string representing 
 * the file extensions of the registered modules
 */
std::string GetAvailableModules()
{
    auto modules = ModuleUtils::GetAvailableModules();
    std::string modulesString;
    for (unsigned i = 0; i < modules.size(); i++)
    {
        modulesString += modules[i];
        if (i != modules.size() - 1)
            modulesString += ",";
    }
    return modulesString;
}

/*
 * Returns a semi-colon delimited string representing 
 * the command-line options available for the given
 * module type
 */
std::string GetAvailableOptions(std::string moduleType)
{
    ModuleType moduleTypeEnum = ModuleUtils::GetTypeFromFileExtension(moduleType);
    auto options = Module::GetAvailableOptions(moduleTypeEnum);
    //std::vector<std::string> options;
    std::string optionsString;
    for (unsigned i = 0; i < options.size(); i++)
    {
        optionsString += options[i];
        if (i != options.size() - 1)
            optionsString += ";";
    }
    return optionsString;
}

/*
 * Imports and stores module from specified filename
 * Returns true upon failure
 */
bool ModuleImport(std::string filename)
{
    G_InputFilename = filename;
    G_Module = Module::CreateAndImport(filename);
    return !G_Module;
}

/*
 * Converts the previously imported module to a module of the
 * given file extension.
 * Returns the filename of the converted file, or an empty string if an error occurred
 */
std::string ModuleConvert(std::string moduleType)
{
    if (!G_Module)
        return ""; // Need to import the module first

    const std::string outputFilename = ModuleUtils::ReplaceFileExtension(G_InputFilename, moduleType);
    if (outputFilename.empty())
        return "";
    
    if (outputFilename == G_InputFilename)
        return outputFilename; // Same type; No conversion necessary

    auto moduleTypeEnum = ModuleUtils::GetTypeFromFileExtension(moduleType);

    // Not passing options yet; Use default options:
    ConversionOptionsPtr options = ConversionOptions::Create(moduleTypeEnum);
    if (!options)
        return ""; // Registration issue

    ModulePtr output = G_Module->Convert(moduleTypeEnum, options);
    if (!output || output->GetStatus().Failed())
        return "";

    if (output->Export(outputFilename))
        return "";
    
    return outputFilename;
}


EMSCRIPTEN_BINDINGS(dmf2mod)
{
    emscripten::function("getAvailableModules", &GetAvailableModules);
    emscripten::function("getAvailableOptions", &GetAvailableOptions);
    emscripten::function("moduleImport", &ModuleImport);
    emscripten::function("moduleConvert", &ModuleConvert);
}
