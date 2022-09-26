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
using OrderRowPosition = uint32_t;

using GlobalOrderRowPosition = OrderRowPosition;
using ChannelOrderRowPosition = OrderRowPosition;

// Helpers for conversion:
inline constexpr OrderRowPosition GetOrderRowPosition(OrderIndex order, RowIndex row) { return (order << 16) | row; };
inline constexpr std::pair<OrderIndex, RowIndex> GetOrderRowPosition(OrderRowPosition pos) { return { pos >> 16, pos & 0x00FF }; };

namespace detail {

struct StateDefinitionTag {};

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
    using type = size_t;
};

using EffectValueXX = uint8_t;
using EffectValueXXYY = uint8_t; //std::pair<uint8_t, uint8_t>;

// Global state data types

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
using NoteSlotStateData = NoteSlot;
template<class T> using SoundIndexStateData = typename SoundIndex<T>::type;

///////////////////////////////////////////////////////////
// COMMON STATE DEFINITIONS
///////////////////////////////////////////////////////////

template<class ModuleClass>
struct GlobalStateCommonDefinition : public detail::StateDefinitionTag
{
    static constexpr int kCommonCount = 5; // # of variants in StateEnumCommon (remember to update this after changing the enum)
    static constexpr int kLowerBound = -kCommonCount;

    // Common state data have negative indexes
    enum StateEnumCommon
    {
        // Add additional variants here
        kTempo              =-5,
        kSpeedB             =-4,
        kSpeedA             =-3,
        kPatBreak           =-2,
        kPosJump            =-1
        // StateEnum contains values >= 0
    };

    // Lowest to highest
    using StateDataCommon = std::tuple<
        TempoStateData,
        SpeedBStateData,
        SpeedAStateData,
        PatBreakStateData,
        PosJumpStateData
        >;
};

template<class ModuleClass>
struct ChannelStateCommonDefinition : public detail::StateDefinitionTag
{
    static constexpr int kCommonCount = 14; // # of variants in StateEnumCommon (remember to update this after changing the enum)
    static constexpr int kLowerBound = -kCommonCount;

    // Common state data have negative indexes
    enum StateEnumCommon
    {
        // Add additional variants here
        kNoteDelay          =-14,
        kNoteCut            =-13,
        kRetrigger          =-12,
        kVolSlide           =-11,
        kPanning            =-10,
        kTremolo            =-9,
        kVibratoVolSlide    =-8,
        kPort2NoteVolSlide  =-7,
        kVibrato            =-6,
        kPort               =-5, // should this be split into port up, port down, and port2note?
        kArp                =-4,
        kVolume             =-3,
        kNoteSlot           =-2,
        kSoundIndex         =-1
        // StateEnum contains values >= 0
    };

    // Lowest to highest
    using StateDataCommon = std::tuple<
        NoteDelayStateData,
        NoteCutStateData,
        RetriggerStateData,
        VolSlideStateData,
        PanningStateData,
        TremoloStateData,
        VibratoVolSlideStateData,
        Port2NoteVolSlideStateData,
        VibratoStateData,
        PortamentoStateData,
        ArpStateData,
        VolumeStateData,
        NoteSlotStateData,
        SoundIndexStateData<ModuleClass>
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

    constexpr const StateData& GetInitialState() const { return initial_state_; }
    constexpr StateData& GetInitialState() { return initial_state_; }

private:
    StateDataWrapped data_; // Stores all state data
    StateData initial_state_; // Default values which are used when nothing is specified
};

///////////////////////////////////////////////////////////
// GLOBAL/PER-CHANNEL STATE PRIMARY TEMPLATES
///////////////////////////////////////////////////////////

/*
 * The following are the global and per-channel state storage primary class templates.
 * They can be specialized to add additional supported state data if desired.
 * Any specializations must inherit from StateStorage and pass the correct common definition
 * struct plus the new module-specific types to the template parameter.
 * In addition, specializations must define StateEnumCommon and StateEnum.
 * All state data types must have a "==" operator defined for them.
 */

template<class ModuleClass>
struct GlobalState : public StateStorage<GlobalStateCommonDefinition<ModuleClass> /* Module-specific types go here in any specializations */>
{
    using GlobalStateCommonDefinition<ModuleClass>::StateEnumCommon;
    enum StateEnum {};
};

template<class ModuleClass>
struct ChannelState : public StateStorage<ChannelStateCommonDefinition<ModuleClass> /* Module-specific types go here in any specializations */>
{
    using ChannelStateCommonDefinition<ModuleClass>::StateEnumCommon;
    enum StateEnum {};
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
template<int start, class Reader, typename Function, int... Is>
void NextStateHelper(Reader const* reader, Function f, std::integer_sequence<int, Is...>)
{
    (f(reader->template GetVec<start + Is>(), start + Is), ...);
}

// Function F arguments are: (wrapped state data vector, index)
template<int start, int end, class Reader, typename Function>
void NextState(Reader const* reader, Function f)
{
    NextStateHelper<start>(reader, f, std::make_integer_sequence<int, gcem::abs(start) + end>{});
}

} // namespace detail


// Allows easy, efficient reading/traversal of GlobalState/ChannelState
template<class TState, std::enable_if_t<std::is_base_of_v<detail::StateDefinitionTag, TState>, bool> = true>
class StateReader
{
protected:

    static constexpr int enum_lower_bound_ = TState::kLowerBound;
    static constexpr int enum_common_count_ = TState::kCommonCount;
    static constexpr int enum_upper_bound_ = TState::kUpperBound;
    static constexpr int enum_total_count_ = enum_common_count_ + enum_upper_bound_;

public:

    // Bring in dependencies:
    using StateData = typename TState::StateData;
    using StateDataWrapped = typename TState::StateDataWrapped;
    using StateEnumCommon = typename TState::StateEnumCommon;
    using StateEnum = typename TState::StateEnum;

    // Helpers:
    template<int state_data_index> using get_data_t = std::tuple_element_t<state_data_index + enum_common_count_, StateData>;
    template<int state_data_index> using get_data_wrapped_t = std::tuple_element_t<state_data_index + enum_common_count_, StateDataWrapped>;

public:
    StateReader() : state_(nullptr), cur_pos_(0), cur_indexes_{} {}
    StateReader(TState* state) : state_(state), cur_pos_(0), cur_indexes_{} {}
    void AssignState(TState* state) { state_ = state; }

    // Set current read position to the beginning of the Module's state data
    void Reset()
    {
        cur_pos_ = 0;
        cur_indexes_.fill(-1);
    }

    // Get the specified state data (state_data_index) at the current read position
    template<int state_data_index>
    inline constexpr const get_data_t<state_data_index>& Get() const
    {
        const size_t vec_index = cur_indexes_[state_data_index + enum_common_count_];
        assert(vec_index >= 0 && "The initial state must be set before reading");
        return GetVec<state_data_index>().at(vec_index).second;
    }

    // Get the specified state data (state_data_index) at the specified read index (vec_index) within the vector
    template<int state_data_index>
    inline constexpr const get_data_t<state_data_index>& Get(size_t vec_index) const
    {
        return GetVec<state_data_index>().at(vec_index).second;
    }

    // Get the specified state data vector (state_data_index)
    template<int state_data_index>
    inline constexpr const get_data_wrapped_t<state_data_index>& GetVec() const
    {
        assert(state_);
        return state_->template Get<state_data_index>();
    }

    // Gets the initial state
    inline constexpr const StateData& GetInitialState() const
    {
        assert(state_);
        return state_->GetInitialState();
    }

    // Returns a tuple of all the state values at the current read position
    StateData Copy() const
    {
        StateData retVal;
        detail::CopyState<enum_lower_bound_, enum_upper_bound_>(this, retVal,
            [this](auto& retValElem, const auto& val) constexpr
        {
            retValElem = val;
        });
        return retVal;
    }

    /*
     * Advances the read position to the next row in the state data if needed; pos should be the current position.
     * Call this method at the start of an inner loop before any the reading has been done for that iteration.
     * If return_deltas == true, returns an array of bools specifying which state values have changed since last iteration.
     */
    template<bool return_deltas = false>
    auto SetReadPos(OrderRowPosition pos) -> std::conditional_t<return_deltas, std::array<bool, enum_total_count_>, void>
    {
        cur_pos_ = pos;

        [[maybe_unused]] std::array<bool, enum_total_count_> deltas{}; // Will probably be optimized away if return_deltas == false

        detail::NextState<enum_lower_bound_, enum_upper_bound_>(this, [&, this](const auto& vec, int state_data_index) constexpr
        {
            size_t& index = cur_indexes_[state_data_index + enum_common_count_]; // Current index within state data
            const size_t vecSize = vec.size();

            if (vecSize == 0 || index + 1 == vecSize)
                return; // No state data for data type state_data_index, or on last element in state data

            // There's a next state that we could potentially need to advance to
            if (cur_pos_ >= vec.at(index+1).first)
            {
                // Need to advance
                ++index;

                if constexpr (return_deltas)
                {
                    // NOTE: If Set() is called with ignore_duplicates == true, delta could be true even if nothing changed.
                    deltas[state_data_index + enum_common_count_] = true;
                }
            }
        });

        if constexpr (return_deltas)
            return deltas;
        else
            return;
    }

    /*
     * Advances the read position to the next row in the state data if needed; pos should be the current position.
     * Call this method at the start of an inner loop before any the reading has been done for that iteration.
     * If return_deltas == true, returns an array of bools specifying which state values have changed since last iteration.
     */
    template<bool return_deltas = false>
    inline auto SetReadPos(OrderIndex order, RowIndex row) -> std::conditional_t<return_deltas, std::array<bool, enum_total_count_>, void>
    {
        if constexpr (return_deltas)
            return SetReadPos<return_deltas>(GetOrderRowPosition(order, row));
        else
            SetReadPos<return_deltas>(GetOrderRowPosition(order, row));
    }

    // Get the size of the specified state data vector (state_data_index)
    template<int state_data_index>
    inline constexpr size_t GetSize() const
    {
        return GetVec<state_data_index>().size();
    }

    // Add this value to StateEnumCommon or StateEnum variants to get a zero-based index into an array such as the one returned by SetReadPos
    constexpr inline int GetIndexOffset() const { return enum_common_count_; }

protected:

    TState* state_; // The state this reader is reading from
    OrderRowPosition cur_pos_; // The current read position in terms of order and pattern row. (The write position is the end of the state data vector)
    std::array<size_t, enum_total_count_> cur_indexes_; // array of state data vector indexes
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
void InsertStateHelper(TWriter* writer, const TTuple& t, std::integer_sequence<int, Is...>)
{
    (writer->template Set<start + Is>(std::get<Is>(t)), ...);
}

// Calls writer->Set() for each element in the tuple t
template<int start, int end, class TWriter, class TTuple>
void InsertState(TWriter* writer, const TTuple& t)
{
    InsertStateHelper<start>(writer, t, std::make_integer_sequence<int, gcem::abs(start) + end>{});
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

template<class TState>
class StateReaderWriter : public StateReader<TState>
{
public:

    // Inherit constructors
    using StateReader<TState>::StateReader;

    // Bring in dependencies from parent:
    using R = StateReader<TState>;
    using typename R::StateData;
    using typename R::StateEnumCommon;
    using typename R::StateEnum;
    template<int state_data_index> using get_data_t = typename R::template get_data_t<state_data_index>;

    // Set the specified state data (state_data_index) at the current write position (the end of the vector) to val
    template<int state_data_index, bool ignore_duplicates = false>
    void Set(get_data_t<state_data_index>&& val)
    {
        assert(R::state_ != nullptr);
        auto& vec = R::state_->template Get<state_data_index>();

        // For the 1st time setting this state. TODO: Use SetInitial() for this for greater efficiency?
        if (vec.empty())
        {
            // Add new element
            vec.push_back({R::cur_pos_, std::move(val)});

            // Adjust current index
            size_t& index = R::cur_indexes_[state_data_index + R::enum_common_count_]; // Current index within state data for data type I
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
            size_t& index = R::cur_indexes_[state_data_index + R::enum_common_count_]; // Current index within state data for data type I
            ++index;
        }
        else
            vec_elem.second = std::move(val); // Update current element
    }

    // Set the specified state data (state_data_index) at the current write position (the end of the vector) to val
    template<int state_data_index, bool ignore_duplicates = false>
    inline void Set(const get_data_t<state_data_index>& val)
    {
        get_data_t<state_data_index> valCopy = val;
        Set<state_data_index, ignore_duplicates>(std::move(valCopy));
    }

    // For non-persistent state values. Next time SetWritePos is called, nextVal will automatically be set.
    template<int state_data_index, bool ignore_duplicates = false>
    inline void SetSingle(get_data_t<state_data_index>&& val, get_data_t<state_data_index>&& nextVal)
    {
        std::get<state_data_index + R::enum_common_count_>(next_vals_) = std::move(nextVal);
        has_next_vals_ = true;
        Set<state_data_index, ignore_duplicates>(std::move(val));
    }

    // For non-persistent state values. Next time SetWritePos is called, nextVal will automatically be set.
    template<int state_data_index, bool ignore_duplicates = false>
    inline void SetSingle(const get_data_t<state_data_index>& val, const get_data_t<state_data_index>& nextVal)
    {
        std::get<state_data_index + R::enum_common_count_>(next_vals_) = nextVal;
        has_next_vals_ = true;
        Set<state_data_index, ignore_duplicates>(val);
    }

    // Sets the initial state
    void SetInitialState(StateData&& vals)
    {
        assert(R::state_);
        R::state_->GetInitialState() = std::move(vals);
    }

    // Sets the initial state
    inline void SetInitialState(const StateData& vals)
    {
        StateData vals_copy = vals;
        SetInitialState(std::move(vals_copy));
    }

    // Inserts state data at current position. Use with Copy() in order to "resume" a state.
    void Insert(const StateData& vals)
    {
        // Calls Set() for each element in vals
        detail::InsertState<R::enum_lower_bound_, R::enum_upper_bound_>(this, vals);
    }

    // Call this at the start of an inner loop before Set() is called
    void SetWritePos(OrderRowPosition pos)
    {
        R::cur_pos_ = pos;

        // If SetSingle() was used, the next values are set here
        if (has_next_vals_)
        {
            detail::InsertStateOptional<R::enum_lower_bound_, R::enum_upper_bound_>(this, next_vals_);
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
        global_reader_writer.Insert(saved_global_data_);
        for (unsigned i = 0; i < channel_reader_writers.size(); ++i)
            channel_reader_writers[i].Insert(saved_channel_states_[i]);
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
            retVal->channel_readers[i].AssignState(&channel_states_[i]);
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
        retVal->global_reader_writer.AssignState(&global_state_);
        retVal->channel_reader_writers.resize(channel_states_.size());
        for (unsigned i = 0; i < channel_states_.size(); ++i)
        {
            retVal->channel_reader_writers[i].AssignState(&channel_states_[i]);
        }
        return retVal;
    }

private:
    GlobalState<ModuleClass> global_state_;
    std::vector<ChannelState<ModuleClass>> channel_states_;
};

} // namespace d2m
