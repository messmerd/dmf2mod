#pragma once

#include <string>
#include <map>
#include <functional>

// Forward declarations
enum class ModuleType;
class Module;

extern std::map<ModuleType, std::function<Module*(void)>> G_ModuleTypeCreateFunctionMap;

// Class containing miscellaneous Module-related static methods
class ModuleUtils
{
public:
    static ModuleType GetType(const char* filename);

};

// CRTP so each class derived from Module can have its own static type variable and static creation
template<typename T>
class ModuleStatic
{
public:
    static Module* CreateStatic() { return new T; }
    const static ModuleType _Type;
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
        const auto iter = G_ModuleTypeCreateFunctionMap.find(type);
        if (iter != G_ModuleTypeCreateFunctionMap.end())
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
     * Get the name of the module
     */
    virtual std::string GetName() = 0;
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
