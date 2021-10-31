#pragma once

//#include "error.h"

#include <string>
#include <map>
#include <functional>
#include <vector>
#include <memory>

#define DMF2MOD_VERSION "0.1"

// Forward declarations
enum class ModuleType;
class Module;
class ModuleBase;
class ModuleUtils;
class ConversionOptions;
class ConversionOptionsBase;
struct InputOutput;


// Helper macro for setting a module class's info
#define REGISTER_MODULE(moduleClass, optionsClass, enumType, fileExt) \
template<> const ModuleType ModuleStatic<moduleClass>::_Type = enumType; \
template<> const std::string ModuleStatic<moduleClass>::_FileExtension = fileExt; \
template<> const std::function<ConversionOptionsBase*(void)> ModuleStatic<moduleClass>::_CreateConversionOptionsStatic = &ConversionOptionsStatic<optionsClass>::CreateStatic; \
template<> const ModuleType ConversionOptionsStatic<optionsClass>::_Type = enumType;


// CRTP so each class derived from Module can have its own static type variable and static creation
template<typename T>
class ModuleStatic
{
public:
    friend class ModuleUtils;
    friend class ModuleBase;
    friend class ConversionOptionsBase;

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
    static ModuleBase* CreateStatic() { return new T; }
    const static std::function<ConversionOptionsBase*(void)> _CreateConversionOptionsStatic;
    const static std::string _FileExtension; // Without dot
};


// CRTP so each class derived from ConversionOptions can have its own static creation
template <typename T>
class ConversionOptionsStatic
{
public:
    static ConversionOptionsBase* CreateStatic() { return new T; }

protected:
    friend class ModuleUtils;
    friend class ModuleBase;
    ConversionOptionsStatic() {};
    
    const static ModuleType _Type;
    static ModuleType GetOutputType() { return _Type; }
};


// Class containing miscellaneous Module-related static methods
class ModuleUtils
{
public:
    static void RegisterModules();
    static bool ParseArgs(int argc, char *argv[], InputOutput& inputOutputInfo, ConversionOptions& options);

    static ModuleType GetTypeFromFilename(const std::string& filename);
    static ModuleType GetTypeFromFileExtension(const std::string& extension);
    static std::string GetExtensionFromType(ModuleType moduleType);
    
private:
    friend class Module;
    //friend class ModuleBase;
    friend class ConversionOptions;

    static bool PrintHelp(const std::string& executable, ModuleType moduleType);

    /*
     * Registers a module in the registration maps
     * TODO: Need to also check whether the ConversionOptionsStatic<T> specialization exists
     */
    template <class T, 
        class = typename std::enable_if<
            std::is_base_of<ModuleBase, T>{} && 
            std::is_base_of<ModuleStatic<T>, T>{}
            >::type>
    static void Register()
    {
        // TODO: Check for file extension clashes here.
        // In order to make modules fully dynamically registered, will need to make ModuleType an int and 
        // assign it to the module here rather than let them choose their own ModuleType.
        RegistrationMap[T::_Type] = &T::CreateStatic;
        FileExtensionMap[T::_FileExtension] = T::_Type;

        ConversionOptionsRegistrationMap[T::_Type] = T::_CreateConversionOptionsStatic;
    }

    // Map which registers a module type enum value with the static create function associated with that module
    static std::map<ModuleType, std::function<ModuleBase*(void)>> RegistrationMap;

    // File extension to ModuleType map
    static std::map<std::string, ModuleType> FileExtensionMap;

    // Map which registers a module type enum value with the static conversion option create function associated with that module
    static std::map<ModuleType, std::function<ConversionOptionsBase*(void)>> ConversionOptionsRegistrationMap;
};


// Base class for all module types (DMF, MOD, XM, etc.)
class ModuleBase
{
public:
    friend class Module;

    virtual ~ModuleBase() {};

protected:
    /*
     * Load the specified module file
     * Returns true upon failure
     */
    virtual bool Load(const std::string& filename) = 0;

    /*
     * Save module to the specified file
     * Returns true upon failure
     */
    virtual bool Save(const std::string& filename) = 0;

public:

    /*
     * Cast a Module pointer to a pointer of a derived type
     */
    template <class T, 
        class = typename std::enable_if<
            std::is_base_of<ModuleBase, T>{} && 
            std::is_base_of<ModuleStatic<T>, T>{}
            >::type>
    const T* Cast() const
    {
        const T* ptr = reinterpret_cast<const T*>(this);
        //static_assert(ptr);
        return ptr;
    }
    
    /*
     * Get a ModuleType enum value representing the type of the module
     */
    virtual ModuleType GetType() const = 0;

    /*
     * Get the file extension of the module (does not include dot)
     */
    virtual std::string GetFileExtension() const = 0;

    /*
     * Get the name of the module
     */
    virtual std::string GetName() const = 0;
};


// ModuleBase wrapper so that the user doesn't have to work with ModuleBase* and remember to delete it
class Module
{
public:
    Module() { m_Module = nullptr; };
    ~Module() { m_Module.reset(); }

private:
    Module(ModuleBase* module) { m_Module = std::unique_ptr<ModuleBase>(module); }

public:
    operator bool() const { return m_Module.get(); }

    /*
     * Create a new module using the ModuleType enum to specify the desired module type
     * If the resulting Module object evaluates to true or Get() == nullptr, the module type is 
     * probably not registered
     * TODO: Is Module being copied when returned and is that OK given the unique_ptr member?
     */
    static Module Create(ModuleType type)
    {
        const auto iter = ModuleUtils::RegistrationMap.find(type);
        if (iter != ModuleUtils::RegistrationMap.end())
        {
            // Call ModuleStatic<T>::CreateStatic()
            return Module(iter->second());
        }
        return Module();
    }

    /*
     * Create a new module of the desired module type
     */
    template <class T, 
        class = typename std::enable_if<
            std::is_base_of<ModuleBase, T>{} && 
            std::is_base_of<ModuleStatic<T>, T>{}
            >::type>
    static Module Create()
    {
        // Create<T>() is only enabled if T is a derived class of both Module and ModuleStatic<T>
        return Module(new T);
    }

    /*
     * Get the pointer to the module this class wraps
     */
    const ModuleBase* Get() const { return m_Module.get(); }

    /*
     * Load the specified module file
     * Returns true upon failure
     */
    bool Load(const std::string& filename)
    {
        if (!m_Module)
            return true;
        return m_Module->Load(filename);
    }

    /*
     * Save module to the specified file
     * Returns true upon failure
     */
    bool Save(const std::string& filename)
    {
        if (!m_Module)
            return true;
        return m_Module->Save(filename);
    }

    /*
     * Get a ModuleType enum value representing the type of the module
     */
    ModuleType GetType() const;

    /*
     * Get the file extension of the module (does not include dot)
     */
    std::string GetFileExtension() const
    {
        if (!m_Module)
            return "";
        return m_Module->GetFileExtension();
    }

    /*
     * Get the name of the module
     */
    std::string GetName() const
    {
        if (!m_Module)
            return "";
        return m_Module->GetName();
    }

private:
    std::unique_ptr<ModuleBase> m_Module;
};


// Base class for conversion options
class ConversionOptionsBase
{
public:
    ConversionOptionsBase() { m_OutputFile.clear(); }
    virtual ~ConversionOptionsBase() {};

    /*
     * Get a ModuleType enum value representing the type of the conversion option's module
     */
    virtual ModuleType GetType() const = 0;

    std::string GetOutputFile() const { return m_OutputFile; }

protected:
    friend class ModuleUtils;
    friend class ConversionOptions;

    virtual bool ParseArgs(std::vector<std::string>& args) = 0;
    virtual void PrintHelp() = 0;

protected:
    std::string m_OutputFile;
};


// ConversionOptionsBase wrapper so that the user doesn't have to work with ConversionOptionsBase* and remember to delete it
class ConversionOptions
{
public:
    ConversionOptions() { m_Options = nullptr; };
    ~ConversionOptions() { m_Options.reset(); }

private:
    ConversionOptions(ConversionOptionsBase* options) { m_Options = std::unique_ptr<ConversionOptionsBase>(options); }

public:
    operator bool() const { return m_Options.get(); }

    /*
     * Create a new ConversionOptions object for the desired module type
     */
    template <class moduleClass, 
        class = typename std::enable_if<
            std::is_base_of<Module, moduleClass>{} && 
            std::is_base_of<ModuleStatic<moduleClass>, moduleClass>{}
            >::type>
    static ConversionOptions Create()
    {
        // Create<T>() is only enabled if T is a derived class of both Module and ModuleStatic<T>
        return ConversionOptions(ModuleStatic<moduleClass>::_CreateConversionOptionsStatic());
    }

    /*
     * Create a new module using the ModuleType enum to specify the desired module type
     * If the resulting ConversionOptions object evaluates to true or Get() == nullptr, the module type 
     * is probably not registered
     * TODO: Is ConversionOptions being copied when returned and is that OK given the unique_ptr member?
     */
    static ConversionOptions Create(ModuleType type)
    {
        const auto iter = ModuleUtils::ConversionOptionsRegistrationMap.find(type);
        if (iter != ModuleUtils::ConversionOptionsRegistrationMap.end())
        {
            // Call ConversionOptionsStatic<T>::CreateStatic()
            return ConversionOptions(iter->second());
        }
        return ConversionOptions();
    }

    /*
     * Get the pointer to the options this class wraps
     */
    const ConversionOptionsBase* Get() const { return m_Options.get(); }

    /*
     * Get a ModuleType enum value representing the type of the conversion option's module
     */
    ModuleType GetType() const;

private:
    friend class ModuleUtils;

    bool ParseArgs(std::vector<std::string>& args)
    {
        if (!m_Options)
            return true;
        return m_Options->ParseArgs(args);
    }

    void PrintHelp()
    {
        if (!m_Options)
            return;
        m_Options->PrintHelp();
    }

    std::unique_ptr<ConversionOptionsBase>& GetUnique()
    {
        return m_Options;
    }

    void Set(ConversionOptionsBase* options)
    {
        m_Options.reset();
        if (options)
            m_Options = std::unique_ptr<ConversionOptionsBase>(options);
    }

    void MoveFrom(ConversionOptions& options)
    {
        m_Options = std::move(options.m_Options);
        options.Set(nullptr);
    }

private:
    std::unique_ptr<ConversionOptionsBase> m_Options;
};


// Used for returning input/output info when parsing command-line arguments
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
