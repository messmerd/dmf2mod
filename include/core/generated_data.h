/*
 * generated_data.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines GeneratedData and related class templates
 */

#pragma once

#include "core/note.h"
#include "core/data.h"
#include "core/state.h"
#include "core/module_base.h"

#include <cstddef>
#include <map>
#include <set>
#include <optional>
#include <cassert>

namespace d2m {

namespace detail {

struct GenDataDefinitionTag {};

template<typename T>
struct WrappedGenData {};

template<typename... Ts>
struct WrappedGenData<std::tuple<Ts...>>
{
    // type is either an empty tuple or a tuple with each Ts wrapped in an optional
    using type = std::conditional_t<sizeof...(Ts)==0, std::tuple<>, std::tuple<std::optional<Ts>...>>;
};

template<typename... T>
using WrappedGenDataType = typename WrappedGenData<T...>::type;

} // namespace detail

///////////////////////////////////////////////////////////
// COMMON GENERATED DATA TYPES
///////////////////////////////////////////////////////////

using TotalOrdersGenData = OrderIndex;
using NoteOffUsedGenData = bool;
using ChannelNoteExtremesGenData = std::map<ChannelIndex, std::pair<Note, Note>>;
template<class T> using SoundIndexNoteExtremesGenData = std::map<typename SoundIndex<T>::type, std::pair<Note, Note>>;
template<class T> using SoundIndexesUsedGenData = std::set<typename SoundIndex<T>::type>;
template<class T> using StateGenData = ModuleState<T>;

///////////////////////////////////////////////////////////
// COMMON GENERATED DATA DEFINITION
///////////////////////////////////////////////////////////

template<class T>
struct GeneratedDataCommonDefinition : public detail::GenDataDefinitionTag
{
    static constexpr int kCommonCount = 6; // # of variants in GenDataEnumCommon (remember to update this after changing the enum)
    static constexpr int kLowerBound = -kCommonCount;
    using ModuleClass = T;

    enum GenDataEnumCommon
    {
        //kDuplicateOrders        =-7,
        kTotalOrders            =-6,
        kNoteOffUsed            =-5,
        kChannelNoteExtremes    =-4,
        kSoundIndexNoteExtremes =-3,
        kSoundIndexesUsed       =-2,
        kState                  =-1,
    };

    // Lowest to highest
    using GenDataCommon = std::tuple<
        TotalOrdersGenData, // Gen data's total orders <= data's total orders
        NoteOffUsedGenData,
        ChannelNoteExtremesGenData,
        SoundIndexNoteExtremesGenData<ModuleClass>,
        SoundIndexesUsedGenData<ModuleClass>,
        StateGenData<ModuleClass>
        >;
};

///////////////////////////////////////////////////////////
// GENERATED DATA STORAGE
///////////////////////////////////////////////////////////

namespace detail {

// Compile-time for loop helper
template<int start, class Storage, int... integers>
void ClearAllGenDataHelper(Storage* storage, std::integer_sequence<int, integers...>)
{
    (storage->template Clear<start + integers>(), ...);
}

// Calls GeneratedDataStorage::Clear() for every type of generated data
template<int start, int end, class Storage>
void ClearAllGenData(Storage* storage)
{
    ClearAllGenDataHelper<start>(storage, std::make_integer_sequence<int, detail::abs(start) + end>{});
}

} // namespace detail


template<class CommonDef, typename... Ts>
class GeneratedDataStorage : public CommonDef
{
public:
    static constexpr int kUpperBound = sizeof...(Ts); // # of module-specific gen data types

    using typename CommonDef::GenDataEnumCommon;
    using typename CommonDef::ModuleClass;

    // The GenDataEnum for any module-specific types should be defined
    //  in the GeneratedData template specialization

    using GenDataModuleSpecific = std::tuple<Ts...>;

    // Single tuple of all data types stored by this gen data storage
    using GenData = detail::tuple_cat_t<typename CommonDef::GenDataCommon, GenDataModuleSpecific>;

    // Single tuple of all wrapped data types stored by this gen data storage. They should all be optionals.
    using GenDataWrapped = detail::WrappedGenDataType<GenData>;

    // Returns an immutable reference to generated data at index gen_data_index
    template<int gen_data_index>
    constexpr auto Get() const -> const auto&
    {
        return std::get<gen_data_index + CommonDef::kCommonCount>(data_);
    }

    // Returns a mutable reference to generated data at index gen_data_index
    template<int gen_data_index>
    constexpr auto Get() -> auto&
    {
        return std::get<gen_data_index + CommonDef::kCommonCount>(data_);
    }

    // For convenience:
    constexpr auto GetState() const -> const std::optional<ModuleState<ModuleClass>>& { return Get<GenDataEnumCommon::kState>(); }
    constexpr auto GetState() -> std::optional<ModuleState<ModuleClass>>& { return Get<GenDataEnumCommon::kState>(); }
    constexpr auto GetNumOrders() const -> const std::optional<OrderIndex>& { return Get<GenDataEnumCommon::kTotalOrders>(); }

    // Destroys any generated data at index gen_data_index. Call this after any change which would make the data invalid.
    template<int gen_data_index>
    void Clear()
    {
        auto& data = std::get<gen_data_index + CommonDef::kCommonCount>(data_);
        if (data.has_value())
        {
            data.reset();
            generated_.reset();
            status_ = 0;
        }
    }

    // Destroys all generated data
    void ClearAll()
    {
        detail::ClearAllGenData<-CommonDef::kCommonCount, kUpperBound>(this);
        generated_.reset();
        status_ = 0;
    }

    auto IsValid() const -> bool { return generated_.has_value(); }
    auto GetGenerated() const -> std::optional<std::size_t> { return generated_; }
    void SetGenerated(std::optional<std::size_t> val) { generated_ = val; }
    auto GetStatus() const -> std::size_t { return status_; }
    void SetStatus(std::size_t val) { status_ = val; }

protected:
    GenDataWrapped data_; // Stores all generated data
    std::optional<std::size_t> generated_; // The value passed to GenerateDataImpl. Has a value if gen data is valid.
    std::size_t status_; // The value returned by GenerateDataImpl. Only valid if IsValid() == true.
};

///////////////////////////////////////////////////////////
// GENERATED DATA PRIMARY TEMPLATE
///////////////////////////////////////////////////////////

/*
 * The following is the generated data storage primary class template.
 * It can be specialized to add additional supported generated data if desired.
 * Any specializations must inherit from GeneratedDataStorage and pass the correct
 * common definition struct plus the new module-specific types to the template parameter.
 * In addition, specializations must define GenDataEnumCommon and GenDataEnum.
 * All generated data types must have a "==" operator defined for them.
 */

template<class ModuleClass>
struct GeneratedData : public GeneratedDataStorage<GeneratedDataCommonDefinition<ModuleClass> /* Module-specific types go here in any specializations */>
{
    //using Parent = GeneratedDataStorage<GeneratedDataCommonDefinition<ModuleClass>>;
    using typename GeneratedDataCommonDefinition<ModuleClass>::GenDataEnumCommon;
    enum GenDataEnum {};
};

} // namespace d2m
