/*
    registrar.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares the Registrar class, which registers all the available 
    modules at runtime, provides factory methods for creating Module and 
    ConversionOptions objects, and provides helper methods for retrieving 
    info about registered modules.

    All changes needed to add support for new module types are done within 
    this header file, its cpp file by the same name, and all_modules.h.
*/

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>


// Add all supported modules to this enum
enum class ModuleType
{
    NONE=0,
    DMF,
    MOD
};


// Forward declares
class ModuleBase;
class ConversionOptionsBase;
template <typename T, typename O> class ModuleInterface;


// Typedefs to make usage easier
typedef ModuleBase Module;
typedef std::unique_ptr<Module> ModulePtr;
typedef ConversionOptionsBase ConversionOptions;
typedef std::unique_ptr<ConversionOptions> ConversionOptionsPtr;


// Handles module registration and creation
class Registrar
{
public:
    // This must be called in main() to register to supported module types with dmf2mod
    static void RegisterModules();

    // Module factory method
    static ModulePtr CreateModule(ModuleType moduleType);

    // ConversionOptions factory method
    static ConversionOptionsPtr CreateConversionOptions(ModuleType moduleType);

    // Helper methods that utilize module registration information
    static std::vector<std::string> GetAvailableModules();
    static ModuleType GetTypeFromFilename(const std::string& filename);
    static ModuleType GetTypeFromFileExtension(const std::string& extension);
    static std::string GetExtensionFromType(ModuleType moduleType);
    static std::vector<std::string> GetAvailableOptions(ModuleType moduleType);

private:
    /*
     * Registers a module in the registration maps
     * TODO: Need to also check whether the ConversionOptionsStatic<T> specialization exists?
     */
    template <class T, 
        class = typename std::enable_if<std::is_base_of<ModuleInterface<T, typename T::OptionsType>, T>{}>::type>
    static void Register()
    {
        const ModuleType moduleType = T::GetTypeStatic();
        const std::string fileExtension = T::GetFileExtensionStatic();

        // TODO: Check for file extension clashes here.
        // In order to make modules fully dynamically loaded, would need to make ModuleType an int and 
        // assign it to the module here rather than let them choose their own ModuleType.
        m_RegistrationMap[moduleType] = &T::CreateStatic;
        m_FileExtensionMap[fileExtension] = moduleType;

        m_ConversionOptionsRegistrationMap[moduleType] = &T::OptionsType::CreateStatic;
        
        //typedef typename T::OptionsType OPT;
        m_AvailableOptionsMap[moduleType] = T::OptionsType::GetAvailableOptionsStatic();
    }

private:

    // Map which registers a module type enum value with the static create function associated with that module
    static std::map<ModuleType, std::function<ModuleBase*(void)>> m_RegistrationMap;

    // File extension to ModuleType map
    static std::map<std::string, ModuleType> m_FileExtensionMap;

    // Map which registers a module type enum value with the static conversion option create function associated with that module
    static std::map<ModuleType, std::function<ConversionOptionsBase*(void)>> m_ConversionOptionsRegistrationMap;

    // Map which maps a module type to the available command-line options for that module type
    static std::map<ModuleType, std::vector<std::string>> m_AvailableOptionsMap;
};
