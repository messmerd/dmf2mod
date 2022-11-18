/*
    state.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines GlobalState, ChannelState, StateReader, and ModuleState
*/

#pragma once

#include "config_types.h"
#include "note.h"
#include "effects.h"

#include <vector>
#include <tuple>
#include <array>
#include <optional>
#include <memory>
#include <cstdint>
#include <type_traits>
#include <cassert>

#include "gcem.hpp"

namespace d2m {

// Unique, quickly calculated value encoding order # (not pattern #!) and pattern row #. Easily and quickly comparable.
using OrderRowPosition = int32_t;

using GlobalOrderRowPosition = OrderRowPosition;
using ChannelOrderRowPosition = OrderRowPosition;

// Helpers for conversion:
inline constexpr OrderRowPosition GetOrderRowPosition(OrderIndex order, RowIndex row) { return (order << 16) | row; };
inline constexpr std::pair<OrderIndex, RowIndex> GetOrderRowPosition(OrderRowPosition pos) { return { pos >> 16, pos & 0x00FF }; };

namespace detail {

struct StateDefinitionTag {};
struct OneShotDefinitionTag {};

// Sourced from: https://stackoverflow.com/a/53398815/8704745
template<typename... input_t>
using tuple_cat_t = decltype(std::tuple_cat(std::declval<input_t>()...));

template<typename T>
struct wrapped_state_data {};

template<typename... Ts>
struct wrapped_state_data<std::tuple<Ts...>>
{
    // type is either an empty tuple or a tuple with each Ts wrapped in a vector of pairs
    using type = std::conditional_t<sizeof...(Ts)==0, std::tuple<>, std::tuple<std::vector<std::pair<OrderRowPosition, Ts>>...>>;
};

template<typename... T>
using wrapped_state_data_t = typename wrapped_state_data<T...>::type;

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
using PatBreakStateData = EffectValueXX;
using PosJumpStateData = EffectValueXX;

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
inline constexpr bool operator==(const PortamentoStateData& lhs, const PortamentoStateData& rhs) { return lhs.type == rhs.type && lhs.value == rhs.value; }

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
    static constexpr int kCommonCount = 3; // # of variants in StateEnumCommon (remember to update this after changing the enum)
    static constexpr int kLowerBound = -kCommonCount;

    // Common state data have negative indexes
    enum StateEnumCommon
    {
        // Add additional variants here
        kTempo              =-3,
        kSpeedB             =-2,
        kSpeedA             =-1
        // StateEnum contains values >= 0
    };

    // Lowest to highest
    using StateDataCommon = std::tuple<
        TempoStateData,
        SpeedBStateData,
        SpeedAStateData
        >;
};

struct GlobalOneShotCommonDefinition : public detail::OneShotDefinitionTag
{
    static constexpr int kOneShotCommonCount = 3; // # of variants in OneShotEnumCommon (remember to update this after changing the enum)
    static constexpr int kOneShotLowerBound = -kOneShotCommonCount;

    // Common one-shot data have negative indexes
    enum OneShotEnumCommon
    {
        // Add additional variants here
        kLoopback           =-3,
        kPatBreak           =-2,
        kPosJump            =-1
        // OneShotEnum contains values >= 0
    };

    // Lowest to highest
    using OneShotDataCommon = std::tuple<
        LoopbackStateData,
        PatBreakStateData,
        PosJumpStateData
        >;
};

template<class ModuleClass>
struct ChannelStateCommonDefinition : public detail::StateDefinitionTag
{
    static constexpr int kCommonCount = 12; // # of variants in StateEnumCommon (remember to update this after changing the enum)
    static constexpr int kLowerBound = -kCommonCount;

    // Common state data have negative indexes
    enum StateEnumCommon
    {
        // Add additional variants here
        kVolSlide           =-12,
        kPanning            =-11,
        kTremolo            =-10,
        kVibratoVolSlide    =-9,
        kPort2NoteVolSlide  =-8,
        kVibrato            =-7,
        kPort               =-6, // should this be split into port up, port down, and port2note?
        kArp                =-5,
        kVolume             =-4,
        kNotePlaying        =-3,
        kNoteSlot           =-2,
        kSoundIndex         =-1
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
        NoteSlotStateData,
        SoundIndexStateData<ModuleClass>
        >;
};

struct ChannelOneShotCommonDefinition : public detail::OneShotDefinitionTag
{
    static constexpr int kOneShotCommonCount = 3; // # of variants in OneShotEnumCommon (remember to update this after changing the enum)
    static constexpr int kOneShotLowerBound = -kOneShotCommonCount;

    // Common one-shot data have negative indexes
    enum OneShotEnumCommon
    {
        // Add additional variants here
        kNoteDelay          =-3,
        kNoteCut            =-2,
        kRetrigger          =-1
        // OneShotEnum contains values >= 0
    };

    // Lowest to highest
    using OneShotDataCommon = std::tuple<
        NoteDelayStateData,
        NoteCutStateData,
        RetriggerStateData
        >;
};

///////////////////////////////////////////////////////////
// STATE STORAGE
///////////////////////////////////////////////////////////

template<class CommonDef, typename... Ts>
class StateStorage : public CommonDef
{
public:
    static constexpr int kUpperBound = sizeof...(Ts); // # of module-specific state data types

    // The StateEnum for any module-specific types should be defined
    //  in the GlobalState/ChannelState template specialization

    using StateDataModuleSpecific = std::tuple<Ts...>;

    // Single tuple of all data types stored by this state
    using StateData = detail::tuple_cat_t<typename CommonDef::StateDataCommon, StateDataModuleSpecific>;

    // Single tuple of all wrapped data types stored by this state. They should all be vectors of pairs.
    using StateDataWrapped = detail::wrapped_state_data_t<StateData>;

    // Returns an immutable reference to state data at index state_data_index
    template<int state_data_index>
    constexpr const auto& Get() const
    {
        return std::get<state_data_index + CommonDef::kCommonCount>(data_);
    }

    // Returns a mutable reference to state data at index state_data_index
    template<int state_data_index>
    constexpr auto& Get()
    {
        return std::get<state_data_index + CommonDef::kCommonCount>(data_);
    }

private:
    StateDataWrapped data_; // Stores all state data
};

template<class CommonDef, typename... Ts>
class OneShotStorage : public CommonDef
{
public:
    static constexpr int kOneShotUpperBound = sizeof...(Ts); // # of module-specific one-shot data types

    // The OneShotEnum for any module-specific types should be defined
    //  in the GlobalState/ChannelState template specialization

    using OneShotDataModuleSpecific = std::tuple<Ts...>;

    // Single tuple of all data types stored by this one-shot state
    using OneShotData = detail::tuple_cat_t<typename CommonDef::OneShotDataCommon, OneShotDataModuleSpecific>;

    // Single tuple of all wrapped data types stored by this one-shot state. They should all be vectors of pairs.
    using OneShotDataWrapped = detail::wrapped_state_data_t<OneShotData>;

    // Returns an immutable reference to one-shot data at index oneshot_data_index
    template<int oneshot_data_index>
    constexpr const auto& GetOneShot() const
    {
        return std::get<oneshot_data_index + CommonDef::kOneShotCommonCount>(oneshot_data_);
    }

    // Returns a mutable reference to one-shot data at index oneshot_data_index
    template<int oneshot_data_index>
    constexpr auto& GetOneShot()
    {
        return std::get<oneshot_data_index + CommonDef::kOneShotCommonCount>(oneshot_data_);
    }

private:
    OneShotDataWrapped oneshot_data_; // Stores all one-shot state data
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
    public StateStorage<ChannelStateCommonDefinition<ModuleClass> /* Module-specific types go here in any specializations */>,
    public OneShotStorage<ChannelOneShotCommonDefinition /* Module-specific types go here in any specializations */>
{
    using typename ChannelStateCommonDefinition<ModuleClass>::StateEnumCommon;
    enum StateEnum {};
    using typename ChannelOneShotCommonDefinition::OneShotEnumCommon;
    enum OneShotEnum {};
};

///////////////////////////////////////////////////////////
// STATE READER
///////////////////////////////////////////////////////////

namespace detail {

// Compile-time for loop helper
template<int start, class TReader, class TTuple, typename TFunction, int... Is>
void CopyStateHelper(TReader const* reader, TTuple& t, TFunction f, std::integer_sequence<int, Is...>)
{
    (f(std::get<Is>(t), reader->template Get<start + Is>()), ...);
}

// Function F arguments are: (inner data tuple element reference, inner data)
template<int start, int end, class TReader, class TTuple, typename TFunction>
void CopyState(TReader const* reader, TTuple& t, TFunction f)
{
    CopyStateHelper<start>(reader, t, f, std::make_integer_sequence<int, gcem::abs(start) + end>{});
}

// Compile-time for loop helper
template<int start, bool oneshots, class Reader, typename Function, int... Is>
void NextStateHelper(Reader const* reader, Function f, std::integer_sequence<int, Is...>)
{
    if constexpr (oneshots)
        (f(reader->template GetOneShotVec<start + Is>(), start + Is), ...);
    else
        (f(reader->template GetVec<start + Is>(), start + Is), ...);
}

// Function F arguments are: (wrapped state/one-shot data vector, index)
template<int start, int end, bool oneshots, class Reader, typename Function>
void NextState(Reader const* reader, Function f)
{
    NextStateHelper<start, oneshots>(reader, f, std::make_integer_sequence<int, gcem::abs(start) + end>{});
}

} // namespace detail


// Allows easy, efficient reading/traversal of GlobalState/ChannelState
template<class StateClass, std::enable_if_t<std::is_base_of_v<detail::StateDefinitionTag, StateClass> && std::is_base_of_v<detail::OneShotDefinitionTag, StateClass>, bool> = true>
class StateReader
{
public:

    // Bring in dependencies:
    using State = StateClass;
    using StateData = typename State::StateData;
    using StateDataWrapped = typename State::StateDataWrapped;
    using StateEnumCommon = typename State::StateEnumCommon;
    using StateEnum = typename State::StateEnum;

    using OneShotData = typename State::OneShotData;
    using OneShotDataWrapped = typename State::OneShotDataWrapped;
    using OneShotEnumCommon = typename State::OneShotEnumCommon;
    using OneShotEnum = typename State::OneShotEnum;

    // Helpers:
    template<int state_data_index> using get_data_t = std::tuple_element_t<state_data_index + State::kCommonCount, StateData>;
    template<int state_data_index> using get_data_wrapped_t = std::tuple_element_t<state_data_index + State::kCommonCount, StateDataWrapped>;

    template<int oneshot_data_index> using get_oneshot_data_t = std::tuple_element_t<oneshot_data_index + State::kOneShotCommonCount, OneShotData>;
    template<int oneshot_data_index> using get_oneshot_data_wrapped_t = std::tuple_element_t<oneshot_data_index + State::kOneShotCommonCount, OneShotDataWrapped>;

    using Deltas = std::array<bool, State::kCommonCount + State::kUpperBound>;
    using OneShotDeltas = std::array<bool, State::kOneShotCommonCount + State::kOneShotUpperBound>;

    StateReader() : state_{nullptr} { Reset(); }
    StateReader(State* state) : state_{state} { Reset(); }
    virtual ~StateReader() = default;

    void AssignState(State const* state) { state_ = state; channel_ = 0; }
    void AssignState(State const* state, ChannelIndex channel) { state_ = state; channel_ = channel; }

    // Set current read position to the beginning of the Module's state data
    virtual void Reset()
    {
        cur_pos_ = 0;
        cur_indexes_.fill(-1); // ???
        cur_indexes_oneshot_.fill(0);
        deltas_.fill(false);
        oneshot_deltas_.fill(false);
    }

    // Get the specified state data vector (state_data_index)
    template<int state_data_index>
    inline constexpr const get_data_wrapped_t<state_data_index>& GetVec() const
    {
        assert(state_);
        return state_->template Get<state_data_index>();
    }

    // Get the specified one-shot data vector (oneshot_data_index)
    template<int oneshot_data_index>
    inline constexpr const get_oneshot_data_wrapped_t<oneshot_data_index>& GetOneShotVec() const
    {
        assert(state_);
        return state_->template GetOneShot<oneshot_data_index>();
    }

    // Get the specified state data (state_data_index) at the current read position
    template<int state_data_index>
    inline constexpr const get_data_t<state_data_index>& Get() const
    {
        const int vec_index = cur_indexes_[GetIndex(state_data_index)];
        assert(vec_index >= 0 && "The initial state must be set before reading");
        return GetVec<state_data_index>().at(vec_index).second;
    }

    // Get the specified state data (state_data_index) at the specified read index (vec_index) within the vector
    template<int state_data_index>
    inline constexpr const get_data_t<state_data_index>& Get(size_t vec_index) const
    {
        return GetVec<state_data_index>().at(vec_index).second;
    }

    // Get the specified one-shot data (oneshot_data_index) at the current read position. Only valid if GetOneShotDelta() returned true.
    template<int oneshot_data_index>
    inline constexpr const get_oneshot_data_t<oneshot_data_index>& GetOneShot() const
    {
        const int vec_index = cur_indexes_oneshot_[GetOneShotIndex(oneshot_data_index)];
        assert(vec_index > 0 && "Only call GetOneShot() if GetOneShotDelta() returned true");
        return GetOneShotVec<oneshot_data_index>().at(vec_index-1).second;
    }

    // Gets the specified state data (state_data_index) if it is exactly at the current read position.
    // TODO: This makes a copy. Try using std::reference_wrapper
    template<int state_data_index>
    inline constexpr auto GetImpulse() const -> std::optional<get_data_t<state_data_index>>
    {
        const auto& vec = GetVec<state_data_index>();
        if (vec.empty())
            return std::nullopt;

        const int vec_index = cur_indexes_[GetIndex(state_data_index)];
        assert(vec_index >= 0 && "The initial state must be set before reading");
        const auto& elem = vec.at(vec_index);
        if (elem.first != cur_pos_)
            return std::nullopt;

        return elem.second;
    }

    // Returns a tuple of all the state values at the current read position
    StateData Copy() const
    {
        StateData retVal;
        detail::CopyState<State::kLowerBound, State::kUpperBound>(this, retVal,
            [](auto& retValElem, const auto& val) constexpr
        {
            retValElem = val;
        });
        return retVal;
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
            const int vec_size = vec.size();
            if (vec_size == 0)
                return; // No state data for data type state_data_index

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
            const int vec_size = vec.size();
            if (vec_size == 0)
                return; // No one-shot data for data type oneshot_data_index

            int& index = cur_indexes_oneshot_[GetOneShotIndex(oneshot_data_index)]; // Current index within one-shot data
            if (index == vec_size)
                return;

            // While we need to advance the state
            while (cur_pos_ >= vec.at(index).first)
            {
                if constexpr (set_deltas)
                {
                    // If we've reached the position of the current one-shot data
                    if (cur_pos_ == vec.at(index).first)
                        oneshot_deltas_[GetOneShotIndex(oneshot_data_index)] = true;
                }

                // Advance to next index
                ++index;

                // Break if there is no more one-shot data left to read
                if (index == vec_size)
                    break;
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
    StateData ReadAt(OrderRowPosition pos)
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
    inline StateData ReadAt(OrderIndex order, RowIndex row)
    {
        return ReadAt(GetOrderRowPosition(order, row));
    }

    // Get the size of the specified state data vector (state_data_index)
    template<int state_data_index>
    inline constexpr size_t GetSize() const
    {
        return GetVec<state_data_index>().size();
    }

    // Returns the deltas from the last SetReadPos<true>() call
    inline constexpr const Deltas& GetDeltas() const { return deltas_; }

    inline constexpr bool GetDelta(int state_data_index) const { return deltas_[GetIndex(state_data_index)]; }

    // Returns the one-shot deltas from the last SetReadPos<true>() call
    inline constexpr const OneShotDeltas& GetOneShotDeltas() const { return oneshot_deltas_; }

    inline constexpr bool GetOneShotDelta(int oneshot_data_index) const { return oneshot_deltas_[GetOneShotIndex(oneshot_data_index)]; }

    // Only useful for ChannelStateReader
    inline ChannelIndex GetChannel() const { return channel_; }

    // Gets a desired value from StateData
    template<int state_data_index>
    static constexpr get_data_t<state_data_index> GetValue(const StateData& data)
    {
        return std::get<GetIndex(state_data_index)>(data);
    }

protected:

    // Converts StateEnumCommon or StateEnum variants into a zero-based index of an array. Returns offset if no enum is provided.
    static inline constexpr int GetIndex(int state_data_index = 0) { return State::kCommonCount + state_data_index; }

    // Converts OneShotEnumCommon or OneShotEnum variants into a zero-based index of an array. Returns offset if no enum is provided.
    static inline constexpr int GetOneShotIndex(int oneshot_data_index = 0) { return State::kOneShotCommonCount + oneshot_data_index; }

    State const* state_; // The state this reader is reading from
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
template<int start, class TWriter, class TTuple, int... Is>
void ResumeStateHelper(TWriter* writer, const TTuple& t, std::integer_sequence<int, Is...>)
{
    (writer->template Set<start + Is>(std::get<Is>(t)), ...);
}

// Calls writer->Set() for each element in the tuple t
template<int start, int end, class TWriter, class TTuple>
void ResumeState(TWriter* writer, const TTuple& t)
{
    ResumeStateHelper<start>(writer, t, std::make_integer_sequence<int, gcem::abs(start) + end>{});
}

template<typename T>
struct optional_state_data {};

template<typename... Ts>
struct optional_state_data<std::tuple<Ts...>>
{
    // type is a tuple with each Ts wrapped in std::optional
    using type = std::tuple<std::optional<Ts>...>;
};

template<typename... T>
using optional_state_data_t = typename optional_state_data<T...>::type;

// Compile-time for loop helper
template<int start, class TWriter, class TTuple, int... Is>
void InsertStateOptionalHelper(TWriter* writer, const TTuple& t, std::integer_sequence<int, Is...>)
{
    ((std::get<Is>(t).has_value() ? writer->template Set<start + Is>(std::get<Is>(t).value()) : void(0)), ...);
}

// Calls writer->Set() for each element which has a value in the tuple of optionals t
template<int start, int end, class TWriter, class TTuple>
void InsertStateOptional(TWriter* writer, const TTuple& t)
{
    InsertStateOptionalHelper<start>(writer, t, std::make_integer_sequence<int, gcem::abs(start) + end>{});
}

} // namespace detail

template<class StateClass>
class StateReaderWriter : public StateReader<StateClass>
{
public:

    // Bring in dependencies:
    using R = StateReader<StateClass>;
    using typename R::State;
    using typename R::StateData;
    using typename R::StateEnumCommon;
    using typename R::StateEnum;
    using typename R::OneShotData;
    using typename R::OneShotEnumCommon;
    using typename R::OneShotEnum;
    template<int state_data_index> using get_data_t = typename R::template get_data_t<state_data_index>;
    template<int oneshot_data_index> using get_oneshot_data_t = typename R::template get_oneshot_data_t<oneshot_data_index>;

    StateReaderWriter() : R()
    {
        next_vals_ = {};
        has_next_vals_ = false;
    }
    StateReaderWriter(State* state) : R(state)
    {
        next_vals_ = {};
        has_next_vals_ = false;
    }
    ~StateReaderWriter() = default;

    void AssignStateWrite(State* state) { state_write_ = state; R::AssignState(state); }
    void AssignStateWrite(State* state, ChannelIndex channel) { state_write_ = state; R::AssignState(state, channel); }

    void Reset() override
    {
        next_vals_ = {};
        has_next_vals_ = false;
        R::Reset();
    }

    // Set the specified state data (state_data_index) at the current write position (the end of the vector) to val
    template<int state_data_index, bool ignore_duplicates = false>
    void Set(get_data_t<state_data_index>&& val)
    {
        assert(state_write_);
        auto& vec = state_write_->template Get<state_data_index>();

        // For the 1st time setting this state. TODO: Use SetInitial() for this for greater efficiency?
        if (vec.empty())
        {
            // Add new element
            vec.push_back({R::cur_pos_, std::move(val)});

            // Adjust current index
            int& index = R::cur_indexes_[R::GetIndex(state_data_index)]; // Current index within state data for this data type
            ++index;
            return;
        }

        auto& vec_elem = vec.back(); // Current vec element (always the end when writing)

        // There can only be one state data value for a given OrderRowPosition, so we won't always be adding a new element to the vector
        if (vec_elem.first != R::cur_pos_)
        {
            if constexpr (!ignore_duplicates)
            {
                // If the latest value in the state is the same
                // as what we're trying to add to the state, don't add it
                if (vec_elem.second == val)
                    return;
            }

            // Add new element
            vec.push_back({R::cur_pos_, std::move(val)});

            // Adjust current index
            int& index = R::cur_indexes_[R::GetIndex(state_data_index)]; // Current index within state data for this data type
            ++index;
        }
        else
            vec_elem.second = std::move(val); // Update current element
    }

    // Set the specified state data (state_data_index) at the current write position (the end of the vector) to val
    template<int state_data_index, bool ignore_duplicates = false>
    inline void Set(const get_data_t<state_data_index>& val)
    {
        get_data_t<state_data_index> val_copy = val;
        Set<state_data_index, ignore_duplicates>(std::move(val_copy));
    }

    // For non-persistent state values. Next time SetWritePos is called, next_val will automatically be set. TODO: Not needed?
    template<int state_data_index, bool ignore_duplicates = false>
    inline void SetSingle(get_data_t<state_data_index>&& val, get_data_t<state_data_index>&& next_val)
    {
        std::get<state_data_index + State::kCommonCount>(next_vals_) = std::move(next_val);
        has_next_vals_ = true;
        Set<state_data_index, ignore_duplicates>(std::move(val));
    }

    // For non-persistent state values. Next time SetWritePos is called, next_val will automatically be set. TODO: Not needed?
    template<int state_data_index, bool ignore_duplicates = false>
    inline void SetSingle(const get_data_t<state_data_index>& val, const get_data_t<state_data_index>& next_val)
    {
        std::get<state_data_index + State::kCommonCount>(next_vals_) = next_val;
        has_next_vals_ = true;
        Set<state_data_index, ignore_duplicates>(val);
    }

    // Set the specified one-shot data (oneshot_data_index) at the current write position (the end of the vector) to val
    template<int oneshot_data_index>
    void SetOneShot(get_oneshot_data_t<oneshot_data_index>&& val)
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
            vec_elem.second = std::move(val); // Update current element
    }

    // Set the specified state data (oneshot_data_index) at the current write position (the end of the vector) to val
    template<int oneshot_data_index>
    inline void SetOneShot(const get_oneshot_data_t<oneshot_data_index>& val)
    {
        get_oneshot_data_t<oneshot_data_index> val_copy = val;
        SetOneShot<oneshot_data_index>(std::move(val_copy));
    }

    // Inserts state data at current position. Use with Copy() in order to "resume" a state.
    void Resume(const StateData& vals)
    {
        // Calls Set() for each element in vals
        detail::ResumeState<State::kLowerBound, State::kUpperBound>(this, vals);
    }

    // Inserts data into the state at the current read position. The read/write position is invalid afterwards, so it is reset.
    template<int state_data_index, bool overwrite = false>
    bool Insert(const get_data_t<state_data_index>& val)
    {
        assert(state_write_);
        auto& vec = state_write_->template Get<state_data_index>();
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
    bool Insert(OrderRowPosition pos, const get_data_t<state_data_index>& val)
    {
        Reset();
        R::SetReadPos(pos);
        SetWritePos(pos);
        return Insert<state_data_index, overwrite>(val);
    }

    // Call this at the start of an inner loop before Set() is called
    void SetWritePos(OrderRowPosition pos)
    {
        R::cur_pos_ = pos;

        // If SetSingle() was used, the next values are set here
        if (has_next_vals_)
        {
            detail::InsertStateOptional<State::kLowerBound, State::kUpperBound>(this, next_vals_);
            next_vals_ = {}; // Clear the tuple and reset optionals
            has_next_vals_ = false;
        }
    }

    // Call this at the start of an inner loop before Set() is called
    inline void SetWritePos(OrderIndex order, RowIndex row)
    {
        SetWritePos(GetOrderRowPosition(order, row));
    }

private:

    State* state_write_; // The state this reader is writing to
    detail::optional_state_data_t<StateData> next_vals_;
    bool has_next_vals_{false};
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

    void SetReadPos(OrderRowPosition pos)
    {
        global_reader.SetReadPos(pos);
        for (auto& temp : channel_readers)
            temp.SetReadPos(pos);
    }

    void SetReadPos(OrderIndex order, RowIndex row) { SetReadPos(GetOrderRowPosition(order, row)); }

    void Reset()
    {
        global_reader.Reset();
        for (auto& temp : channel_readers)
            temp.Reset();
    }
};

template<class ModuleClass>
struct StateReaderWriters
{
    GlobalStateReaderWriter<ModuleClass> global_reader_writer;
    std::vector<ChannelStateReaderWriter<ModuleClass>> channel_reader_writers;

    void SetReadPos(OrderRowPosition pos)
    {
        global_reader_writer.SetReadPos(pos);
        for (auto& temp : channel_reader_writers)
            temp.SetReadPos(pos);
    }

    void SetReadPos(OrderIndex order, RowIndex row) { SetReadPos(GetOrderRowPosition(order, row)); }

    void SetWritePos(OrderRowPosition pos)
    {
        global_reader_writer.SetWritePos(pos);
        for (auto& temp : channel_reader_writers)
            temp.SetWritePos(pos);
    }

    void SetWritePos(OrderIndex order, RowIndex row) { SetWritePos(GetOrderRowPosition(order, row)); }

    void Reset()
    {
        global_reader_writer.Reset();
        for (auto& temp : channel_reader_writers)
            temp.Reset();
    }

    void Save()
    {
        saved_channel_states_.resize(channel_reader_writers.size());
        saved_global_data_ = global_reader_writer.Copy();
        for (unsigned i = 0; i < channel_reader_writers.size(); ++i)
            saved_channel_states_[i] = channel_reader_writers[i].Copy();
    }

    void Restore()
    {
        assert(saved_channel_states_.size() == channel_reader_writers.size());
        global_reader_writer.Restore(saved_global_data_);
        for (unsigned i = 0; i < channel_reader_writers.size(); ++i)
            channel_reader_writers[i].Restore(saved_channel_states_[i]);
    }

private:
    typename GlobalState<ModuleClass>::StateData saved_global_data_;
    std::vector<typename ChannelState<ModuleClass>::StateData> saved_channel_states_;
};


///////////////////////////////////////////////////////////
// MODULE STATE
///////////////////////////////////////////////////////////

template<class ModuleClass>
class ModuleState
{
public:

    // Creates and returns a pointer to a StateReaders object. The readers are valid only for as long as ModuleState is valid.
    std::shared_ptr<StateReaders<ModuleClass>> GetReaders() const
    {
        auto retVal = std::make_shared<StateReaders<ModuleClass>>();
        retVal->global_reader.AssignState(&global_state_);
        retVal->channel_readers.resize(channel_states_.size());
        for (unsigned i = 0; i < channel_states_.size(); ++i)
        {
            retVal->channel_readers[i].AssignState(&channel_states_[i], i);
        }
        return retVal;
    }

private:

    // Only the ModuleClass which this class stores state information for is allowed to write state data
    friend ModuleClass;

    void Initialize(unsigned numChannels) { channel_states_.resize(numChannels); }

    // Creates and returns a pointer to a StateReaderWriters object. The reader/writers are valid only for as long as ModuleState is valid.
    std::shared_ptr<StateReaderWriters<ModuleClass>> GetReaderWriters()
    {
        auto retVal = std::make_shared<StateReaderWriters<ModuleClass>>();
        retVal->global_reader_writer.AssignStateWrite(&global_state_);
        retVal->channel_reader_writers.resize(channel_states_.size());
        for (unsigned i = 0; i < channel_states_.size(); ++i)
        {
            retVal->channel_reader_writers[i].AssignStateWrite(&channel_states_[i], i);
        }
        return retVal;
    }

private:
    GlobalState<ModuleClass> global_state_;
    std::vector<ChannelState<ModuleClass>> channel_states_;
};

} // namespace d2m
