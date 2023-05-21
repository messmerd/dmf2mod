/*
 * state.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines GlobalState, ChannelState, StateReader, and ModuleState
 */

#pragma once

#include "core/config_types.h"
#include "core/data.h"
#include "core/note.h"
#include "core/effects.h"

#include <vector>
#include <tuple>
#include <array>
#include <optional>
#include <memory>
#include <cstdint>
#include <type_traits>
#include <functional>
#include <cassert>

namespace d2m {

// Unique, quickly calculated value encoding order # (not pattern #!) and pattern row #. Easily and quickly comparable.
using OrderRowPosition = int32_t;

using GlobalOrderRowPosition = OrderRowPosition;
using ChannelOrderRowPosition = OrderRowPosition;

// Helpers for conversion:
[[nodiscard]] inline constexpr auto GetOrderRowPosition(OrderIndex order, RowIndex row) -> OrderRowPosition { return (order << 16) | row; };
[[nodiscard]] inline constexpr auto GetOrderRowPosition(OrderRowPosition pos) -> std::pair<OrderIndex, RowIndex> { return { pos >> 16, pos & 0x00FF }; };

namespace detail {

struct StateDefinitionTag {};
struct OneShotDefinitionTag {};

// Sourced from: https://stackoverflow.com/a/53398815/8704745
template<typename... Input>
using tuple_cat_t = decltype(std::tuple_cat(std::declval<Input>()...));

template<typename T>
struct WrappedStateData {};

template<typename... Ts>
struct WrappedStateData<std::tuple<Ts...>>
{
    // type is either an empty tuple or a tuple with each Ts wrapped in a vector of pairs
    using type = std::conditional_t<sizeof...(Ts) == 0, std::tuple<>, std::tuple<std::vector<std::pair<OrderRowPosition, Ts>>...>>;
};

template<typename... Ts>
using WrappedStateDataType = typename WrappedStateData<Ts...>::type;

template<typename T>
[[nodiscard]] inline constexpr auto abs(const T x) noexcept -> T
{
    return x == T(0) ? T(0) : (x < T(0) ? -x : x);
}

} // namespace detail

///////////////////////////////////////////////////////////
// COMMON STATE DATA TYPES
///////////////////////////////////////////////////////////

// A unique identifier for wavetables, duty cycles, samples, etc.
// This can be specialized, but an "==" operator must be defined for the type.
template<class ModuleClass>
struct SoundIndex
{
    using type = uint8_t;
};

template<class ModuleClass>
using SoundIndexType = typename SoundIndex<ModuleClass>::type;

using EffectValueXX = uint8_t;
using EffectValueXXYY = uint8_t; //std::pair<uint8_t, uint8_t>;

// Global state data types

using LoopbackStateData = OrderRowPosition; // Order/Row where the PosJump occurred
using TempoStateData = EffectValueXX;
using SpeedAStateData = EffectValueXX;
using SpeedBStateData = EffectValueXX;
using PatBreakStateData = RowIndex;
using PosJumpStateData = OrderIndex;

// Per-channel state data types

using NoteDelayStateData = EffectValueXX;
using NoteCutStateData = EffectValueXX;
using RetriggerStateData = EffectValueXXYY;
using VolSlideStateData = EffectValueXXYY;
using PanningStateData = EffectValueXX;
using TremoloStateData = EffectValueXXYY;
using VibratoVolSlideStateData = EffectValueXXYY;
using Port2NoteVolSlideStateData = EffectValueXXYY;
using VibratoStateData = EffectValueXXYY;

struct PortamentoStateData
{
    enum Type { kNone, kUp, kDown, kToNote } type;
    EffectValueXX value;
};

// All state data types must have an equal-to operator defined for them
inline constexpr auto operator==(const PortamentoStateData& lhs, const PortamentoStateData& rhs) -> bool { return lhs.type == rhs.type && lhs.value == rhs.value; }

using ArpStateData = EffectValueXXYY;
using VolumeStateData = EffectValueXX;
using NotePlayingStateData = bool;
using NoteSlotStateData = NoteSlot;
template<class T> using SoundIndexStateData = typename SoundIndex<T>::type;

///////////////////////////////////////////////////////////
// COMMON STATE DEFINITIONS
///////////////////////////////////////////////////////////

struct GlobalStateCommonDefinition : public detail::StateDefinitionTag
{
    // Common state data have negative indexes
    enum StateEnumCommon
    {
        // Add additional variants here
        kTempo  = -3,
        kSpeedB = -2,
        kSpeedA = -1
        // StateEnum contains values >= 0
    };

    // Lowest to highest
    using StateDataCommon = std::tuple<
        TempoStateData,
        SpeedBStateData,
        SpeedAStateData
        >;

    static constexpr int kCommonCount = std::tuple_size_v<StateDataCommon>;
    static constexpr int kLowerBound = -kCommonCount;
};

struct GlobalOneShotCommonDefinition : public detail::OneShotDefinitionTag
{
    // Common one-shot data have negative indexes
    enum OneShotEnumCommon
    {
        // Add additional variants here
        kLoopback = -3,
        kPatBreak = -2,
        kPosJump  = -1
        // OneShotEnum contains values >= 0
    };

    // Lowest to highest
    using OneShotDataCommon = std::tuple<
        LoopbackStateData,
        PatBreakStateData,
        PosJumpStateData
        >;

    static constexpr int kOneShotCommonCount = std::tuple_size_v<OneShotDataCommon>;
    static constexpr int kOneShotLowerBound = -kOneShotCommonCount;
};

struct ChannelStateCommonDefinition : public detail::StateDefinitionTag
{
    // Common state data have negative indexes
    enum StateEnumCommon
    {
        // Add additional variants here
        kVolSlide          = -11,
        kPanning           = -10,
        kTremolo           = -9,
        kVibratoVolSlide   = -8,
        kPort2NoteVolSlide = -7,
        kVibrato           = -6,
        kPort              = -5, // should this be split into port up, port down, and port2note?
        kArp               = -4,
        kVolume            = -3,
        kNotePlaying       = -2,
        kNoteSlot          = -1
        // StateEnum contains values >= 0
    };

    // Lowest to highest
    using StateDataCommon = std::tuple<
        VolSlideStateData,
        PanningStateData,
        TremoloStateData,
        VibratoVolSlideStateData,
        Port2NoteVolSlideStateData,
        VibratoStateData,
        PortamentoStateData,
        ArpStateData,
        VolumeStateData,
        NotePlayingStateData,
        NoteSlotStateData
        >;

    static constexpr int kCommonCount = std::tuple_size_v<StateDataCommon>;
    static constexpr int kLowerBound = -kCommonCount;
};

struct ChannelOneShotCommonDefinition : public detail::OneShotDefinitionTag
{
    // Common one-shot data have negative indexes
    enum OneShotEnumCommon
    {
        // Add additional variants here
        kNoteDelay = -3,
        kNoteCut   = -2,
        kRetrigger = -1
        // OneShotEnum contains values >= 0
    };

    // Lowest to highest
    using OneShotDataCommon = std::tuple<
        NoteDelayStateData,
        NoteCutStateData,
        RetriggerStateData
        >;

    static constexpr int kOneShotCommonCount = std::tuple_size_v<OneShotDataCommon>;
    static constexpr int kOneShotLowerBound = -kOneShotCommonCount;
};

///////////////////////////////////////////////////////////
// STATE STORAGE
///////////////////////////////////////////////////////////

template<class CommonDef>
class StateStorageCommon : public CommonDef
{
public:
    using typename CommonDef::StateDataCommon;

    // Tuple of all common data types stored by this state, wrapped. They should all be vectors of pairs.
    using StateDataCommonWrapped = detail::WrappedStateDataType<StateDataCommon>;

    virtual ~StateStorageCommon() = default;

    template<int state_data_index, std::enable_if_t<(state_data_index < 0), bool> = true>
    [[nodiscard]] constexpr auto Get2() const -> const auto&
    {
        return std::get<state_data_index + CommonDef::kCommonCount>(common_data_);
    }

    template<int state_data_index, std::enable_if_t<(state_data_index < 0), bool> = true>
    [[nodiscard]] constexpr auto Get2() -> auto&
    {
        return std::get<state_data_index + CommonDef::kCommonCount>(common_data_);
    }

private:
    StateDataCommonWrapped common_data_;
};

template<class CommonDef, typename... Ts>
class StateStorage : public StateStorageCommon<CommonDef>
{
public:
    static constexpr int kUpperBound = sizeof...(Ts); // # of module-specific state data types

    // The StateEnum for any module-specific types should be defined
    //  in the GlobalState/ChannelState template specialization

    using StateDataSpecialized = std::tuple<Ts...>;

    // Tuple of all specialized data types stored by this state, wrapped. They should all be vectors of pairs.
    using StateDataSpecializedWrapped = detail::WrappedStateDataType<StateDataSpecialized>;

    using CombinedStateData = detail::tuple_cat_t<typename StateStorageCommon<CommonDef>::StateDataCommon, StateDataSpecialized>;

    template<int state_data_index>
    [[nodiscard]] constexpr auto Get() const -> const auto&
    {
        if constexpr (state_data_index >= 0) { return std::get<state_data_index>(specialized_data_); }
        else { return StateStorageCommon<CommonDef>::template Get2<state_data_index>(); }
    }

    template<int state_data_index>
    [[nodiscard]] constexpr auto Get() -> auto&
    {
        if constexpr (state_data_index >= 0) { return std::get<state_data_index>(specialized_data_); }
        else { return StateStorageCommon<CommonDef>::template Get2<state_data_index>(); }
    }

private:
    StateDataSpecializedWrapped specialized_data_;
};

template<class CommonDef>
class OneShotStorageCommon : public CommonDef
{
public:
    using typename CommonDef::OneShotDataCommon;

    // Tuple of all common data types stored by this state, wrapped. They should all be vectors of pairs.
    using OneShotDataCommonWrapped = detail::WrappedStateDataType<OneShotDataCommon>;

    virtual ~OneShotStorageCommon() = default;

    template<int oneshot_data_index, std::enable_if_t<(oneshot_data_index < 0), bool> = true>
    [[nodiscard]] constexpr auto GetOneShot2() const -> const auto&
    {
        return std::get<oneshot_data_index + CommonDef::kOneShotCommonCount>(common_oneshot_data_);
    }

    template<int oneshot_data_index, std::enable_if_t<(oneshot_data_index < 0), bool> = true>
    [[nodiscard]] constexpr auto GetOneShot2() -> auto&
    {
        return std::get<oneshot_data_index + CommonDef::kOneShotCommonCount>(common_oneshot_data_);
    }

private:
    OneShotDataCommonWrapped common_oneshot_data_;
};

template<class CommonDef, typename... Ts>
class OneShotStorage : public OneShotStorageCommon<CommonDef>
{
public:
    static constexpr int kOneShotUpperBound = sizeof...(Ts); // # of module-specific one-shot data types

    // The OneShotEnum for any module-specific types should be defined
    //  in the GlobalState/ChannelState template specialization

    using OneShotDataSpecialized = std::tuple<Ts...>;

    // Tuple of all specialized data types stored by this state, wrapped. They should all be vectors of pairs.
    using OneShotDataSpecializedWrapped = detail::WrappedStateDataType<OneShotDataSpecialized>;

    template<int oneshot_data_index>
    [[nodiscard]] constexpr auto GetOneShot() const -> const auto&
    {
        if constexpr (oneshot_data_index >= 0) { return std::get<oneshot_data_index>(specialized_oneshot_data_); }
        else { return OneShotStorageCommon<CommonDef>::template GetOneShot2<oneshot_data_index>(); }
    }

    template<int oneshot_data_index>
    [[nodiscard]] constexpr auto GetOneShot() -> auto&
    {
        if constexpr (oneshot_data_index >= 0) { return std::get<oneshot_data_index>(specialized_oneshot_data_); }
        else { return OneShotStorageCommon<CommonDef>::template GetOneShot2<oneshot_data_index>(); }
    }

private:
    OneShotDataSpecializedWrapped specialized_oneshot_data_;
};

///////////////////////////////////////////////////////////
// GLOBAL/PER-CHANNEL STATE PRIMARY TEMPLATES
///////////////////////////////////////////////////////////

/*
 * The following are the global and per-channel state storage primary class templates.
 * They can be specialized to add additional supported state data if desired.
 * Any specializations must inherit from StateStorage and OneShotStorage and pass the correct
 * common definition structs plus the new module-specific types to the template parameters.
 * In addition, specializations must define StateEnumCommon, StateEnum, OneShotEnumCommon, and
 * OneShotEnum.
 * All state data types must have a "==" operator defined for them.
 */

template<class ModuleClass>
struct GlobalState :
    public StateStorage<GlobalStateCommonDefinition /* Module-specific types go here in any specializations */>,
    public OneShotStorage<GlobalOneShotCommonDefinition /* Module-specific types go here in any specializations */>
{
    using typename GlobalStateCommonDefinition::StateEnumCommon;
    enum StateEnum {};
    using typename GlobalOneShotCommonDefinition::OneShotEnumCommon;
    enum OneShotEnum {};
};

template<class ModuleClass>
struct ChannelState :
    public StateStorage<ChannelStateCommonDefinition, SoundIndexStateData<ModuleClass> /* Module-specific types go here in any specializations */>,
    public OneShotStorage<ChannelOneShotCommonDefinition /* Module-specific types go here in any specializations */>
{
    using typename ChannelStateCommonDefinition::StateEnumCommon;
    enum StateEnum { kSoundIndex };
    using typename ChannelOneShotCommonDefinition::OneShotEnumCommon;
    enum OneShotEnum {};
};

namespace detail {
    enum StateType { kState, kOneShot };

    template<class CommonStorage, StateType state_type>
    inline constexpr int kStorageCommonCount = state_type == kState ? CommonStorage::kCommonCount : CommonStorage::kOneShotCommonCount;

    template<class CommonStorage, StateType state_type>
    using CommonStorageDataType = std::conditional_t<state_type == kState, typename CommonStorage::StateDataCommon, typename CommonStorage::OneShotDataCommon>;
    template<class SpecializedStorage, StateType state_type>
    using SpecializedStorageDataType = std::conditional_t<state_type == kState, typename SpecializedStorage::StateDataSpecialized, typename SpecializedStorage::OneShotDataSpecialized>;

    template<class CommonStorage, StateType state_type>
    using CommonStorageWrappedDataType = std::conditional_t<state_type == kState, typename CommonStorage::StateDataCommonWrapped, typename CommonStorage::OneShotDataCommonWrapped>;
    template<class SpecializedStorage, StateType state_type>
    using SpecializedStorageWrappedDataType = std::conditional_t<state_type == kState, typename SpecializedStorage::StateDataSpecializedWrapped, typename SpecializedStorage::OneShotDataSpecializedWrapped>;

    template<int data_index, class Storage, StateType state_type>
    using GetTypeFromStorage = std::conditional_t<(data_index < 0),
        std::tuple_element_t<static_cast<size_t>(data_index + kStorageCommonCount<Storage, state_type>), CommonStorageDataType<Storage, state_type>>,
        std::tuple_element_t<static_cast<size_t>(data_index), SpecializedStorageDataType<Storage, state_type>>
    >;

    template<int data_index, class Storage, StateType state_type>
    using GetWrappedTypeFromStorage = std::conditional_t<(data_index < 0),
        std::tuple_element_t<data_index + kStorageCommonCount<Storage, state_type>, CommonStorageWrappedDataType<Storage, state_type>>,
        std::tuple_element_t<static_cast<size_t>(data_index), SpecializedStorageWrappedDataType<Storage, state_type>>
    >;

} // namespace detail

///////////////////////////////////////////////////////////
// STATE READER
///////////////////////////////////////////////////////////

namespace detail {

// Compile-time for loop helper
template<int start, class Reader, class Tuple, typename Function, int... integers>
void CopyStateHelper(const Reader* reader, Tuple& t, const Function& f, std::integer_sequence<int, integers...>&&)
{
    (f(std::get<integers>(t), reader->template Get<start + integers>()), ...);
}

// Function F arguments are: (inner data tuple element reference, inner data)
template<int start, int end, class Reader, class Tuple, typename Function>
void CopyState(const Reader* reader, Tuple& t, const Function& f)
{
    CopyStateHelper<start>(reader, t, f, std::make_integer_sequence<int, detail::abs(start) + end>{});
}

// Compile-time for loop helper
template<int start, bool oneshots, class Reader, typename Function, int... integers>
void NextStateHelper(const Reader* reader, const Function& f, std::integer_sequence<int, integers...>&&)
{
    if constexpr (oneshots) { (f(reader->template GetOneShotVec<start + integers>(), start + integers), ...); }
    else { (f(reader->template GetVec<start + integers>(), start + integers), ...); }
}

// Function F arguments are: (wrapped state/one-shot data vector, index)
template<int start, int end, bool oneshots, class Reader, typename Function>
void NextState(const Reader* reader, const Function& f)
{
    NextStateHelper<start, oneshots>(reader, f, std::make_integer_sequence<int, detail::abs(start) + end>{});
}

} // namespace detail


// Allows easy, efficient reading/traversal of GlobalState/ChannelState
template<class StateClass, std::enable_if_t<std::is_base_of_v<detail::StateDefinitionTag, StateClass> && std::is_base_of_v<detail::OneShotDefinitionTag, StateClass>, bool> = true>
class StateReader
{
public:

    // Bring in dependencies:
    using State = StateClass;
    //using StateData = typename State::StateData;
    //using StateDataWrapped = typename State::StateDataWrapped;
    using StateEnumCommon = typename State::StateEnumCommon;
    using StateEnum = typename State::StateEnum;

    //using OneShotData = typename State::OneShotData;
    //using OneShotDataWrapped = typename State::OneShotDataWrapped;
    using OneShotEnumCommon = typename State::OneShotEnumCommon;
    using OneShotEnum = typename State::OneShotEnum;

    // Helpers:
    template<int data_index, detail::StateType state_type>
    using GetType = detail::GetTypeFromStorage<data_index, State, state_type>;
    template<int data_index, detail::StateType state_type>
    using GetWrappedType = detail::GetWrappedTypeFromStorage<data_index, State, state_type>;

    using Deltas = std::array<bool, State::kCommonCount + State::kUpperBound>;
    using OneShotDeltas = std::array<bool, State::kOneShotCommonCount + State::kOneShotUpperBound>;

    StateReader() { Reset(); }
    StateReader(State* state) : state_{state} { Reset(); }
    virtual ~StateReader() = default;

    void AssignState(const State* state) { state_ = state; channel_ = 0; }
    void AssignState(const State* state, ChannelIndex channel) { state_ = state; channel_ = channel; }

    // Set current read position to the beginning of the Module's state data
    virtual void Reset()
    {
        cur_pos_ = 0;
        cur_indexes_.fill(0);
        cur_indexes_oneshot_.fill(0);
        deltas_.fill(false);
        oneshot_deltas_.fill(false);
    }

    // Get the specified state data vector (state_data_index)
    template<int state_data_index>
    [[nodiscard]] inline constexpr auto GetVec() const -> const GetWrappedType<state_data_index, detail::kState>&
    {
        assert(state_);
        return state_->template Get<state_data_index>();
    }

    // Get the specified one-shot data vector (oneshot_data_index)
    template<int oneshot_data_index>
    [[nodiscard]] inline constexpr auto GetOneShotVec() const -> const GetWrappedType<oneshot_data_index, detail::kOneShot>&
    {
        assert(state_);
        return state_->template GetOneShot<oneshot_data_index>();
    }

    // Get the specified state data (state_data_index) at the current read position
    template<int state_data_index>
    [[nodiscard]] inline constexpr auto Get() const -> const GetType<state_data_index, detail::kState>&
    {
        const int vec_index = cur_indexes_[GetIndex(state_data_index)];
        const auto& vec = GetVec<state_data_index>();
        assert(!vec.empty() && "The initial state must be set before reading");
        return vec.at(vec_index).second;
    }

    // Get the specified state data (state_data_index) at the specified read index (vec_index) within the vector
    template<int state_data_index>
    [[nodiscard]] inline constexpr auto Get(size_t vec_index) const -> const GetType<state_data_index, detail::kState>&
    {
        return GetVec<state_data_index>().at(vec_index).second;
    }

    // Get the specified one-shot data (oneshot_data_index) at the current read position. Only valid if GetOneShotDelta() returned true.
    template<int oneshot_data_index>
    [[nodiscard]] inline constexpr auto GetOneShot() const -> const GetType<oneshot_data_index, detail::kOneShot>&
    {
        const int vec_index = cur_indexes_oneshot_[GetOneShotIndex(oneshot_data_index)];
        assert(vec_index > 0 && "Only call GetOneShot() if GetOneShotDelta() returned true");
        return GetOneShotVec<oneshot_data_index>().at(vec_index - 1).second;
    }

    // Gets the specified state data (state_data_index) if it is exactly at the current read position.
    // TODO: This makes a copy. Try using std::reference_wrapper
    template<int state_data_index>
    [[nodiscard]] inline constexpr auto GetImpulse() const -> std::optional<GetType<state_data_index, detail::kState>>
    {
        const auto& vec = GetVec<state_data_index>();
        assert(!vec.empty() && "The initial state must be set before reading");

        const int vec_index = cur_indexes_[GetIndex(state_data_index)];
        const auto& elem = vec.at(vec_index);
        if (elem.first != cur_pos_) { return std::nullopt; }

        return elem.second;
    }

    // Returns a tuple of all the state values at the current read position
    [[nodiscard]] auto Copy() const -> typename State::CombinedStateData
    {
        typename State::CombinedStateData return_val;
        detail::CopyState<State::kLowerBound, State::kUpperBound>(this, return_val,
            [](auto& return_val_elem, const auto& val) constexpr { return_val_elem = val; });
        return return_val;
    }

    /*
     * Advances the read position to the next row in the state data if needed; pos should be the current position.
     * Call this method at the start of an inner loop before any the reading has been done for that iteration.
     * It can also be used to seek forward to the specified position even if it's not the next row.
     * If set_deltas == true, sets an array of bools specifying which state values have changed since last iteration.
     * These delta values can then be obtained by calling GetDeltas() or GetOneShotDeltas().
     */
    template<bool set_deltas = true>
    void SetReadPos(OrderRowPosition pos)
    {
        cur_pos_ = pos;
        if constexpr (set_deltas)
        {
            deltas_.fill(false);
            oneshot_deltas_.fill(false);
        }

        detail::NextState<State::kLowerBound, State::kUpperBound, false>(this, [&, this](const auto& vec, int state_data_index) constexpr
        {
            const int vec_size = static_cast<int>(vec.size());
            if (vec_size == 0) { return; } // No state data for data type state_data_index

            int& index = cur_indexes_[GetIndex(state_data_index)]; // Current index within state data

            // While there's a next state that we need to advance to
            while (index + 1 != vec_size && cur_pos_ >= vec.at(index+1).first)
            {
                // Need to advance
                ++index;

                if constexpr (set_deltas)
                {
                    // NOTE: If Set() is called with ignore_duplicates == true, delta could be true even if nothing changed.
                    deltas_[GetIndex(state_data_index)] = true;
                }
            }
        });

        detail::NextState<State::kOneShotLowerBound, State::kOneShotUpperBound, true>(this, [&, this](const auto& vec, int oneshot_data_index) constexpr
        {
            const int vec_size = static_cast<int>(vec.size());
            if (vec_size == 0) { return; } // No one-shot data for data type oneshot_data_index

            int& index = cur_indexes_oneshot_[GetOneShotIndex(oneshot_data_index)]; // Current index within one-shot data
            if (index == vec_size) { return; }

            // While we need to advance the state
            while (cur_pos_ >= vec.at(index).first)
            {
                if constexpr (set_deltas)
                {
                    // If we've reached the position of the current one-shot data
                    if (cur_pos_ == vec.at(index).first)
                    {
                        oneshot_deltas_[GetOneShotIndex(oneshot_data_index)] = true;
                    }
                }

                // Advance to next index
                ++index;

                // Break if there is no more one-shot data left to read
                if (index == vec_size) { break; }
            }
        });
    }

    /*
     * Advances the read position to the next row in the state data if needed; pos should be the current position.
     * Call this method at the start of an inner loop before any the reading has been done for that iteration.
     * It can also be used to seek forward to the specified position even if it's not the next row.
     * If set_deltas == true, sets an array of bools specifying which state values have changed since last iteration.
     * These delta values can then be obtained by calling GetDeltas() or GetOneShotDeltas().
     */
    template<bool set_deltas = true>
    inline void SetReadPos(OrderIndex order, RowIndex row)
    {
        SetReadPos<set_deltas>(GetOrderRowPosition(order, row));
    }

    // Returns state data at given position, then restores position to what it was previously
    [[nodiscard]] auto ReadAt(OrderRowPosition pos) -> typename State::CombinedStateData
    {
        const OrderRowPosition cur_pos_temp = cur_pos_;
        const auto deltas_temp = deltas_;
        const auto oneshot_deltas_temp = oneshot_deltas_;
        const auto cur_indexes_temp = cur_indexes_;
        const auto cur_indexes_oneshot_temp = cur_indexes_oneshot_;

        Reset();
        SetReadPos(pos);
        auto state_data = Copy();

        cur_pos_ = cur_pos_temp;
        deltas_ = deltas_temp;
        oneshot_deltas_ = oneshot_deltas_temp;
        cur_indexes_ = cur_indexes_temp;
        cur_indexes_oneshot_ = cur_indexes_oneshot_temp;

        return state_data;
    }

    // Returns state data at given position, then restores position to what it was previously
    [[nodiscard]] inline auto ReadAt(OrderIndex order, RowIndex row) -> typename State::CombinedStateData
    {
        return ReadAt(GetOrderRowPosition(order, row));
    }

    // Get the size of the specified state data vector (state_data_index)
    template<int state_data_index>
    [[nodiscard]] inline constexpr auto GetSize() const -> size_t
    {
        return GetVec<state_data_index>().size();
    }

    // Returns the deltas from the last SetReadPos<true>() call
    [[nodiscard]] inline constexpr auto GetDeltas() const -> const Deltas& { return deltas_; }

    [[nodiscard]] inline constexpr auto GetDelta(int state_data_index) const -> bool { return deltas_[GetIndex(state_data_index)]; }

    // Returns the one-shot deltas from the last SetReadPos<true>() call
    [[nodiscard]] inline constexpr auto GetOneShotDeltas() const -> const OneShotDeltas& { return oneshot_deltas_; }

    [[nodiscard]] inline constexpr auto GetOneShotDelta(int oneshot_data_index) const -> bool { return oneshot_deltas_[GetOneShotIndex(oneshot_data_index)]; }

    // Only useful for ChannelStateReader
    [[nodiscard]] inline auto GetChannel() const -> ChannelIndex { return channel_; }

    // Gets a desired value from CombinedStateData
    template<int state_data_index>
    [[nodiscard]] static constexpr auto GetValue(const typename State::CombinedStateData& data) -> GetType<state_data_index, detail::kState>
    {
        return std::get<GetIndex(state_data_index)>(data);
    }

    template<int state_data_index>
    [[nodiscard]] constexpr auto Find(std::function<bool(const GetType<state_data_index, detail::kState>&)> cmp) const -> std::optional<std::pair<OrderRowPosition, GetType<state_data_index, detail::kState>>>
    {
        const auto& vec = GetVec<state_data_index>();
        assert(!vec.empty() && "The initial state must be set before reading");

        int vec_index = cur_indexes_[GetIndex(state_data_index)];
        for (; vec_index < static_cast<int>(vec.size()); ++vec_index)
        {
            const auto& elem = vec.at(vec_index);
            if (cmp(elem.second)) { return elem; }
        }

        return std::nullopt;
    }

protected:

    // Converts StateEnumCommon or StateEnum variants into a zero-based index of an array. Returns offset if no enum is provided.
    [[nodiscard]] static inline constexpr auto GetIndex(int state_data_index = 0) -> int { return State::kCommonCount + state_data_index; }

    // Converts OneShotEnumCommon or OneShotEnum variants into a zero-based index of an array. Returns offset if no enum is provided.
    [[nodiscard]] static inline constexpr auto GetOneShotIndex(int oneshot_data_index = 0) -> int { return State::kOneShotCommonCount + oneshot_data_index; }

    const State* state_ = nullptr; // The state this reader is reading from
    Deltas deltas_; // An array of bools indicating which (if any) state data values have changed since the last SetReadPos<true>() call
    OneShotDeltas oneshot_deltas_; // Same as deltas_ but for one-shots
    OrderRowPosition cur_pos_; // The current read position in terms of order and pattern row. (The write position is the end of the state data vector)
    std::array<int, State::kCommonCount + State::kUpperBound> cur_indexes_; // array of state data vector indexes
    std::array<int, State::kOneShotCommonCount + State::kOneShotUpperBound> cur_indexes_oneshot_; // array of one-shot data vector indexes
    ChannelIndex channel_; // Which channel this reader is used for (if applicable)
};

// Type aliases for convenience
template<class T> using GlobalStateReader = StateReader<GlobalState<T>>;
template<class T> using ChannelStateReader = StateReader<ChannelState<T>>;

///////////////////////////////////////////////////////////
// STATE READER/WRITER
///////////////////////////////////////////////////////////

namespace detail {

// Compile-time for loop helper
template<int start, class Writer, class Tuple, int... integers>
void ResumeStateHelper(Writer* writer, const Tuple& t, std::integer_sequence<int, integers...>&&)
{
    (writer->template Set<start + integers>(std::get<integers>(t)), ...);
}

// Calls writer->Set() for each element in the tuple t
template<int start, int end, class Writer, class Tuple>
void ResumeState(Writer* writer, const Tuple& t)
{
    ResumeStateHelper<start>(writer, t, std::make_integer_sequence<int, detail::abs(start) + end>{});
}

} // namespace detail

template<class StateClass>
class StateReaderWriter : public StateReader<StateClass>
{
public:

    // Bring in dependencies:
    using R = StateReader<StateClass>;
    using typename R::State;
    using typename R::StateEnumCommon;
    using typename R::StateEnum;
    using typename R::OneShotEnumCommon;
    using typename R::OneShotEnum;

    template<int data_index, detail::StateType state_type>
    using GetType = typename R::template GetType<data_index, state_type>;
    template<int data_index, detail::StateType state_type>
    using GetWrappedType = typename R::template GetWrappedType<data_index, state_type>;

    StateReaderWriter() : R{} {}
    StateReaderWriter(State* state) : R{state} {}
    ~StateReaderWriter() = default;

    void AssignStateWrite(State* state) { state_write_ = state; R::AssignState(state); }
    void AssignStateWrite(State* state, ChannelIndex channel) { state_write_ = state; R::AssignState(state, channel); }

    void Reset() override
    {
        R::Reset();
    }

    // Set the initial state
    template<int state_data_index>
    void SetInitial(GetType<state_data_index, detail::kState>&& val)
    {
        SetWritePos(-1);
        assert(state_write_);
        auto& vec = state_write_->template Get<state_data_index>();
        assert(vec.empty());
        vec.push_back({R::cur_pos_, std::move(val)});
    }

    // Set the initial state
    template<int state_data_index>
    inline void SetInitial(const GetType<state_data_index, detail::kState>& val)
    {
        GetType<state_data_index, detail::kState> val_copy = val;
        SetInitial<state_data_index>(std::move(val_copy));
    }

    // Set the specified state data (state_data_index) at the current write position (the end of the vector) to val
    template<int state_data_index, bool ignore_duplicates = false>
    void Set(GetType<state_data_index, detail::kState>&& val)
    {
        assert(state_write_);
        auto& vec = state_write_->template Get<state_data_index>();
        assert(!vec.empty() && "Use SetInitial() to set the initial state before using Set()");
        auto& vec_elem = vec.back(); // Current vec element (always the end when writing)

        // There can only be one state data value for a given OrderRowPosition, so we won't always be adding a new element to the vector
        if (vec_elem.first != R::cur_pos_)
        {
            if constexpr (!ignore_duplicates)
            {
                // If the latest value in the state is the same
                // as what we're trying to add to the state, don't add it
                if (vec_elem.second == val) { return; }
            }

            // Add new element
            vec.push_back({R::cur_pos_, std::move(val)});

            // Adjust current index
            int& index = R::cur_indexes_[R::GetIndex(state_data_index)]; // Current index within state data for this data type
            ++index;
        }
        else
        {
            vec_elem.second = std::move(val); // Update current element
        }
    }

    // Set the specified state data (state_data_index) at the current write position (the end of the vector) to val
    template<int state_data_index, bool ignore_duplicates = false>
    inline void Set(const GetType<state_data_index, detail::kState>& val)
    {
        GetType<state_data_index, detail::kState> val_copy = val;
        Set<state_data_index, ignore_duplicates>(std::move(val_copy));
    }

    // Set the specified one-shot data (oneshot_data_index) at the current write position (the end of the vector) to val
    template<int oneshot_data_index>
    void SetOneShot(GetType<oneshot_data_index, detail::kOneShot>&& val)
    {
        assert(state_write_);
        auto& vec = state_write_->template GetOneShot<oneshot_data_index>();

        // For the 1st time setting this state. TODO: Use SetInitial() for this for greater efficiency?
        if (vec.empty())
        {
            // Add new element
            vec.push_back({R::cur_pos_, std::move(val)});

            // Adjust current index
            int& index = R::cur_indexes_oneshot_[R::GetOneShotIndex(oneshot_data_index)]; // Current index within one-shot data for this data type
            ++index;
            return;
        }

        auto& vec_elem = vec.back(); // Current vec element (always the end when writing)

        // There can only be one one-shot data value for a given OrderRowPosition, so we won't always be adding a new element to the vector
        if (vec_elem.first != R::cur_pos_)
        {
            // Add new element
            vec.push_back({R::cur_pos_, std::move(val)});

            // Adjust current index
            int& index = R::cur_indexes_oneshot_[R::GetOneShotIndex(oneshot_data_index)]; // Current index within one-shot data for this data type
            ++index;
        }
        else
        {
            vec_elem.second = std::move(val); // Update current element
        }
    }

    // Set the specified state data (oneshot_data_index) at the current write position (the end of the vector) to val
    template<int oneshot_data_index>
    inline void SetOneShot(const GetType<oneshot_data_index, detail::kOneShot>& val)
    {
        GetType<oneshot_data_index, detail::kOneShot> val_copy = val;
        SetOneShot<oneshot_data_index>(std::move(val_copy));
    }

    // Inserts state data at current position. Use with Copy() in order to "resume" a state.
    void Resume(const typename State::CombinedStateData& vals)
    {
        // Calls Set() for each element in vals
        detail::ResumeState<State::kLowerBound, State::kUpperBound>(this, vals);
    }

    // Inserts data into the state at the current read position. The read/write position is invalid afterwards, so it is reset.
    template<int state_data_index, bool overwrite = false>
    auto Insert(const GetType<state_data_index, detail::kState>& val) -> bool
    {
        assert(state_write_);
        auto& vec = state_write_->template Get<state_data_index>();
        assert(!vec.empty() && "The initial state must be set before reading");
        const int vec_index = R::cur_indexes_[R::GetIndex(state_data_index)];
        auto& vec_elem = vec[vec_index];

        if (R::cur_pos_ == vec_elem.first)
        {
            // There's already an element at the position we want to insert val
            if constexpr (overwrite)
            {
                vec_elem.second = val;
                Reset();
                return false;
            }
            Reset();
            return true; // Failure
        }

        // pos > vec_elem.first
        const auto iter_after = vec.begin() + vec_index + 1;
        vec.insert(iter_after, std::pair{R::cur_pos_, val});
        Reset();
        return false;
    }

    // Inserts data into the state at a given position. The read/write position is invalid afterwards, so it is reset.
    template<int state_data_index, bool overwrite = false>
    auto Insert(OrderRowPosition pos, const GetType<state_data_index, detail::kState>& val) -> bool
    {
        Reset();
        R::SetReadPos(pos);
        SetWritePos(pos);
        return Insert<state_data_index, overwrite>(val);
    }

    // Call this at the start of an inner loop before Set() is called
    inline void SetWritePos(OrderRowPosition pos)
    {
        R::cur_pos_ = pos;
    }

    // Call this at the start of an inner loop before Set() is called
    inline void SetWritePos(OrderIndex order, RowIndex row)
    {
        SetWritePos(GetOrderRowPosition(order, row));
    }

private:

    State* state_write_; // The state this reader is writing to
};

// Type aliases for convenience
template<class T> using GlobalStateReaderWriter = StateReaderWriter<GlobalState<T>>;
template<class T> using ChannelStateReaderWriter = StateReaderWriter<ChannelState<T>>;

///////////////////////////////////////////////////////////
// STATE READERS/WRITERS
///////////////////////////////////////////////////////////

template<class ModuleClass>
struct StateReaders
{
    GlobalStateReader<ModuleClass> global_reader;
    std::vector<ChannelStateReader<ModuleClass>> channel_readers;

    void Reset()
    {
        global_reader.Reset();
        for (auto& temp : channel_readers)
        {
            temp.Reset();
        }
    }
};

template<class ModuleClass>
struct StateReaderWriters
{
    GlobalStateReaderWriter<ModuleClass> global_reader_writer;
    std::vector<ChannelStateReaderWriter<ModuleClass>> channel_reader_writers;

    void Reset()
    {
        global_reader_writer.Reset();
        for (auto& temp : channel_reader_writers)
        {
            temp.Reset();
        }
    }

    /*
    void Save()
    {
        // TODO: Are oneshot states saved?
        saved_channel_states_.resize(channel_reader_writers.size());
        saved_global_data_ = global_reader_writer.Copy();
        for (unsigned i = 0; i < channel_reader_writers.size(); ++i)
        {
            saved_channel_states_[i] = channel_reader_writers[i].Copy();
        }
    }

    void Restore()
    {
        // TODO: Are oneshot states restored?
        assert(saved_channel_states_.size() == channel_reader_writers.size());
        global_reader_writer.Restore(saved_global_data_);
        for (unsigned i = 0; i < channel_reader_writers.size(); ++i)
        {
            channel_reader_writers[i].Restore(saved_channel_states_[i]);
        }
    }
    */

private:
    typename GlobalState<ModuleClass>::CombinedStateData saved_global_data_;
    std::vector<typename ChannelState<ModuleClass>::CombinedStateData> saved_channel_states_;
};


///////////////////////////////////////////////////////////
// MODULE STATE
///////////////////////////////////////////////////////////

template<class ModuleClass>
class ModuleState
{
public:

    // Creates and returns a pointer to a StateReaders object. The readers are valid only for as long as ModuleState is valid.
    [[nodiscard]] auto GetReaders() const -> std::shared_ptr<StateReaders<ModuleClass>>
    {
        auto return_val = std::make_shared<StateReaders<ModuleClass>>();
        return_val->global_reader.AssignState(&global_state_);
        return_val->channel_readers.resize(channel_states_.size());
        for (unsigned i = 0; i < channel_states_.size(); ++i)
        {
            return_val->channel_readers[i].AssignState(&channel_states_[i], i);
        }
        return return_val;
    }

private:

    // Only the ModuleClass which this class stores state information for is allowed to write state data
    friend ModuleClass;

    void Initialize(unsigned numChannels) { channel_states_.resize(numChannels); }

    // Creates and returns a pointer to a StateReaderWriters object. The reader/writers are valid only for as long as ModuleState is valid.
    [[nodiscard]] auto GetReaderWriters() -> std::shared_ptr<StateReaderWriters<ModuleClass>>
    {
        auto return_val = std::make_shared<StateReaderWriters<ModuleClass>>();
        return_val->global_reader_writer.AssignStateWrite(&global_state_);
        return_val->channel_reader_writers.resize(channel_states_.size());
        for (unsigned i = 0; i < channel_states_.size(); ++i)
        {
            return_val->channel_reader_writers[i].AssignStateWrite(&channel_states_[i], i);
        }
        return return_val;
    }

private:
    GlobalState<ModuleClass> global_state_;
    std::vector<ChannelState<ModuleClass>> channel_states_;
};

} // namespace d2m
