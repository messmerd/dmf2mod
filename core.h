/*
    core.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares various classes used by dmf2mod.

    Everything in core.h/core.cpp is written to be 
    module-independent. All supported modules must 
    implement and register the ModuleInterface abstract 
    class found here.
*/

#pragma once

#include <string>
#include <map>
#include <functional>
#include <vector>
#include <memory>

#define DMF2MOD_VERSION "0.11"

// Forward declarations
enum class ModuleType;
class ModuleUtils;
class ModuleBase;
class ConversionOptionsBase;
template <typename T, typename O> class ModuleInterface;
template <typename T> class ConversionOptionsInterface;
struct InputOutput;

// Typedefs to make usage easier
typedef ModuleBase Module;
typedef std::unique_ptr<Module> ModulePtr;
typedef ConversionOptionsBase ConversionOptions;
typedef std::unique_ptr<ConversionOptions> ConversionOptionsPtr;

/*
    Helper macro for explicit template specialization of a module.
    Must be called in a module's header file BEFORE defining the module's class and its
    conversion options class.
*/
#define REGISTER_MODULE_HEADER(moduleClass, optionsClass) \
template class ModuleStatic<moduleClass>; \
template class ConversionOptionsStatic<optionsClass>; \
template class ModuleInterface<moduleClass, optionsClass>; \
template<> ConversionOptionsBase* ConversionOptionsStatic<optionsClass>::CreateStatic(); \
template<> Module* ModuleStatic<moduleClass>::CreateStatic(); \
template<> ModuleType ModuleStatic<moduleClass>::GetTypeStatic(); \
template<> std::string ModuleStatic<moduleClass>::GetFileExtensionStatic(); \
template<> std::function<ConversionOptionsBase*(void)> ModuleStatic<moduleClass>::GetCreateConversionOptionsStatic(); \
template<> ModuleType ConversionOptionsStatic<optionsClass>::GetTypeStatic(); \
template<> std::vector<std::string> ConversionOptionsStatic<optionsClass>::GetAvailableOptionsStatic(); \
template<> ModuleType ModuleInterface<moduleClass, optionsClass>::GetType() const; \
template<> std::string ModuleInterface<moduleClass, optionsClass>::GetFileExtension() const; \
template<> ModuleType ConversionOptionsInterface<optionsClass>::GetType() const;

/*
    Helper macro for setting static data members and defining template specializations
    of a module's methods.
    Must be called in a module's cpp file AFTER the module's class and its
    conversion options class are defined in the header.
*/
#define REGISTER_MODULE_CPP(moduleClass, optionsClass, enumType, fileExt, availOptions) \
template<> const ModuleType ModuleStatic<moduleClass>::m_Type = enumType; \
template<> const std::string ModuleStatic<moduleClass>::m_FileExtension = fileExt; \
template<> ConversionOptionsBase* ConversionOptionsStatic<optionsClass>::CreateStatic() { return new optionsClass; } \
template<> const std::function<ConversionOptionsBase*(void)> ModuleStatic<moduleClass>::m_CreateConversionOptionsStatic = &ConversionOptionsStatic<optionsClass>::CreateStatic; \
template<> const ModuleType ConversionOptionsStatic<optionsClass>::m_Type = enumType; \
template<> const std::vector<std::string> ConversionOptionsStatic<optionsClass>::m_AvailableOptions = availOptions; \
template<> Module* ModuleStatic<moduleClass>::CreateStatic() { return new moduleClass; } \
template<> ModuleType ModuleStatic<moduleClass>::GetTypeStatic() { return m_Type; } \
template<> std::string ModuleStatic<moduleClass>::GetFileExtensionStatic() { return m_FileExtension; } \
template<> std::function<ConversionOptionsBase*(void)> ModuleStatic<moduleClass>::GetCreateConversionOptionsStatic() { return m_CreateConversionOptionsStatic; } \
template<> ModuleType ConversionOptionsStatic<optionsClass>::GetTypeStatic() { return m_Type; } \
template<> std::vector<std::string> ConversionOptionsStatic<optionsClass>::GetAvailableOptionsStatic() { return m_AvailableOptions; } \
template<> ModuleType ModuleInterface<moduleClass, optionsClass>::GetType() const { return ModuleStatic<moduleClass>::GetTypeStatic(); } \
template<> std::string ModuleInterface<moduleClass, optionsClass>::GetFileExtension() const { return ModuleStatic<moduleClass>::GetFileExtensionStatic(); } \
template<> std::vector<std::string> ConversionOptionsInterface<optionsClass>::GetAvailableOptions() const { return ConversionOptionsStatic<optionsClass>::GetAvailableOptionsStatic(); } \
template<> ModuleType ConversionOptionsInterface<optionsClass>::GetType() const { return ConversionOptionsStatic<optionsClass>::GetTypeStatic(); } \

//template<> typename ModuleInterface<moduleClass, optionsClass>::OptionsType = optionsClass;


// Command-line options that are supported regardless of which modules are supported
struct CommonFlags
{
    bool force = false;
    bool silent = false;
    // More to be added later
};

// Provides error/warning information after module importing/exporting/converting
class Status
{
public:

    // Common error codes are defined here
    // Module-specific error codes can be implemented using positive values
    enum class ImportError
    {
        Success=0
    };

    enum class ExportError
    {
        Success=0,
        FileOpen=-1
    };
    
    enum class ConvertError
    {
        Success=0,
        InvalidArgument=-1,
        UnsupportedInputType=-2
    };

    enum class Category
    {
        Import,
        Export,
        Convert
    };

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
    void SetError(Category category, T errorCode, const std::string errorMessage = "")
    {
        m_ErrorCode = static_cast<int>(errorCode);

        std::string categoryString;
        switch (category)
        {
            case Category::Import:
                categoryString = "Import: "; break;
            case Category::Export:
                categoryString = "Export: "; break;
            case Category::Convert:
                categoryString = "Convert: "; break;
        }

        if (m_ErrorCode > 0)
        {
            if (m_ErrorMessageCreator)
                m_ErrorMessage = "ERROR: " + categoryString + m_ErrorMessageCreator(category, m_ErrorCode, errorMessage);
            else
                m_ErrorMessage = "ERROR: " + categoryString + errorMessage;
        }
        else
        {
            m_ErrorMessage = "ERROR: " + categoryString + CommonErrorMessageCreator(category, m_ErrorCode, errorMessage);
        }
    }

    bool WarningsIssued() const { return !m_WarningMessages.empty(); }
    void AddWarning(const std::string& warningMessage)
    {
        m_WarningMessages.push_back("WARNING: " + warningMessage);
    }
    
    void PrintError();
    void PrintWarnings();
    void PrintAll();

    void Clear()
    {
        m_ErrorCode = 0;
        m_ErrorMessage.clear();
        m_WarningMessages.clear();
    }

    operator bool() const { return ErrorOccurred(); }

    void SetErrorMessageCreator(const std::function<std::string(Category, int, const std::string&)>& func)
    {
        m_ErrorMessageCreator = func;
    }

private:
    int m_ErrorCode;
    std::string m_ErrorMessage;

    std::vector<std::string> m_WarningMessages;

    std::string CommonErrorMessageCreator(Category category, int errorCode, const std::string& arg);

    // Creates module-specific error message from an error code and string argument
    std::function<std::string(Category, int, const std::string&)> m_ErrorMessageCreator;
};


// CRTP so each class derived from Module can have its own static type variable and static creation
template<typename T>
class ModuleStatic
{
protected:
    friend class ModuleUtils;

    // This class needs to be inherited
    ModuleStatic() = default;
    ModuleStatic(const ModuleStatic&) = default;
    ModuleStatic(ModuleStatic&&) = default;

    static Module* CreateStatic();

    static ModuleType GetTypeStatic();
    static std::string GetFileExtensionStatic();
    static std::function<ConversionOptionsBase*(void)> GetCreateConversionOptionsStatic();
    
private:
    const static ModuleType m_Type;
    const static std::string m_FileExtension; // Without dot
    const static std::function<ConversionOptionsBase*(void)> m_CreateConversionOptionsStatic;
};


// CRTP so each class derived from ConversionOptions can have its own static creation
template <typename T>
class ConversionOptionsStatic
{
public:
    // TODO: Figure out how to make this protected or private
    // Returns a list of strings of the format: "-o, --option=[min,max]" or "-a" or "--flag" or "--flag=[]" etc.
    //  representing the command-line options for this module and their acceptable values
    static std::vector<std::string> GetAvailableOptionsStatic();

protected:
    friend class ModuleUtils;

    // This class needs to be inherited
    ConversionOptionsStatic() = default;
    ConversionOptionsStatic(const ConversionOptionsStatic&) = default;
    ConversionOptionsStatic(ConversionOptionsStatic&&) = default;

    // The output module type
    static ModuleType GetTypeStatic();

public:
    // Unfortunately with the way I'm doing things currently, this needs to be public:
    static ConversionOptionsBase* CreateStatic();

private:
    const static ModuleType m_Type;
    const static std::vector<std::string> m_AvailableOptions;
};


// Class containing miscellaneous Module-related static methods; Also handles module registration
class ModuleUtils
{
public:
    static void RegisterModules();
    static std::vector<std::string> GetAvailableModules();
    static bool ParseArgs(int argc, char *argv[], InputOutput& inputOutputInfo, ConversionOptionsPtr& options);
    static CommonFlags GetCoreOptions() { return m_CoreOptions; }
    static void SetCoreOptions(CommonFlags& options) { m_CoreOptions = options; };

    static ModuleType GetTypeFromFilename(const std::string& filename);
    static ModuleType GetTypeFromFileExtension(const std::string& extension);
    static std::string GetBaseNameFromFilename(const std::string& filename);
    static std::string ReplaceFileExtension(const std::string& filename, const std::string& newFileExtension);
    static std::string GetFileExtension(const std::string& filename);
    static bool FileExists(const std::string& filename);
    
private:
    friend class ModuleBase;
    friend class ConversionOptionsBase;

    static std::string GetExtensionFromType(ModuleType moduleType);
    static std::vector<std::string> GetAvailableOptions(ModuleType moduleType);
    
    static bool PrintHelp(const std::string& executable, ModuleType moduleType);

    /*
     * Registers a module in the registration maps
     * TODO: Need to also check whether the ConversionOptionsStatic<T> specialization exists?
     */
    template <class T, 
        class = typename std::enable_if<std::is_base_of<ModuleInterface<T, typename T::OptionsType>, T>{}>::type>
    static void Register()
    {
        // TODO: Check for file extension clashes here.
        // In order to make modules fully dynamically loaded, would need to make ModuleType an int and 
        // assign it to the module here rather than let them choose their own ModuleType.
        RegistrationMap[T::GetTypeStatic()] = &T::CreateStatic;
        FileExtensionMap[T::GetFileExtensionStatic()] = T::GetTypeStatic();

        ConversionOptionsRegistrationMap[T::GetTypeStatic()] = T::GetCreateConversionOptionsStatic();
        
        //typedef typename T::OptionsType OPT;
        AvailableOptionsMap[T::GetTypeStatic()] = T::OptionsType::GetAvailableOptionsStatic();
    }

    // Map which registers a module type enum value with the static create function associated with that module
    static std::map<ModuleType, std::function<ModuleBase*(void)>> RegistrationMap;

    // File extension to ModuleType map
    static std::map<std::string, ModuleType> FileExtensionMap;

    // Map which registers a module type enum value with the static conversion option create function associated with that module
    static std::map<ModuleType, std::function<ConversionOptionsBase*(void)>> ConversionOptionsRegistrationMap;

    // Map which maps a module type to the available command-line options for that module type
    static std::map<ModuleType, std::vector<std::string>> AvailableOptionsMap;

    // Core conversion options
    static CommonFlags m_CoreOptions;
};


// Base class for all module types (DMF, MOD, XM, etc.)
class ModuleBase
{
public:
    virtual ~ModuleBase() {};

    /*
     * Create a new module using the ModuleType enum to specify the desired module type
     * If the resulting Module object evaluates to false or Get() == nullptr, the module type is 
     * probably not registered
     */
    static ModulePtr Create(ModuleType type)
    {
        const auto iter = ModuleUtils::RegistrationMap.find(type);
        if (iter != ModuleUtils::RegistrationMap.end())
        {
            // Call ModuleStatic<T>::CreateStatic()
            return ModulePtr(iter->second());
        }
        return nullptr;
    }

    /*
     * Create a new module of the desired module type
     */
    template <class T, 
        class = typename std::enable_if<std::is_base_of<ModuleInterface<T, typename T::OptionsType>, T>{}>::type>
    static ModulePtr Create();

    /*
     * Create and import a new module given a filename. Module type is inferred from the file extension.
     * Returns pointer to the module
     */
    static ModulePtr CreateAndImport(const std::string& filename)
    {
        const ModuleType type = ModuleUtils::GetTypeFromFilename(filename);
        ModulePtr m = Module::Create(type);
        if (!m)
            return nullptr;
        if (m->Import(filename))
            return m;
        return m;
    }

    /*
     * Import the specified module file
     * Returns true upon failure
     */
    virtual bool Import(const std::string& filename) = 0;

    /*
     * Export module to the specified file
     * Returns true upon failure
     */
    virtual bool Export(const std::string& filename) = 0;

    /*
     * Converts the module to the specified type using the provided conversion options
     */
    ModulePtr Convert(ModuleType type, ConversionOptionsPtr& options)
    {
        // Don't convert if the types are the same
        if (type == GetType())
            return nullptr;

        // Create new module object
        ModulePtr output = Module::Create(type);
        if (!output)
            return nullptr;

        // Perform the conversion
        output->ConvertFrom(this, options);
        return output;
    }

    bool ErrorOccurred() const { return m_Status.ErrorOccurred(); }

    Status GetStatus() const { return m_Status; }

    /*
     * Cast a Module pointer to a pointer of a derived type
     */
    template <class T, 
        class = typename std::enable_if<std::is_base_of<ModuleInterface<T, typename T::OptionsType>, T>{}>::type>
    const T* Cast() const
    {
        return reinterpret_cast<const T*>(this);
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
     * Get the file extension of the module of the given type (does not include dot)
     */
    static std::string GetFileExtension(ModuleType moduleType)
    {
        return ModuleUtils::GetExtensionFromType(moduleType);
    }

    /*
     * Get the available command-line options for this module
     */
    virtual std::vector<std::string> GetAvailableOptions() const = 0;

    /*
     * Get the available command-line options for the given module type
     */
    static std::vector<std::string> GetAvailableOptions(ModuleType moduleType)
    {
        return ModuleUtils::GetAvailableOptions(moduleType);
    }

    /*
     * Get the name of the module
     */
    virtual std::string GetName() const = 0;

protected:
    virtual bool ConvertFrom(const Module* input, ConversionOptionsPtr& options) = 0;

    Status m_Status;
};


// Base class for conversion options
class ConversionOptionsBase
{
public:
    ConversionOptionsBase() { m_OutputFile.clear(); }
    virtual ~ConversionOptionsBase() {};

    /*
     * Create a new ConversionOptions object for the desired module type
     */
    template <class moduleClass, 
        class = typename std::enable_if<std::is_base_of<ModuleInterface<moduleClass, typename moduleClass::OptionsType>, moduleClass>{}>::type>
    static ConversionOptionsPtr Create();

    /*
     * Create a new module using the ModuleType enum to specify the desired module type
     * If the resulting ConversionOptions object evaluates to false or Get() == nullptr, the module type 
     * is probably not registered
     */
    static ConversionOptionsPtr Create(ModuleType type)
    {
        const auto iter = ModuleUtils::ConversionOptionsRegistrationMap.find(type);
        if (iter != ModuleUtils::ConversionOptionsRegistrationMap.end())
        {
            // Call ConversionOptionsStatic<T>::CreateStatic()
            return ConversionOptionsPtr(iter->second());
        }
        return nullptr;
    }

    /*
     * Cast an options pointer to a pointer of a derived type
     */
    template <class optionsClass, 
        class = typename std::enable_if<std::is_base_of<ConversionOptionsInterface<optionsClass>, optionsClass>{}>::type>
    const optionsClass* Cast() const
    {
        return reinterpret_cast<const optionsClass*>(this);
    }

    /*
     * Get a ModuleType enum value representing the type of the conversion option's module
     */
    virtual ModuleType GetType() const = 0;

    /*
     * Returns a list of strings of the format: "-o, --option=[min,max]" or "-a" or "--flag" or "--flag=[]" etc.
     *  representing the command-line options for this module and their acceptable values
     */
    virtual std::vector<std::string> GetAvailableOptions() const = 0;

    /*
     * Returns a list of strings of the format: "-o, --option=[min,max]" or "-a" or "--flag" or "--flag=[]" etc.
     *  representing the command-line options and their acceptable values for the given module type
     */
    static std::vector<std::string> GetAvailableOptions(ModuleType moduleType)
    {
        return ModuleUtils::GetAvailableOptions(moduleType);
    }

    /*
     * Get the filename of the output file. Returns empty string if error occurred.
     */
    std::string GetOutputFilename() const { return m_OutputFile; }

    /*
     * Fills in this object's command-line arguments from a list of arguments.
     * Arguments are removed from the list if they are successfully parsed.
     */
    virtual bool ParseArgs(std::vector<std::string>& args) = 0;

protected:
    friend class ModuleUtils;

    virtual void PrintHelp() = 0;

protected:
    std::string m_OutputFile;
};


// All module classes must inherit this
template <typename T, typename O>
class ModuleInterface : public ModuleBase, public ModuleStatic<T>
{
public:
    using OptionsType = O;

protected:
    ModuleType GetType() const override;
    std::string GetFileExtension() const override;
    std::vector<std::string> GetAvailableOptions() const override;
};

// All conversion options classes must inherit this
template <typename T>
class ConversionOptionsInterface : public ConversionOptionsBase, public ConversionOptionsStatic<T>
{
protected:
    ModuleType GetType() const override;
    std::vector<std::string> GetAvailableOptions() const override;
};


// Used for returning input/output info when parsing command-line arguments
struct InputOutput
{
    std::string InputFile;
    ModuleType InputType;
    std::string OutputFile;
    ModuleType OutputType;
};
