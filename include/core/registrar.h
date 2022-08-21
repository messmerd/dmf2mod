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

#include "options.h"

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <variant>
#include <type_traits>

namespace d2m {

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
template <typename T> class ConversionOptionsInterface;

// Using's to make usage easier
using Module = ModuleBase;
using ModulePtr = std::shared_ptr<Module>;
using ConversionOptions = ConversionOptionsBase;
using ConversionOptionsPtr = std::shared_ptr<ConversionOptions>;

class ModuleInfo
{
public:
    friend class ModuleBase;
    friend class ConversionOptionsBase;

    ModuleInfo() = default;

    template<class T, class = std::enable_if_t<std::is_base_of_v<ModuleInterface<T, typename T::OptionsType>, T>>>
    static ModuleInfo Create(ModuleType moduleType, std::string friendlyName, std::string fileExtension)
    {
        return ModuleInfo(
            [](){ return std::make_shared<T>(); },
            [](){ return std::make_shared<typename T::OptionsType>(); },
            moduleType,
            friendlyName,
            fileExtension
        );
    }

    ModuleType GetType() const { return m_TypeEnum; }
    std::string GetFriendlyName() const { return m_FriendlyName; }
    std::string GetFileExtension() const { return m_FileExtension; }

private:
    ModuleInfo(std::function<ModulePtr()> createFunc, std::function<ConversionOptionsPtr()> createOptionsFunc, ModuleType moduleType, std::string friendlyName, std::string fileExtension)
        : m_CreateFunc(createFunc), m_CreateOptionsFunc(createOptionsFunc), m_TypeEnum(moduleType), m_FriendlyName(friendlyName), m_FileExtension(fileExtension) {}

    std::function<ModulePtr()> m_CreateFunc = nullptr;
    std::function<ConversionOptionsPtr()> m_CreateOptionsFunc = nullptr;
    ModuleType m_TypeEnum = ModuleType::NONE;
    std::string m_FriendlyName{};
    std::string m_FileExtension{};
};


class ConversionOptionsInfo
{
public:
    friend class ModuleBase;
    friend class ConversionOptionsBase;

    ConversionOptionsInfo() = default;

    template<class T, class = std::enable_if_t<std::is_base_of_v<ConversionOptionsInterface<T>, T>>>
    static ConversionOptionsInfo Create(ModuleType moduleType, std::shared_ptr<OptionDefinitionCollection> definitions)
    {
        return ConversionOptionsInfo([](){ return std::make_shared<T>(); }, moduleType, definitions);
    }

    template<class T, class = std::enable_if_t<std::is_base_of_v<ConversionOptionsInterface<T>, T>>>
    static ConversionOptionsInfo Create(ModuleType moduleType)
    {
        return ConversionOptionsInfo([](){ return std::make_shared<T>(); }, moduleType, std::make_shared<OptionDefinitionCollection>());
    }

    ModuleType GetType() const { return m_TypeEnum; }
    const std::shared_ptr<const OptionDefinitionCollection>& GetDefinitions() const { return m_Definitions; }

private:
    ConversionOptionsInfo(std::function<ConversionOptionsPtr()> createFunc, ModuleType moduleType, const std::shared_ptr<OptionDefinitionCollection>& definitions)
        : m_CreateFunc(createFunc), m_TypeEnum(moduleType), m_Definitions(definitions) {}

    std::function<ConversionOptionsPtr()> m_CreateFunc = nullptr;
    ModuleType m_TypeEnum = ModuleType::NONE;
    std::shared_ptr<const OptionDefinitionCollection> m_Definitions{};
};


// Handles module registration and creation
class Registrar
{
public:
    // This must be called in main() to register to supported module types with dmf2mod
    static void RegisterModules();

    // Helper methods that utilize module registration information
    static std::vector<ModuleType> GetAvailableModules();
    
    static ModuleType GetTypeFromFilename(const std::string& filename);
    static ModuleType GetTypeFromFileExtension(const std::string& extension);
    static std::string GetExtensionFromType(ModuleType moduleType);

    static const ModuleInfo* GetModuleInfo(ModuleType moduleType);
    static const ConversionOptionsInfo* GetConversionOptionsInfo(ModuleType moduleType);
    static std::shared_ptr<const OptionDefinitionCollection> GetOptionDefinitions(ModuleType moduleType);

private:
    /*
     * Registers a module in the registration maps
     * TODO: Need to also check whether the ConversionOptionsStatic<T> specialization exists?
     */
    template <class T, class = std::enable_if_t<std::is_base_of_v<ModuleInterface<T, typename T::OptionsType>, T>>>
    static void Register();

private:

    static std::map<ModuleType, const ModuleInfo*> m_ModuleRegistrationMap;
    static std::map<ModuleType, const ConversionOptionsInfo*> m_ConversionOptionsRegistrationMap;

    // File extension to ModuleType map (could use m_ModuleRegistrationMap but this should be more efficient)
    static std::map<std::string, ModuleType> m_FileExtensionMap;
};

} // namespace d2m
