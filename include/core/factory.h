/*
    factory.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Implementation of the abstract factory pattern.
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
#include <stdexcept>
#include <typeindex>
#include <utility>

namespace d2m {

// Forward declares
template<class T> class Factory;

namespace detail {
    struct EnableFactoryBase {};
    struct EnableReflectionBase {};

    template<class T> constexpr bool factory_enabled_v = std::is_base_of_v<EnableFactoryBase, T>;
    template<class T> constexpr bool reflection_enabled_v = std::is_base_of_v<EnableReflectionBase, T>;
} // namespace detail

template<class Base>
class BuilderBase
{
public:
    friend class Factory<Base>; // Only the factory can use BuilderBase
    virtual ~BuilderBase() = default;
protected:
    BuilderBase() = default;
    virtual std::shared_ptr<Base> Build() const = 0;
};

// Builds an instance of a factory-enabled class; Can specialize this, but it must inherit from BuilderBase and Factory<Base> must have access to its members.
template<class Derived, class Base>
class Builder : public BuilderBase<Base>
{
    // NOTE: Derived must have a default constructor accessible by this class and also must have a public destructor.

    friend class Factory<Base>; // Only the factory can use Builder

    // Default constructs an object of Derived wrapped in a shared_ptr. std::make_shared cannot be used for classes with a non-public constructor even if Builder is a friend.
    std::shared_ptr<Base> Build() const override { return std::shared_ptr<Derived>(new Derived{}); };
};


struct InfoBase
{
    TypeEnum type = TypeInvalid;
};

// Static data for a factory-enabled class; Can specialize this, but it must inherit from InfoBase.
template<class T>
struct Info : public InfoBase {};


template<class Base>
class Factory
{
private:

    Factory() = delete;
    ~Factory() { Clear(); }

    Factory(const Factory&) = delete;
    Factory(Factory&&) = delete;
    Factory& operator=(const Factory&) = delete;
    Factory& operator=(Factory&&) = delete;

    struct InitializeImpl
    {
        // Note: Factories will have to implement this.
        InitializeImpl();
    };

    /*
     * Initialize must be called at the start of every public Factory method to
     * ensure lazy initialization of the Factory whenever it is first used.
     */
    static bool Initialize()
    {
        if (!m_Initialized)
        {
            Clear();
            // This gets around static initialization ordering issues:
            [[maybe_unused]] auto init = std::make_unique<InitializeImpl>();
            m_Initialized = true;
        }
        return true;
    }

public:

    static std::shared_ptr<Base> Create(TypeEnum classType)
    {
        [[maybe_unused]] static bool init = Initialize();
        if (m_Builders.find(classType) != m_Builders.end())
            return m_Builders.at(classType)->Build();
        assert(false && "Factory is not initialized for Base.");
        return nullptr;
    }

    static Info<Base> const* GetInfo(TypeEnum classType)
    {
        [[maybe_unused]] static bool init = Initialize();
        if (m_Info.find(classType) != m_Info.end())
            return m_Info.at(classType).get();
        assert(false && "Factory is not initialized for Base.");
        return nullptr;
    }

    template<class Derived, std::enable_if_t<detail::factory_enabled_v<Derived>, bool> = true>
    static std::shared_ptr<Derived> Create()
    {
        // Initialize() not needed here because GetEnumFromType calls it
        static TypeEnum classType = GetEnumFromType<Derived>();
        return std::static_pointer_cast<Derived>(Create(classType));
    }

    template<class Derived, std::enable_if_t<detail::factory_enabled_v<Derived>, bool> = true>
    static Info<Base> const* GetInfo()
    {
        // Initialize() not needed here because GetEnumFromType calls it
        static TypeEnum classType = GetEnumFromType<Derived>();
        return GetInfo(classType);
    }

    static const std::map<TypeEnum, std::unique_ptr<const Info<Base>>>& TypeInfo()
    {
        [[maybe_unused]] static bool init = Initialize();
        return m_Info;
    }

    static std::vector<TypeEnum> GetInitializedTypes()
    {
        [[maybe_unused]] static bool init = Initialize();
        std::vector<ModuleType> vec;
        for (const auto& mapPair : m_Builders)
        {
            vec.push_back(mapPair.first);
        }
        return vec;
    }

    template<class Type, std::enable_if_t<detail::factory_enabled_v<Type>, bool> = true>
    static TypeEnum GetEnumFromType()
    {
        [[maybe_unused]] static bool init = Initialize();
        const auto& type = std::type_index(typeid(Type));
        if (m_TypeToEnum.find(type) != m_TypeToEnum.end())
            return m_TypeToEnum.at(type);
        assert(false && "Factory is not initialized for Type.");
        return TypeInvalid;
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

    template<TypeEnum eT, class T, typename... Args>
    static inline void Register(Args&&... infoArgs)
    {
        Register<T>(Info<Base>{ InfoBase{ eT }, std::forward<Args>(infoArgs)... });
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
 * If a base class inherits from EnableReflection, EnableReflection will declare pure virtual methods which must be implemented
 * in a derived class. One of the derived classes will be EnableFactory. EnableFactory must know whether to implement the pure
 * virtual methods or not, and this depends on whether the base class inherits from EnableReflection. In order to conditionally
 * implement these methods, EnableFactory uses std::conditional_t to inherit from either Base or ReflectionImpl<Derived, Base>.
 * ReflectionImpl<Derived, Base> implements the pure virtual methods.
 */
template<class Derived, class Base>
struct ReflectionImpl : public Base
{
    TypeEnum GetType() const final override
    {
        static const TypeEnum classType = Factory<Base>::template GetEnumFromType<Derived>();
        return classType;
    }
    Info<Base> const* GetInfo() const final override
    {
        return Factory<Base>::GetInfo(GetType());
    }
};


// Inherit this class using CRTP to enable factory for any class
template<class Derived, class Base>
struct EnableFactory : public detail::EnableFactoryBase, public std::conditional_t<detail::reflection_enabled_v<Base>, ReflectionImpl<Derived, Base>, Base> // See note above
{
    static_assert(std::is_base_of_v<InfoBase, Info<Derived>>, "Info<Derived> must inherit from InfoBase");
    static_assert(std::is_base_of_v<BuilderBase<Base>, Builder<Derived, Base>>, "Builder<Derived, Base> must inherit from BuilderBase<Base>");
};

} // namespace d2m
