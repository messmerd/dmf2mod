#pragma once

#include <string>

enum class ModuleType; // Forward declaration

class ModuleUtils
{
public:
    static ModuleType GetType(const char* filename);

};

// CRTP so each class derived from Module can have its own static type variable
template<typename T>
class ModuleStatic
{
public:
    static T* CreateStatic() { return new T; }
    const static ModuleType _Type;
};

class Module
{
public:
    virtual ~Module() {};

    template<typename T>
    static T* Create(ModuleType type);
    
    // Create<T> is enabled only if T is a derived class of Module and ModuleStatic<T>
    template <class T, 
        class = typename std::enable_if<
            std::is_base_of<Module, T>{} && 
            std::is_base_of<ModuleStatic<T>, T>{}
            >::type>
    static T* Create();

    // GetType<T> is enabled only if T is a derived class of Module and ModuleStatic<T>
    template <class T, 
        class = typename std::enable_if<
            std::is_base_of<Module, T>{} && 
            std::is_base_of<ModuleStatic<T>, T>{}
            >::type>
    static ModuleType Type();


    virtual bool Load(const char* filename) = 0;
    virtual bool Save(const char* filename) = 0;

    template<typename T> T* Cast() { return reinterpret_cast<T*>(this); }
    virtual ModuleType GetType() = 0;

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

const char* GetFilenameExt(const char *fname);

