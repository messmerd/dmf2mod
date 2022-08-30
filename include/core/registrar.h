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

#include "config_types.h"
#include "options.h"

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <variant>
#include <type_traits>
#include <experimental/type_traits>
#include <exception>

namespace d2m {

// Forward declares
template<class T> class Factory;

namespace detail {

    struct EnableFactoryBase {};
    struct EnableSubfactoriesBase {};
    struct EnableReflectionBase {};

    // With C++20 concepts, these won't be needed
    template<class T> using get_subclasses = typename T::subclasses_t;
    template<class T> using get_subclasses_or_void = std::experimental::detected_or_t<void, detail::get_subclasses, T>;
    template<class T> constexpr bool subfactories_enabled_v = std::is_base_of_v<EnableSubfactoriesBase, T>;
    template<class T> constexpr bool reflection_enabled_v = std::is_base_of_v<EnableReflectionBase, T>;
} // namespace detail


template <class...Sub>
struct EnableSubfactories : public EnableSubfactoriesBase
{
    using subclasses_t = std::tuple<Sub...>;
};


struct BuilderBase {};

template <class BaseType>
struct IBuilder : public BuilderBase
{
    virtual std::shared_ptr<BaseType> Build() const = 0;
};

template <class DerivedType, class BaseType>
struct Builder : public IBuilder<BaseType>
{
    std::shared_ptr<BaseType> Build() const override { return std::make_shared<DerivedType>(); };
};

struct InfoBase
{
    ModuleType moduleType = ModuleType::NONE;
};

// Static data for a factory-enabled class; Can specialize this
template <class T>
struct Info : public InfoBase {};




template <class BaseClass> // BaseClass would be ModuleBase, ConversionOptionsBase, etc.
class Factory
{
private:

    Factory() = delete;
    virtual ~Factory() { Clear(); }

    Factory(const Factory&) = delete;
    Factory(Factory&&) = delete;
    Factory& operator=(const Factory&) = delete;
    Factory& operator=(Factory&&) = delete;

    // If BaseClass defines subclasses, subclasses_t = the subclasses, else subclasses_t = void.
    using subclasses_t = detail::get_subclasses_or_void<BaseClass>;
    static void InitializeImpl(); // Declaration. Registrars will have to define their own.

public:

    static void Initialize()
    {
        std::cout << "In Factory<" << typeid(BaseClass).name() << ">::Initialize()\n";
        if (m_Initialized)
            return;

        InitializeImpl();

        if constexpr (detail::subfactories_enabled_v<BaseClass>)
        {
            std::cout << "--Factory has subclasses\n";
            auto indexes = std::make_index_sequence<std::tuple_size_v<subclasses_t>>{};
            Initialize(indexes); // Calls Initialize for each class in subclasses_t
            std::cout << "--Initialized the subclass factories.\n";
        }
        else
        {
            std::cout << "--Factory<"<< typeid(BaseClass).name() << "> does not have subclasses\n";
        }
        m_Initialized = true;
    }

    static std::shared_ptr<BaseClass> Create(ModuleType moduleType)
    {
        if (!m_Initialized)
            throw std::runtime_error("Factory is not initialized for base class: " + std::string(typeid(BaseClass).name()));
        if (m_Builders.find(moduleType) != m_Builders.end())
            return static_cast<IBuilder<BaseClass> const*>(m_Builders.at(moduleType))->Build();
        throw std::runtime_error("Factory is not initialized for ModuleType '" + std::to_string(static_cast<int>(moduleType)) + ".");
        return nullptr;
    }

    static InfoBase const* GetInfo(ModuleType moduleType)
    {
        if (!m_Initialized)
            throw std::runtime_error("Factory is not initialized for base class: " + std::string(typeid(BaseClass).name()));
        if (m_Info.find(moduleType) != m_Info.end())
            return m_Info.at(moduleType);
        throw std::runtime_error("Factory is not initialized for ModuleType '" + std::to_string(static_cast<int>(moduleType)) + ".");
        return nullptr;
    }

    template <class T, class = std::enable_if_t<std::is_base_of_v<BaseClass, T>>>
    static std::shared_ptr<T> Create()
    {
        return std::make_shared<T>();
    }

    //template <class T>
    //static Info<T> const* GetInfo();

    template<size_t Index>
    static auto CreateSubclass(ModuleType moduleType)
    {
        if (!m_Initialized)
            throw std::runtime_error("Factory is not initialized for base class: " + std::string(typeid(BaseClass).name()));
        static_assert(detail::subfactories_enabled_v<BaseClass>, "Base class must inherit from EnableSubfactories");
        static_assert(Index < std::tuple_size_v<subclasses_t>, "Out of range of subclasses_t tuple");
        return Factory<std::tuple_element_t<Index, subclasses_t>>::Create(moduleType);
    }

    static const std::map<ModuleType, InfoBase const*>& Info() { return m_Info; }

    template<class Type>
    static ModuleType GetEnumFromType()
    {
        const auto& type = typeid(Type);
        if (m_TypeToEnum.find(type) != m_TypeToEnum.end())
            return m_TypeToEnum.at(type);
        throw std::runtime_error("Enum does not exist for type, or Factory has not been initialized");
    }

protected:

    template <ModuleType eT, class T>
    static void Register()
    {
        // TODO: Enable if T inherits from EnableFactory?

        m_Builders[eT] = Builder<T>{};
        m_Info[eT] = d2m::Info<T>{};
        m_Info[eT]->moduleType = eT;
        m_TypeToEnum[typeid(T)] = eT;
    }

    template <ModuleType eT, class T>
    static void Register(d2m::Info<T>&& info)
    {
        // TODO: Enable if T inherits from EnableFactory?

        m_Builders[eT] = Builder<T>{};
        m_Info[eT] = std::move(info);
        m_Info[eT]->moduleType = eT;
        m_TypeToEnum[typeid(T)] = eT;
    }

    static void Clear()
    {
        m_Builders.clear();
        m_Info.clear();
        m_Initialized = false;
    }

private:

    template<std::size_t...Is>
    static auto Initialize(std::index_sequence<Is...>)
    {
        ( ( Factory<std::tuple_element_t<Is, subclasses_t>>::Initialize()), ... );
    }

private:

    static std::map<ModuleType, BuilderBase const*> m_Builders;
    static std::map<ModuleType, InfoBase*> m_Info;
    static std::map<std::type_info, ModuleType> m_TypeToEnum;
    static bool m_Initialized;
};


// If a base inherits this using CRTP, derived classes will have knowledge about themselves after Factory initialization
struct EnableReflection : public detail::EnableReflectionBase
{
    virtual ModuleType GetType() const = 0;
    virtual InfoBase const* GetInfo() const = 0;
};


/*
 * If a base class inherits from EnableReflection, EnableReflection will declare virtual methods which must be implemented
 * in a derived class. One of the derived classes will be EnableFactory. EnableFactory must know whether to implement the
 * virtual methods or not, and this depends on whether the base class inherits from EnableReflection. In order to conditionally
 * implement these methods, EnableFactory uses std::conditional_t to inherit from either Base or ReflectionImpl<Derived, Base>.
 * ReflectionImpl<Derived, Base> implements the virtual methods.
 */
template<class Derived, class Base>
struct ReflectionImpl : public Base
{
    ModuleType GetType() const override
    {
        static ModuleType moduleType = Factory<Base>::GetEnumForType<Derived>();
        return moduleType;
    }
    InfoBase const* GetInfo() const override
    {
        return Factory<Base>::GetInfo(GetType());
    }
};


// Inherit this class using CRTP to enable factory for any class
template <class Derived, class Base>
class EnableFactory : public EnableFactoryBase, public std::conditional_t<detail::reflection_enabled_v<Base>, detail::ReflectionImpl<Derived, Base>, Base> // See note above
{
    static_assert(std::is_base_of_v<InfoBase, Info<Derived>>, "Info<Derived> must inherit from InfoBase");
    static_assert(std::is_base_of_v<IBuilder<Base>, Builder<Derived, Base>>, "Builder<Derived, Base> must inherit from IBuilder<Base>");
};


} // namespace d2m
