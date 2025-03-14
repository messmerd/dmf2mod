/*
 * factory.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Implementation of the abstract factory pattern.
 */

#pragma once

#include "core/config_types.h"

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
#include <cassert>

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
	// Only the factory can use BuilderBase
	friend class Factory<Base>;

	virtual ~BuilderBase() = default;

protected:
	BuilderBase() = default;
	virtual auto Build() const -> std::shared_ptr<Base> = 0;
};

/**
 * Builds an instance of a factory-enabled class.
 *
 * Derived must have a default constructor accessible by this class and also must have a public destructor.
 * Can specialize this, but it must inherit from BuilderBase and Factory<Base> must have access to its members.
 */
template<class Derived, class Base>
class Builder : public BuilderBase<Base>
{
	// Only the factory can use Builder
	friend class Factory<Base>;

	//! Default constructs an object of Derived wrapped in a shared_ptr
	auto Build() const -> std::shared_ptr<Base> override { return std::shared_ptr<Derived>(new Derived{}); };
};


struct InfoBase
{
	ModuleType type = ModuleType::kNone;
};

/**
 * Static data for a factory-enabled class.
 * Can specialize this, but it must inherit from InfoBase.
 */
template<class T>
struct Info : public InfoBase {};


template<class Base>
class Factory
{
private:
	~Factory() { Clear(); }

	struct InitializeImpl
	{
		// Note: Factories will have to implement this.
		InitializeImpl();
	};

	/*
	 * Initialize must be called at the start of every public Factory method to
	 * ensure lazy initialization of the Factory whenever it is first used.
	 */
	static auto Initialize() -> bool
	{
		if (!initialized_)
		{
			Clear();
			// This gets around static initialization ordering issues:
			[[maybe_unused]] auto init = std::make_unique<InitializeImpl>();
			initialized_ = true;
		}
		return true;
	}

public:
	Factory() = delete;
	Factory(const Factory&) = delete;
	Factory(Factory&&) noexcept = delete;
	auto operator=(const Factory&) -> Factory& = delete;
	auto operator=(Factory&&) noexcept -> Factory& = delete;

	static auto Create(ModuleType module_type) -> std::shared_ptr<Base>
	{
		[[maybe_unused]] static bool init = Initialize();
		if (builders_.find(module_type) != builders_.end()) { return builders_.at(module_type)->Build(); }
		assert(false && "Factory is not initialized for Base.");
		return nullptr;
	}

	static auto GetInfo(ModuleType module_type) -> const Info<Base>*
	{
		[[maybe_unused]] static bool init = Initialize();
		const auto it = info_.find(module_type);
		if (it != info_.end()) { return it->second.get(); }
		assert(false && "Factory is not initialized for Base.");
		return nullptr;
	}

	template<class Derived, std::enable_if_t<detail::factory_enabled_v<Derived>, bool> = true>
	static auto Create() -> std::shared_ptr<Derived>
	{
		// Initialize() not needed here because GetEnumFromType calls it
		static ModuleType module_type = GetEnumFromType<Derived>();
		return std::static_pointer_cast<Derived>(Create(module_type));
	}

	template<class Derived, std::enable_if_t<detail::factory_enabled_v<Derived>, bool> = true>
	static auto GetInfo() -> const Info<Base>*
	{
		// Initialize() not needed here because GetEnumFromType calls it
		static ModuleType module_type = GetEnumFromType<Derived>();
		return GetInfo(module_type);
	}

	static auto TypeInfo() -> const std::map<ModuleType, std::unique_ptr<const Info<Base>>>&
	{
		[[maybe_unused]] static bool init = Initialize();
		return info_;
	}

	static auto GetInitializedTypes() -> std::vector<ModuleType>
	{
		[[maybe_unused]] static bool init = Initialize();
		std::vector<ModuleType> vec;
		for (const auto& map_pair : builders_)
		{
			vec.push_back(map_pair.first);
		}
		return vec;
	}

	template<class Type, std::enable_if_t<detail::factory_enabled_v<Type>, bool> = true>
	static auto GetEnumFromType() -> ModuleType
	{
		[[maybe_unused]] static bool init = Initialize();
		const auto& type = std::type_index(typeid(Type));
		const auto it = type_to_enum_.find(type);
		if (it != type_to_enum_.end()) { return it->second; }
		assert(false && "Factory is not initialized for Type.");
		return ModuleType::kNone;
	}

private:
	template<ModuleType module_type, class Type>
	static void Register()
	{
		static_assert(detail::factory_enabled_v<Type>,
			"Cannot register a class which does not inherit from EnableFactory");
		static_assert(std::is_base_of_v<InfoBase, Info<Base>>, "Info<Base> must derive from InfoBase");
		static_assert(std::is_base_of_v<BuilderBase<Base>, Builder<Type, Base>>,
			"Builder<Derived, Base> must derive from BuilderBase<Base>");

		builders_[module_type] = std::make_unique<const Builder<Type, Base>>();
		auto temp = std::make_unique<Info<Base>>();
		temp->type = module_type;
		info_[module_type] = std::move(temp);
		type_to_enum_[std::type_index(typeid(Type))] = module_type;
	}

	template<class Type>
	static void Register(Info<Base>&& info)
	{
		static_assert(detail::factory_enabled_v<Type>,
			"Cannot register a class which does not inherit from EnableFactory");
		static_assert(std::is_base_of_v<InfoBase, Info<Base>>, "Info<Base> must derive from InfoBase");
		static_assert(std::is_base_of_v<BuilderBase<Base>, Builder<Type, Base>>,
			"Builder<Derived, Base> must derive from BuilderBase<Base>");

		const ModuleType module_type = info.type;

		builders_[module_type] = std::make_unique<const Builder<Type, Base>>();
		info_[module_type] = std::make_unique<const Info<Base>>(std::move(info));
		type_to_enum_[std::type_index(typeid(Type))] = module_type;
	}

	template<ModuleType module_type, class Type, typename... Args>
	static void Register(Args&&... info_args)
	{
		Register<Type>(Info<Base>{InfoBase{module_type}, std::forward<Args>(info_args)...});
	}

	static void Clear()
	{
		builders_.clear();
		info_.clear();
		initialized_ = false;
	}

	static std::map<ModuleType, std::unique_ptr<const BuilderBase<Base>>> builders_;
	static std::map<ModuleType, std::unique_ptr<const Info<Base>>> info_;
	static std::map<std::type_index, ModuleType> type_to_enum_;
	static bool initialized_;
};


// Initialize static members
template<class Base> std::map<ModuleType, std::unique_ptr<const BuilderBase<Base>>> Factory<Base>::builders_{};
template<class Base> std::map<ModuleType, std::unique_ptr<const Info<Base>>> Factory<Base>::info_{};
template<class Base> std::map<std::type_index, ModuleType> Factory<Base>::type_to_enum_{};
template<class Base> bool Factory<Base>::initialized_ = false;


// If a base inherits this using CRTP, derived classes will have knowledge about themselves after Factory initialization
template<class Base>
struct EnableReflection : public detail::EnableReflectionBase
{
	virtual auto GetType() const -> ModuleType = 0;
	virtual auto GetInfo() const -> const Info<Base>* = 0;
};


/**
 * If a base class inherits from EnableReflection, EnableReflection will declare pure virtual methods which must be implemented
 * in a derived class. One of the derived classes will be EnableFactory. EnableFactory must know whether to implement the pure
 * virtual methods or not, and this depends on whether the base class inherits from EnableReflection. In order to conditionally
 * implement these methods, EnableFactory uses std::conditional_t to inherit from either Base or ReflectionImpl<Derived, Base>.
 * ReflectionImpl<Derived, Base> implements the pure virtual methods.
 */
template<class Derived, class Base>
struct ReflectionImpl : public Base
{
	auto GetType() const -> ModuleType final
	{
		static const ModuleType module_type = Factory<Base>::template GetEnumFromType<Derived>();
		return module_type;
	}

	auto GetInfo() const -> const Info<Base>* final
	{
		return Factory<Base>::GetInfo(GetType());
	}
};


//! Inherit this class using CRTP to enable factory for any class
template<class Derived, class Base>
struct EnableFactory
	: public detail::EnableFactoryBase
	, public std::conditional_t<detail::reflection_enabled_v<Base>, ReflectionImpl<Derived, Base>, Base>
{
	static_assert(std::is_base_of_v<InfoBase, Info<Derived>>, "Info<Derived> must inherit from InfoBase");
	static_assert(std::is_base_of_v<BuilderBase<Base>, Builder<Derived, Base>>,
		"Builder<Derived, Base> must inherit from BuilderBase<Base>");
};

} // namespace d2m
