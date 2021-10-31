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


struct CommonFlags
{
    bool force = false;
    bool silent = false;
    // More to be added later
};


class Status
{
public:

    // Common error codes are defined here
    // Module-specific error codes can be implemented using positive values
    enum class ConvertError : int
    {
        Success=0,
        InvalidArgument=-1,
        UnsupportedInputType=-2,
    };

    static constexpr int RegistrationError = -666;

public:
    Status()
    {
        Clear();
        m_ErrorMessageCreator = nullptr;
    }

    bool ErrorOccurred() const { return m_ErrorCode != 0; }
    bool Failed() const { return ErrorOccurred(); }
    int GetLastErrorCode() const { return m_ErrorCode; }
    
    template <class T, 
        class = typename std::enable_if<
        (std::is_enum<T>{} || std::is_integral<T>{}) &&
        (!std::is_enum<T>{} || std::is_convertible<std::underlying_type_t<T>, int>{})
        >::type>
    void SetError(T errorCode, const std::string errorMessage = "")
    {
        m_ErrorCode = static_cast<int>(errorCode);
        if (m_ErrorCode > 0)
        {
            if (m_ErrorMessageCreator)
                m_ErrorMessage = m_ErrorMessageCreator(m_ErrorCode, errorMessage);
            else
                m_ErrorMessage = errorMessage;
        }
        else
        {
            // TODO: Create an error message creator for common errors
            m_ErrorMessage = errorMessage;
        }
    }

    void PrintError();

    bool WarningsIssued() const { return !m_WarningMessages.empty(); }
    void AddWarning(const std::string& warningMessage)
    {
        m_WarningMessages.push_back(warningMessage);
    }
    void PrintWarnings();

    void Clear()
    {
        m_ErrorCode = 0;
        m_ErrorMessage.clear();
        m_WarningMessages.clear();
    }

    operator bool() const { return ErrorOccurred(); }

    void SetErrorMessageCreator(const std::function<std::string(int, const std::string&)>& func)
    {
        m_ErrorMessageCreator = func;
    }

private:
    int m_ErrorCode;
    std::string m_ErrorMessage;

    std::vector<std::string> m_WarningMessages;

    // Creates module-specific error message from 
    std::function<std::string(int, const std::string&)> m_ErrorMessageCreator;
};


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
    static std::vector<std::string> GetAvaliableModules();
    static bool ParseArgs(int argc, char *argv[], InputOutput& inputOutputInfo, ConversionOptions& options);
    static CommonFlags GetCoreOptions();
    static void SetCoreOptions(CommonFlags& options) { m_CoreOptions = options; };

    static ModuleType GetTypeFromFilename(const std::string& filename);
    static ModuleType GetTypeFromFileExtension(const std::string& extension);
    static std::string GetExtensionFromType(ModuleType moduleType);
    static std::string GetBaseNameFromFilename(const std::string& filename);
    static std::string ReplaceFileExtension(const std::string& filename, const std::string& newFileExtension);
    
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

    // Core conversion options
    static CommonFlags m_CoreOptions;
};


// Base class for all module types (DMF, MOD, XM, etc.)
class ModuleBase
{
public:
    virtual ~ModuleBase() {};

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

    /*
     * Converts the module to the specified type using the provided conversion options
     */
    Module Convert(ModuleType type, ConversionOptions& options)
    {
        // TODO: Check if type is same as current type. If so, create a copy?
        Module output = Module::Create(type);
        if (!output)
            return output;
        output.Get()->ConvertFrom(this, options);
        return output;
    }

    bool ErrorOccurred() const { return m_Status.ErrorOccurred(); }

    Status GetStatus() const { return m_Status; }

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

protected:
    friend class Module;

    virtual bool ConvertFrom(const ModuleBase* input, ConversionOptions& options) = 0;

    Status m_Status;
};


// ModuleBase wrapper so that the user doesn't have to work with ModuleBase* and remember to delete it
class Module
{
public:
    Module()
    {
        m_Module = nullptr;
    };

    Module(Module &&other) : m_Module(std::move(other.m_Module))
    {
        other.m_Module.reset();
    }
    
    Module& operator=(Module&& other)
    {
        if (this != &other)
        {
            m_Module = std::move(other.m_Module);
        }
        return *this;
    }

    ~Module() { m_Module.reset(); }

private:
    Module(ModuleBase* module)
    {
        m_Module = std::unique_ptr<ModuleBase>(module);
    }

public:
    operator bool() const { return m_Module.get(); }

    /*
     * Create a new module using the ModuleType enum to specify the desired module type
     * If the resulting Module object evaluates to false or Get() == nullptr, the module type is 
     * probably not registered
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
     * Create and load a new module given a filename. Module type is inferred from the file extension.
     * Returns
     */
    static Module CreateAndLoad(const std::string& filename)
    {
        const ModuleType type = ModuleUtils::GetTypeFromFilename(filename);
        Module m = Module::Create(type);
        if (!m)
            return m;
        if (m.Load(filename))
            return m;
        return m;
    }

    /*
     * Converts the module to the specified type using the provided conversion options
     */
    Module Convert(ModuleType type, ConversionOptions& options)
    {
        if (!m_Module)
            return Module();
        return m_Module->Convert(type, options);
    }

    bool ErrorOccurred() const
    {
        if (!m_Module)
            return true;
        return m_Module->m_Status.ErrorOccurred();
    }

    Status GetStatus() const
    {
        if (!m_Module)
        {
            Status s;
            s.SetError(Status::RegistrationError);
            return s;
        }
        return m_Module->m_Status;
    }

    /*
     * Get the pointer to the module this class wraps
     */
    const ModuleBase* Get() const { return m_Module.get(); }

    /*
     * Get the pointer to the module this class wraps
     */
    ModuleBase* Get() { return m_Module.get(); }

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
     * Cast an options pointer to a pointer of a derived type
     */
    template <class T>
    const T* Cast() const
    {
        const T* ptr = reinterpret_cast<const T*>(this);
        static_assert(ptr);
        return ptr;
    }

    /*
     * Get a ModuleType enum value representing the type of the conversion option's module
     */
    virtual ModuleType GetType() const = 0;

    /*
     * Get the filename of the output file. Returns empty string if error occurred.
     */
    std::string GetOutputFilename() const { return m_OutputFile; }

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
    ConversionOptions(ConversionOptions &&other) : m_Options(std::move(other.m_Options))
    {
        other.m_Options.reset();
    }

    ConversionOptions& operator=(ConversionOptions&& other)
    {
        if (this != &other)
        {
            m_Options = std::move(other.m_Options);
        }
        return *this;
    }

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
     * If the resulting ConversionOptions object evaluates to false or Get() == nullptr, the module type 
     * is probably not registered
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

    /*
     * Get the filename of the output file. Returns empty string if error occurred.
     */
    std::string GetOutputFilename() const
    {
        if (!m_Options)
            return "";
        return m_Options->GetOutputFilename();
    }

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
