#include "modules.h"
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>

#define EXPORT __attribute__((visibility("default")))

static ModulePtr G_Module;
static std::string G_InputFilename;

/*
 * Initialize modules
 * Must be called before any other function
 */
EXPORT void Init()
{
    ModuleUtils::RegisterModules();

    CommonFlags coreOptions;
    coreOptions.force = true;
    coreOptions.silent = false;
    ModuleUtils::SetCoreOptions(coreOptions);
}

/*
 * Returns a list of strings representing the file 
 * extensions of the registered modules
 */
EXPORT std::vector<std::string> GetAvailableModules()
{
    return ModuleUtils::GetAvaliableModules();
}

/*
 * Imports and stores module from specified filename
 * Returns true upon failure
 */
EXPORT bool ModuleImport(std::string filename)
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
EXPORT std::string ModuleConvert(std::string moduleType)
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
    emscripten::function("init", &Init);
    emscripten::function("getAvailableModules", &GetAvailableModules);
    emscripten::function("moduleImport", &ModuleImport);
    emscripten::function("moduleConvert", &ModuleConvert);
}

