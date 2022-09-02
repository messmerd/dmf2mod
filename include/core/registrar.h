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
#include <typeindex>
#include <utility>

#include <iostream> // temporary

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
    template<class T> constexpr bool factory_enabled_v = std::is_base_of_v<EnableFactoryBase, T>;
    template<class T> constexpr bool subfactories_enabled_v = std::is_base_of_v<EnableSubfactoriesBase, T>;
    template<class T> constexpr bool reflection_enabled_v = std::is_base_of_v<EnableReflectionBase, T>;
} // namespace detail


template <class...Sub>
struct EnableSubfactories : public detail::EnableSubfactoriesBase
{
    using subclasses_t = std::tuple<Sub...>;
};


template <class BaseType>
struct BuilderBase
{
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

    // If Base defines subclasses, subclasses_t = the subclasses, else subclasses_t = void.
    using subclasses_t = detail::get_subclasses_or_void<Base>;
    static void InitializeImpl(); // Declaration. Factories will have to implement their own.

public:

    static void Initialize()
    {
        if (m_Initialized)
            return;

        InitializeImpl();

        if constexpr (detail::subfactories_enabled_v<Base>)
        {
            // Calls Initialize for each class in subclasses_t
            InitializeSubclasses<std::make_index_sequence<std::tuple_size_v<subclasses_t>>{}>();
        }

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

    template<size_t SubclassIndex>
    static auto CreateSubclass(TypeEnum classType)
    {
        if (!m_Initialized)
            throw std::runtime_error("Factory is not initialized for base class: " + std::string(typeid(Base).name()));
        static_assert(detail::subfactories_enabled_v<Base>, "Base class must inherit from EnableSubfactories");
        static_assert(SubclassIndex < std::tuple_size_v<subclasses_t>, "Out of range of subclasses_t tuple");
        return Factory<std::tuple_element_t<SubclassIndex, subclasses_t>>::Create(classType);
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

    template<class T>
    static void Register(const Info<Base>& info)
    {
        auto temp = info;
        Register<T>(std::move(temp));
    }

    static void Clear()
    {
        m_Builders.clear();
        m_Info.clear();
        m_Initialized = false;
    }

    template<std::size_t...Is>
    static auto InitializeSubclasses()
    {
        ( ( Factory<std::tuple_element_t<Is, subclasses_t>>::Initialize()), ... );
    }

private:

    static std::map<TypeEnum, std::unique_ptr<const BuilderBase<Base>>> m_Builders; // NOTE: m_Builders can map to a non-template class instead if there are any compilation issues
    static std::map<TypeEnum, std::unique_ptr<const Info<Base>>> m_Info;            // NOTE: m_Info can map to InfoBase const* instead if there are any compilation issues
    static std::map<std::type_index, TypeEnum> m_TypeToEnum;
    static bool m_Initialized;
};


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
