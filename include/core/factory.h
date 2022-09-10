/*
    factory.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Implementation of the factory pattern.
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
#include <stdexcept>
#include <typeindex>
#include <utility>

namespace d2m {

// Forward declares
template<class T> class Factory;

namespace detail {
    struct EnableFactoryBase {};
    struct EnableReflectionBase {};

    // With C++20 concepts, these won't be needed
    template<class T> constexpr bool factory_enabled_v = std::is_base_of_v<EnableFactoryBase, T>;
    template<class T> constexpr bool reflection_enabled_v = std::is_base_of_v<EnableReflectionBase, T>;
} // namespace detail


template <class BaseType>
struct BuilderBase
{
    virtual ~BuilderBase() = default;
    virtual std::shared_ptr<BaseType> Build() const = 0;
};

// Builds an instance of a factory-enabled class; Can specialize this, but it must inherit from BuilderBase.
template <class Derived, class Base>
struct Builder : public BuilderBase<Base>
{
    std::shared_ptr<Base> Build() const override { return std::make_shared<Derived>(); };
};


struct InfoBase
{
    TypeEnum type = TypeInvalid;
};

// Static data for a factory-enabled class; Can specialize this, but it must inherit from InfoBase.
template <class T>
struct Info : public InfoBase {};


template <class Base>
class Factory
{
private:

    Factory() = delete;
    virtual ~Factory() { Clear(); }

    Factory(const Factory&) = delete;
    Factory(Factory&&) = delete;
    Factory& operator=(const Factory&) = delete;
    Factory& operator=(Factory&&) = delete;

    // Declaration. Factories will have to implement their own.
    static void InitializeImpl();

public:

    static void Initialize()
    {
        if (m_Initialized)
            return;

        InitializeImpl();
        m_Initialized = true;
    }

    static std::shared_ptr<Base> Create(TypeEnum classType)
    {
        if (!m_Initialized)
            throw std::runtime_error("Factory is not initialized for base class: " + std::string(typeid(Base).name()));
        if (m_Builders.find(classType) != m_Builders.end())
            return m_Builders.at(classType)->Build();
        throw std::runtime_error("Factory is not initialized for TypeEnum '" + std::to_string(static_cast<int>(classType)) + ".");
        return nullptr;
    }

    static Info<Base> const* GetInfo(TypeEnum classType)
    {
        if (!m_Initialized)
            throw std::runtime_error("Factory is not initialized for base class: " + std::string(typeid(Base).name()));
        if (m_Info.find(classType) != m_Info.end())
            return m_Info.at(classType).get();
        throw std::runtime_error("Factory is not initialized for TypeEnum '" + std::to_string(static_cast<int>(classType)) + ".");
        return nullptr;
    }

    template <class Derived, class = std::enable_if_t<detail::factory_enabled_v<Derived>>>
    static std::shared_ptr<Derived> Create()
    {
        if (!m_Initialized)
            throw std::runtime_error("Factory is not initialized for base class: " + std::string(typeid(Base).name()));
        static TypeEnum classType = GetEnumFromType<Derived>();
        return std::static_pointer_cast<Derived>(Create(classType));
    }

    template <class Derived, class = std::enable_if_t<detail::factory_enabled_v<Derived>>>
    static Info<Base> const* GetInfo()
    {
        if (!m_Initialized)
            throw std::runtime_error("Factory is not initialized for base class: " + std::string(typeid(Base).name()));
        static TypeEnum classType = GetEnumFromType<Derived>();
        return GetInfo(classType);
    }

    static const std::map<TypeEnum, std::unique_ptr<const Info<Base>>>& TypeInfo() { return m_Info; }

    static std::vector<TypeEnum> GetInitializedTypes()
    {
        std::vector<ModuleType> vec;
        for (const auto& mapPair : m_Builders)
        {
            vec.push_back(mapPair.first);
        }
        return vec;
    }

    // TODO: Use factory_enabled_v
    template<class Type>
    static TypeEnum GetEnumFromType()
    {
        if (!m_Initialized)
            throw std::runtime_error("Factory is not initialized for base class: " + std::string(typeid(Base).name()));
        const auto& type = std::type_index(typeid(Type));
        if (m_TypeToEnum.find(type) != m_TypeToEnum.end())
            return m_TypeToEnum.at(type);
        throw std::runtime_error("Factory is not initialized for Type '" + std::string(typeid(Type).name()) + ".");
    }

private:

    template<TypeEnum eT, class T>
    static void Register()
    {
        static_assert(detail::factory_enabled_v<T>, "Cannot register a class which does not inherit from EnableFactory");
        static_assert(std::is_base_of_v<InfoBase, Info<Base>>, "Info<Base> must derive from InfoBase");
        static_assert(std::is_base_of_v<BuilderBase<Base>, Builder<T, Base>>, "Builder<Derived, Base> must derive from BuilderBase<Base>");

        m_Builders[eT] = std::make_unique<const Builder<T, Base>>();
        auto temp = std::make_unique<Info<Base>>();
        temp->type = eT;
        m_Info[eT] = std::move(temp);
        m_TypeToEnum[std::type_index(typeid(T))] = eT;
    }

    template<class T>
    static void Register(Info<Base>&& info)
    {
        static_assert(detail::factory_enabled_v<T>, "Cannot register a class which does not inherit from EnableFactory");
        static_assert(std::is_base_of_v<InfoBase, Info<Base>>, "Info<Base> must derive from InfoBase");
        static_assert(std::is_base_of_v<BuilderBase<Base>, Builder<T, Base>>, "Builder<Derived, Base> must derive from BuilderBase<Base>");

        const TypeEnum eT = info.type;

        m_Builders[eT] = std::make_unique<const Builder<T, Base>>();
        m_Info[eT] = std::make_unique<const Info<Base>>(std::move(info));
        m_TypeToEnum[std::type_index(typeid(T))] = eT;
    }

    template<class T, typename... Args>
    static void Register(Args&&... infoArgs)
    {
        Register<T>(Info<Base>{ std::forward<Args>(infoArgs)... });
    }

    static void Clear()
    {
        m_Builders.clear();
        m_Info.clear();
        m_Initialized = false;
    }

private:

    static std::map<TypeEnum, std::unique_ptr<const BuilderBase<Base>>> m_Builders;
    static std::map<TypeEnum, std::unique_ptr<const Info<Base>>> m_Info;
    static std::map<std::type_index, TypeEnum> m_TypeToEnum;
    static bool m_Initialized;
};


// Initialize static members
template<class Base> std::map<ModuleType, std::unique_ptr<const BuilderBase<Base>>> Factory<Base>::m_Builders{};
template<class Base> std::map<ModuleType, std::unique_ptr<const Info<Base>>> Factory<Base>::m_Info{};
template<class Base> std::map<std::type_index, ModuleType> Factory<Base>::m_TypeToEnum{};
template<class Base> bool Factory<Base>::m_Initialized = false;


// If a base inherits this using CRTP, derived classes will have knowledge about themselves after Factory initialization
template<class Base>
struct EnableReflection : public detail::EnableReflectionBase
{
    virtual TypeEnum GetType() const = 0;
    virtual Info<Base> const* GetInfo() const = 0;
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
    TypeEnum GetType() const override
    {
        static TypeEnum classType = Factory<Base>::template GetEnumFromType<Derived>();
        return classType;
    }
    Info<Base> const* GetInfo() const override
    {
        return Factory<Base>::GetInfo(GetType());
    }
};


// Inherit this class using CRTP to enable factory for any class
template <class Derived, class Base>
class EnableFactory : public detail::EnableFactoryBase, public std::conditional_t<detail::reflection_enabled_v<Base>, ReflectionImpl<Derived, Base>, Base> // See note above
{
    static_assert(std::is_base_of_v<InfoBase, Info<Derived>>, "Info<Derived> must inherit from InfoBase");
    static_assert(std::is_base_of_v<BuilderBase<Base>, Builder<Derived, Base>>, "Builder<Derived, Base> must inherit from BuilderBase<Base>");
};


} // namespace d2m
