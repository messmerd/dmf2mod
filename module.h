#include <string>

enum class ModuleType
{
    NONE=0,
    DMF,
    MOD
};

class ModuleUtils
{
public:
    static ModuleType GetType(const char* filename);

};

class Module
{
public:
    static Module* Create(ModuleType type);
    template<typename T> static T* Create();

    virtual bool Load(const char* filename) = 0;
    virtual bool Save(const char* filename) = 0;

    template<typename T> T* Cast() { return reinterpret_cast<T*>(this); }
    virtual ModuleType GetType() = 0;

    virtual std::string GetName() = 0;
};


class Converter
{
public:
    Converter();



private:
    DMF* m_DMF;
};



const char* GetFilenameExt(const char *fname);

