#pragma once

#include <string>
#include <map>
#include <functional>

// Forward declarations
enum class ModuleType;
class Module;
class ModuleUtils;
struct ConversionOptions;

// Helper macro for setting a module class's info
#define REGISTER_MODULE(moduleClass, enumType, fileExt) \
template<> ModuleType ModuleStatic<moduleClass>::_Type = enumType; \
template<> std::string ModuleStatic<moduleClass>::_FileExtension = fileExt;

// CRTP so each class derived from Module can have its own static type variable and static creation
template<typename T>
class ModuleStatic
{
protected:
    friend class ModuleUtils;

    static ModuleType _Type;
    static Module* CreateStatic() { return new T; }
    static std::string _FileExtension; // Without dot
};


// Class containing miscellaneous Module-related static methods
class ModuleUtils
{
public:
    static void RegisterModules();
    static ModuleType GetType(const char* filename);
    static ConversionOptions ParseArgs(char *argv[]);

private:
    friend class Module;

    /*
     * Registers a module in the registration maps
     */
    template <class T, 
        class = typename std::enable_if<
            std::is_base_of<Module, T>{} && 
            std::is_base_of<ModuleStatic<T>, T>{}
            >::type>
    static void Register()
    {
        //static_assert(std::is_base_of<ModuleStatic, T>::value);
        ModuleUtils::RegistrationMap[ModuleStatic<T>::_Type] = &ModuleStatic<T>::CreateStatic;
        ModuleUtils::FileExtensionMap[T::_FileExtension] = T::_Type;
    }

    // Map which registers a module type enum value with the static create function associated with that module
    static std::map<ModuleType, std::function<Module*(void)>> RegistrationMap;

    // File extension to ModuleType map
    static std::map<std::string, ModuleType> FileExtensionMap;
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
     * Returns the ModuleType enum value associated with the specified module type
     */
    template <class T, 
        class = typename std::enable_if<
            std::is_base_of<Module, T>{} && 
            std::is_base_of<ModuleStatic<T>, T>{}
            >::type>
    static ModuleType Type()
    {
        // Type<T>() is enabled only if T is a derived class of both Module and ModuleStatic<T>
        return ModuleStatic<T>::_Type;
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
    template<typename T> T* Cast() { return reinterpret_cast<T*>(this); }
    
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


struct ConversionOptions
{
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

const char* GetFilenameExt(const char *filename);
