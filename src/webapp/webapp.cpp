/*
    webapp.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    WebAssembly backend for dmf2mod.
*/

#include "dmf2mod.h"
#include "utils.h"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>

#include <string>
#include <sstream>

// NOTE: When using std::cout, make sure you end it with a newline to flush output. Otherwise nothing will appear.

static ModulePtr G_Module;
static std::string G_InputFilename;
static CommonFlags G_CoreOptions;

static void SetStatusType(bool isError);

int main()
{
    Registrar::RegisterModules();

    // Initialize core options (for web app, user won't provide them)
    G_CoreOptions.force = true;
    G_CoreOptions.silent = true;
    ModuleUtils::SetCoreOptions(G_CoreOptions);

    return 0;
}

/*
 * Returns a comma-delimited string representing 
 * the file extensions of the registered modules
 */
std::string GetAvailableModules()
{
    auto modules = Registrar::GetAvailableModules();
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
    ModuleType moduleTypeEnum = Registrar::GetTypeFromFileExtension(moduleType);
    ModuleOptions options = Module::GetAvailableOptions(moduleTypeEnum);
    
    std::string optionsString;
    for (unsigned i = 0; i < options.Count(); i++)
    {
        optionsString += options.Item(i).GetName();
        if (i != options.Count() - 1)
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
    SetStatusType(true);
    if (Registrar::GetTypeFromFilename(filename) == ModuleType::NONE)
    {
        std::cerr << "The input file is not recognized as a supported module type.\n\n";
        return true;
    }

    G_InputFilename = filename;

    G_Module = Module::CreateAndImport(filename);
    if (!G_Module)
    {
        std::cerr << "Error during import:\n";
        std::cerr << "ERROR: The module type may not be registered.\n\n";
        return true;
    }
    
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
 * Converts the previously imported module to a module of the
 * given file extension.
 * Returns the filename of the converted file, or an empty string if an error occurred
 */
std::string ModuleConvert(std::string outputFilename, std::string commandLineArgs)
{
    if (!G_Module)
        return ""; // Need to import the module first
    
    if (outputFilename == "")
        return ""; // Invalid argument

    if (outputFilename == G_InputFilename)
        return ""; // Same type; No conversion necessary

    SetStatusType(true);
    const auto moduleType = Registrar::GetTypeFromFilename(outputFilename);
    if (moduleType == ModuleType::NONE)
    {
        std::cerr << "The output file is not recognized as a supported module type.\n\n";
        return "";
    }

    // Create conversion options object
    ConversionOptionsPtr options = ConversionOptions::Create(moduleType);
    if (!options)
    {
        std::cerr << "Error occurred when creating ConversionOptions object. Likely a registration issue.\n\n";
        return ""; // Registration issue
    }

    // Convert newline delimited string into vector of strings:
    std::string temp;
    std::stringstream ss(commandLineArgs);
    std::vector<std::string> args;

    while (getline(ss, temp, '\n'))
    {
        args.push_back(temp);
    }

    // Parse the command-line arguments
    if (options->ParseArgs(args))
    {
        // ParseArgs will print error to stderr if one occurs.
        return ""; // Failed to parse args
    }

    ModulePtr output = G_Module->Convert(moduleType, options);
    if (!output)
        return "";
    
    if (output->GetStatus().ErrorOccurred())
    {
        SetStatusType(true);
        std::cerr << "Error during conversion:\n";
        output->GetStatus().PrintError();
        return "";
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
        return "";
    }
    
    return outputFilename;
}

static void SetStatusType(bool isError)
{
    EM_ASM({
        statusMessageIsError = $0;
    }, isError);
}


EMSCRIPTEN_BINDINGS(dmf2mod)
{
    emscripten::function("getAvailableModules", &GetAvailableModules);
    emscripten::function("getAvailableOptions", &GetAvailableOptions);
    emscripten::function("moduleImport", &ModuleImport);
    emscripten::function("moduleConvert", &ModuleConvert);
}
