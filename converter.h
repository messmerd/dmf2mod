#pragma once

#include <string>
#include <map>
#include <functional>
#include <vector>

#define DMF2MOD_VERSION "0.1"

// Forward declarations
enum class ModuleType;
class Module;
class ModuleUtils;
class ConversionOptions;
struct InputOutput;

// Helper macro for setting a module class's info
#define REGISTER_MODULE(moduleClass, optionsClass, enumType, fileExt) \
template<> const ModuleType ModuleStatic<moduleClass>::_Type = enumType; \
template<> const std::string ModuleStatic<moduleClass>::_FileExtension = fileExt; \
template<> const std::function<ConversionOptions*(void)> ModuleStatic<moduleClass>::_CreateConversionOptionsStatic = &ConversionOptionsStatic<optionsClass>::CreateStatic; \
template<> const ModuleType ConversionOptionsStatic<optionsClass>::_Type = enumType;


// CRTP so each class derived from Module can have its own static type variable and static creation
template<typename T>
class ModuleStatic
{
public:
    friend class ModuleUtils;
    friend class Module;
    friend class ConversionOptions;

    /*
     * Returns the ModuleType enum value
     */
    static ModuleType Type() { return _Type; }

    /*
     * Returns the file name extension (not including the dot)
     */
    static std::string FileExtension() { return _FileExtension; }

protected:
    ModuleStatic() {};
    
    const static ModuleType _Type;
    static Module* CreateStatic() { return new T; }
    const static std::function<ConversionOptions*(void)> _CreateConversionOptionsStatic;
    const static std::string _FileExtension; // Without dot
};


// CRTP so each class derived from ConversionOptions can have its own static creation
template <typename T>
class ConversionOptionsStatic
{
public:
    static ConversionOptions* CreateStatic() { return new T; }

protected:
    friend class ModuleUtils;
    friend class Module;
    ConversionOptionsStatic() {};
    
    const static ModuleType _Type;
    static ModuleType GetOutputType() { return _Type; }
};


// Class containing miscellaneous Module-related static methods
class ModuleUtils
{
public:
    static void RegisterModules();
    static bool ParseArgs(int argc, char *argv[], InputOutput& inputOutputInfo, ConversionOptions*& options);

    static ModuleType GetTypeFromFilename(const std::string& filename);
    static ModuleType GetTypeFromFileExtension(const std::string& extension);
    static std::string GetExtensionFromType(ModuleType moduleType);
    
private:
    friend class Module;
    friend class ConversionOptions;

    static bool PrintHelp(const std::string& executable, ModuleType moduleType);

    /*
     * Registers a module in the registration maps
     * TODO: Need to also check whether the ConversionOptionsStatic<T> specialization exists
     */
    template <class T, 
        class = typename std::enable_if<
            std::is_base_of<Module, T>{} && 
            std::is_base_of<ModuleStatic<T>, T>{}
            >::type>
    static void Register()
    {
        RegistrationMap[T::_Type] = &T::CreateStatic;
        FileExtensionMap[T::_FileExtension] = T::_Type;

        ConversionOptionsRegistrationMap[T::_Type] = T::_CreateConversionOptionsStatic;
    }

    // Map which registers a module type enum value with the static create function associated with that module
    static std::map<ModuleType, std::function<Module*(void)>> RegistrationMap;

    // File extension to ModuleType map
    static std::map<std::string, ModuleType> FileExtensionMap;

    // Map which registers a module type enum value with the static conversion option create function associated with that module
    static std::map<ModuleType, std::function<ConversionOptions*(void)>> ConversionOptionsRegistrationMap;
};


// Base class for all module types (DMF, MOD, XM, etc.)
class Module
{
public:
    virtual ~Module() {};

    /*
     * Dynamically create a new module using the ModuleType enum to specify the desired module type
     * If nullptr is returned, the module type is probably not registered
     */
    static Module* Create(ModuleType type)
    {
        const auto iter = ModuleUtils::RegistrationMap.find(type);
        if (iter != ModuleUtils::RegistrationMap.end())
        {
            // Call ModuleStatic<T>::CreateStatic()
            return iter->second();
        }
        return nullptr;
    }

    /*
     * Dynamically create a new module of the desired module type
     */
    template <class T, 
        class = typename std::enable_if<
            std::is_base_of<Module, T>{} && 
            std::is_base_of<ModuleStatic<T>, T>{}
            >::type>
    static T* Create()
    {
        // Create<T>() is only enabled if T is a derived class of both Module and ModuleStatic<T>
        return new T;
    }

    /*
     * Load the specified module file
     * Returns true upon failure
     */
    virtual bool Load(const char* filename) = 0;

    /*
     * Save module to the specified file
     * Returns true upon failure
     */
    virtual bool Save(const char* filename) = 0;

    /*
     * Cast a Module pointer to a pointer of a derived type
     */
    template <class T, 
        class = typename std::enable_if<
            std::is_base_of<Module, T>{} && 
            std::is_base_of<ModuleStatic<T>, T>{}
            >::type>
    T* Cast() 
    {
        T* ptr = reinterpret_cast<T*>(this);
        //static_assert(ptr);
        return ptr;
    }
    
    /*
     * Get a ModuleType enum value representing the type of the module
     */
    virtual ModuleType GetType() = 0;

    /*
     * Get the file extension of the module (does not include dot)
     */
    virtual std::string GetFileExtension() = 0;

    /*
     * Get the name of the module
     */
    virtual std::string GetName() = 0;
};


class ConversionOptions
{
public:

    virtual ~ConversionOptions() {};

    /*
     * Dynamically create a new ConversionOptions object for the desired module type
     */
    template <class moduleClass, 
        class = typename std::enable_if<
            std::is_base_of<Module, moduleClass>{} && 
            std::is_base_of<ModuleStatic<moduleClass>, moduleClass>{}
            >::type>
    static ConversionOptions* Create()
    {
        // Create<T>() is only enabled if T is a derived class of both Module and ModuleStatic<T>
        return ModuleStatic<moduleClass>::_CreateConversionOptionsStatic();
    }

    /*
     * Dynamically create a new module using the ModuleType enum to specify the desired module type
     * If nullptr is returned, the module type is probably not registered
     */
    static ConversionOptions* Create(ModuleType type)
    {
        const auto iter = ModuleUtils::ConversionOptionsRegistrationMap.find(type);
        if (iter != ModuleUtils::ConversionOptionsRegistrationMap.end())
        {
            // Call ConversionOptionsStatic<T>::CreateStatic()
            return iter->second();
        }
        return nullptr;
    }

    /*
     * Get a ModuleType enum value representing the type of the conversion option's module
     */
    virtual ModuleType GetType() = 0;

    std::string OutputFile = "";

protected:
    friend class ModuleUtils;

    virtual bool ParseArgs(std::vector<std::string>& args) = 0;
    virtual void PrintHelp() = 0;
};


struct InputOutput
{
    std::string InputFile;
    ModuleType InputType;
    std::string OutputFile;
    ModuleType OutputType;
};

/*
class Converter
{
public:
    Converter();



private:
    DMF* m_DMF;
};
*/

bool FileExists(const std::string& filename);
std::string GetFileExtension(const std::string& filename);
