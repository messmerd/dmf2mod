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
using pos_t = uint32_t;

using global_pos_t = pos_t;
using channel_pos_t = pos_t;

// Helpers for conversion:
inline constexpr pos_t GetPos(order_index_t order, row_index_t row) { return (order << 16) | row; };
inline constexpr std::pair<order_index_t, row_index_t> GetPos(pos_t pos) { return { pos >> 16, pos & 0x00FF }; };

namespace detail {

struct state_definition_tag {};

// Sourced from: https://stackoverflow.com/a/53398815/8704745
template<typename... input_t>
using tuple_cat_t = decltype(std::tuple_cat(std::declval<input_t>()...));

template<typename T>
struct wrapped_state_data {};

template<typename... Ts>
struct wrapped_state_data<std::tuple<Ts...>>
{
    // type is either an empty tuple or a tuple with each Ts wrapped in a vector of pairs
    using type = std::conditional_t<sizeof...(Ts)==0, std::tuple<>, std::tuple<std::vector<std::pair<pos_t, Ts>>...>>;
};

template<typename... T>
using wrapped_state_data_t = typename wrapped_state_data<T...>::type;

} // namespace detail

///////////////////////////////////////////////////////////
// COMMON STATE DATA TYPES
///////////////////////////////////////////////////////////

// A unique identifier for wavetables, duty cycles, samples, etc.
using sound_index_t = size_t;

using effect_val_xx_t = uint8_t;
using effect_val_xxyy_t = std::pair<uint8_t, uint8_t>;

struct port_t
{
    enum Type { Up, Down, ToNote } type;
    effect_val_xx_t value;
};

constexpr inline bool operator==(const port_t& lhs, const port_t& rhs) { return lhs.type == rhs.type && lhs.value == rhs.value; }

///////////////////////////////////////////////////////////
// COMMON STATE DEFINITIONS
///////////////////////////////////////////////////////////

struct GlobalStateCommonDefinition : public detail::state_definition_tag
{
    static constexpr int kCommonCount = 5; // # of variants in StateEnumCommon (remember to update this after changing the enum)
    static constexpr int kLowerBound = -kCommonCount;

    // Common state data have negative indexes
    enum StateEnumCommon
    {
        // Add additional variants here
        kJumpDestination    =-5,
        kTempo              =-4,
        kSpeed              =-3,
        kPatBreak           =-2,
        kPosJump            =-1
        // StateEnum contains values >= 0
    };

    // Define common data types
    using jump_dest_data_t = bool;
    using tempo_data_t = effect_val_xx_t;
    using speed_data_t = effect_val_xx_t;
    using patbreak_data_t = effect_val_xx_t;
    using posjump_data_t = effect_val_xx_t;

    // Lowest to highest
    using common_data_t = std::tuple<
        jump_dest_data_t,
        tempo_data_t,
        speed_data_t,
        patbreak_data_t,
        posjump_data_t
        >;
};

struct ChannelStateCommonDefinition : public detail::state_definition_tag
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

    // Define common data types
    using notedelay_data_t = effect_val_xx_t;
    using notecut_data_t = effect_val_xx_t;
    using retrigger_data_t = effect_val_xxyy_t;
    using volslide_data_t = effect_val_xxyy_t;
    using panning_data_t = effect_val_xx_t;
    using tremolo_data_t = effect_val_xxyy_t;
    using vibrato_volslide_data_t = effect_val_xxyy_t;
    using port2note_volslide_data_t = effect_val_xxyy_t;
    using vibrato_data_t = effect_val_xxyy_t;
    using port_data_t = port_t;
    using arp_data_t = effect_val_xxyy_t;
    using volume_data_t = effect_val_xx_t;
    using noteslot_data_t = NoteSlot;
    using sound_index_data_t = sound_index_t;

    // Lowest to highest
    using common_data_t = std::tuple<
        notedelay_data_t,
        notecut_data_t,
        retrigger_data_t,
        volslide_data_t,
        panning_data_t,
        tremolo_data_t,
        vibrato_volslide_data_t,
        port2note_volslide_data_t,
        vibrato_data_t,
        port_data_t,
        arp_data_t,
        volume_data_t,
        noteslot_data_t,
        sound_index_data_t
        >;
};

///////////////////////////////////////////////////////////
// STATE STORAGE
///////////////////////////////////////////////////////////

template<class CommonDefT, typename... Ts>
class StateStorage : public CommonDefT
{
public:
    static constexpr int kUpperBound = sizeof...(Ts); // # of module-specific state data types

    // The StateEnum for any module-specific types should be defined
    //  in the GlobalState template specialization

    using module_specific_data_t = std::tuple<Ts...>;

    // Single tuple of all data types stored by this state
    using data_t = detail::tuple_cat_t<typename CommonDefT::common_data_t, module_specific_data_t>;

    // Single tuple of all wrapped data types stored by this state. They should all be vectors of pairs.
    using data_wrapped_t = detail::wrapped_state_data_t<data_t>;

    // Returns an immutable reference to state data at index I
    template<int I>
    constexpr const auto& Get() const
    {
        return std::get<I + CommonDefT::kCommonCount>(data_);
    }

    // Returns a mutable reference to state data at index I
    template<int I>
    constexpr auto& Get()
    {
        return std::get<I + CommonDefT::kCommonCount>(data_);
    }

    constexpr const data_t& GetInitialState() const { return initial_state_; }
    constexpr data_t& GetInitialState() { return initial_state_; }

private:
    data_wrapped_t data_; // Stores all state data
    data_t initial_state_; // Default values which are used when nothing is specified
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
 */

template<class ModuleClass>
struct GlobalState : public StateStorage<GlobalStateCommonDefinition /* Module-specific types go here in any specializations */>
{
    using StateStorage<GlobalStateCommonDefinition>::StateEnumCommon;
    enum StateEnum {};
};

template<class ModuleClass>
struct ChannelState : public StateStorage<ChannelStateCommonDefinition /* Module-specific types go here in any specializations */>
{
    using StateStorage<ChannelStateCommonDefinition>::StateEnumCommon;
    enum StateEnum {};
};

///////////////////////////////////////////////////////////
// STATE READER
///////////////////////////////////////////////////////////

namespace detail {

// Compile-time for loop helper
template<int start, class T, class Tuple, typename F, int... Is>
void copy_state_ctf_helper(T const* reader, Tuple& t, F f, std::integer_sequence<int, Is...>)
{
    (f(std::get<Is>(t), reader->template Get<start + Is>()), ...);
}

// Function F arguments are: (inner data tuple element reference, inner data)
template<int start, int end, class T, class Tuple, typename F>
void copy_state_ctf(T const* reader, Tuple& t, F f)
{
    copy_state_ctf_helper<start>(reader, t, f, std::make_integer_sequence<int, gcem::abs(start) + end>{});
}

// Compile-time for loop helper
template<int start, class T, typename F, int... Is>
void next_state_ctf_helper(T const* reader, F f, std::integer_sequence<int, Is...>)
{
    (f(reader->template GetVec<start + Is>(), start + Is), ...);
}

// Function F arguments are: (wrapped state data vector, index)
template<int start, int end, class T, typename F>
void next_state_ctf(T const* reader, F f)
{
    next_state_ctf_helper<start>(reader, f, std::make_integer_sequence<int, gcem::abs(start) + end>{});
}

} // namespace detail


// Allows easy, efficient reading/traversal of GlobalState/ChannelState
template<class StateT, std::enable_if_t<std::is_base_of_v<detail::state_definition_tag, StateT>, bool> = true>
class StateReader
{
protected:

    static constexpr int enum_lower_bound_ = StateT::kLowerBound;
    static constexpr int enum_common_count_ = StateT::kCommonCount;
    static constexpr int enum_upper_bound_ = StateT::kUpperBound;
    static constexpr int enum_total_count_ = enum_common_count_ + enum_upper_bound_;

public:

    // Bring in dependencies:
    using data_t = typename StateT::data_t;
    using data_wrapped_t = typename StateT::data_wrapped_t;
    using StateEnumCommon = typename StateT::StateEnumCommon;
    using StateEnum = typename StateT::StateEnum;

    // Helpers:
    template<int I> using get_data_t = std::tuple_element_t<I + enum_common_count_, data_t>;
    template<int I> using get_data_wrapped_t = std::tuple_element_t<I + enum_common_count_, data_wrapped_t>;

public:
    StateReader() : state_(nullptr), cur_pos_(0), cur_indexes_{} {}
    StateReader(StateT* state) : state_(state), cur_pos_(0), cur_indexes_{} {}
    void AssignState(StateT* state) { state_ = state; }

    // Set current read position to the beginning of the Module's state data
    void Reset()
    {
        cur_pos_ = 0;
        cur_indexes_.fill(0);
    }

    // Get the specified state data (I) at the current read position
    template<int I>
    inline constexpr const get_data_t<I>& Get() const
    {
        return GetVec<I>().at(cur_indexes_[I]).second;
    }

    // Get the specified state data (I) at the specified read index (index)
    template<int I>
    inline constexpr const get_data_t<I>& Get(size_t index) const
    {
        return GetVec<I>().at(index).second;
    }

    // Get the specified state data vector (I)
    template<int I>
    inline constexpr const get_data_wrapped_t<I>& GetVec() const
    {
        assert(state_);
        return state_->template Get<I>();
    }

    // Gets the initial state
    inline constexpr const data_t& GetInitialState() const
    {
        assert(state_);
        return state_->GetInitialState();
    }

    // Returns a tuple of all the state values at the current read position
    data_t Copy() const
    {
        data_t retVal;
        detail::copy_state_ctf<enum_lower_bound_, enum_upper_bound_>(this, retVal,
            [this](auto& retValElem, const auto& val) constexpr
        {
            retValElem = val;
        });
        return retVal;
    }

    /*
     * Advances the read position to the next row in the state data if needed; pos should be the current position.
     * Call this method at the start of an inner loop before any the reading has been done for that iteration.
     * If ReturnDeltas == true, returns an array of bools specifying which state values have changed since last iteration.
     */
    template<bool ReturnDeltas = false>
    std::conditional_t<ReturnDeltas, std::array<bool, enum_total_count_>, void> SetReadPos(pos_t pos)
    {
        cur_pos_ = pos;

        [[maybe_unused]] std::array<bool, enum_total_count_> deltas{}; // Will probably be optimized away if returnDeltas == false

        detail::next_state_ctf<enum_lower_bound_, enum_upper_bound_>(this, [&, this](const auto& vec, int N) constexpr
        {
            size_t& index = cur_indexes_[N + enum_common_count_]; // Current index within state data for data type N
            const size_t vecSize = vec.size();

            if (vecSize == 0 || index + 1 == vecSize)
                return; // No state data for data type N, or on last element in state data

            // There's a next state that we could potentially need to advance to
            if (cur_pos_ >= vec.at(index+1).first)
            {
                // Need to advance
                ++index;

                if constexpr (ReturnDeltas)
                {
                    // NOTE: If Set() is called with IgnoreDuplicates == true, delta could be true even if nothing changed.
                    deltas[N + enum_common_count_] = true;
                }
            }
        });

        if constexpr (ReturnDeltas)
            return deltas;
        else
            return;
    }

    /*
     * Advances the read position to the next row in the state data if needed; pos should be the current position.
     * Call this method at the start of an inner loop before any the reading has been done for that iteration.
     * If ReturnDeltas == true, returns an array of bools specifying which state values have changed since last iteration.
     */
    template<bool ReturnDeltas = false>
    inline std::conditional_t<ReturnDeltas, std::array<bool, enum_total_count_>, void> SetReadPos(order_index_t order, row_index_t row)
    {
        if constexpr (ReturnDeltas)
            return SetReadPos<ReturnDeltas>(GetPos(order, row));
        else
            SetReadPos<ReturnDeltas>(GetPos(order, row));
    }

    // Add this value to StateEnumCommon or StateEnum variants to get a zero-based index into an array such as the one returned by SetReadPos
    constexpr inline int GetIndexOffset() const { return enum_common_count_; }

protected:

    StateT* state_; // The state this reader is reading from
    pos_t cur_pos_; // The current read position in terms of order and pattern row. (The write position is the end of the state data vector)
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
template<int start, class T, class Tuple, int... Is>
void insert_state_ctf_helper(T* writer, const Tuple& t, std::integer_sequence<int, Is...>)
{
    (writer->template Set<start + Is>(std::get<Is>(t)), ...);
}

// Calls writer->Set() for each element in the tuple t
template<int start, int end, class T, class Tuple>
void insert_state_ctf(T* writer, const Tuple& t)
{
    insert_state_ctf_helper<start>(writer, t, std::make_integer_sequence<int, gcem::abs(start) + end>{});
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
template<int start, class T, class Tuple, int... Is>
void optional_insert_state_ctf_helper(T* writer, const Tuple& t, std::integer_sequence<int, Is...>)
{
    ((std::get<Is>(t).has_value() ? writer->template Set<start + Is>(std::get<Is>(t).value()) : void(0)), ...);
}

// Calls writer->Set() for each element which has a value in the tuple of optionals t
template<int start, int end, class T, class Tuple>
void optional_insert_state_ctf(T* writer, const Tuple& t)
{
    optional_insert_state_ctf_helper<start>(writer, t, std::make_integer_sequence<int, gcem::abs(start) + end>{});
}

} // namespace detail

template<class StateT>
class StateReaderWriter : public StateReader<StateT>
{
public:

    // Inherit constructors
    using StateReader<StateT>::StateReader;

    // Bring in dependencies from parent:
    using R = StateReader<StateT>;
    using typename R::data_t;
    using typename R::StateEnumCommon;
    using typename R::StateEnum;
    template<int I> using get_data_t = typename R::template get_data_t<I>;

    // Set the specified state data (I) at the current write position (the end of the vector) to val
    template<int I, bool IgnoreDuplicates = false>
    void Set(get_data_t<I>&& val)
    {
        assert(R::state_);
        auto& vec = R::state_->template Get<I>();
        auto& vecElem = vec.back(); // Current vec element (always the end when writing)

        // There can only be one state data value for a given pos_t, so we won't always be adding a new element to the vector
        if (vecElem.first != R::cur_pos_)
        {
            if constexpr (!IgnoreDuplicates)
            {
                // If the latest value in the state is the same
                // as what we're trying to add to the state, don't add it
                if (vecElem.second == val)
                    return;
            }

            // Add new element
            vec.push_back({R::cur_pos_, std::move(val)});

            // Adjust current index
            size_t& index = R::cur_indexes_[I + R::enum_common_count_]; // Current index within state data for data type I
            ++index;
        }
        else
            vecElem.second = std::move(val); // Update current element
    }

    // Set the specified state data (I) at the current write position (the end of the vector) to val
    template<int I, bool IgnoreDuplicates = false>
    inline void Set(const get_data_t<I>& val)
    {
        get_data_t<I> valCopy = val;
        Set<I, IgnoreDuplicates>(std::move(valCopy));
    }

    // For non-persistent state values. Next time SetWritePos is called, nextVal will automatically be set.
    template<int I, bool IgnoreDuplicates = false>
    inline void SetSingle(get_data_t<I>&& val, get_data_t<I>&& nextVal)
    {
        std::get<I + R::enum_common_count_>(next_vals_) = std::move(nextVal);
        has_next_vals_ = true;
        Set<I, IgnoreDuplicates>(std::move(val));
    }

    // For non-persistent state values. Next time SetWritePos is called, nextVal will automatically be set.
    template<int I, bool IgnoreDuplicates = false>
    inline void SetSingle(const get_data_t<I>& val, const get_data_t<I>& nextVal)
    {
        std::get<I + R::enum_common_count_>(next_vals_) = nextVal;
        has_next_vals_ = true;
        Set<I, IgnoreDuplicates>(val);
    }

    // Sets the initial state
    void SetInitialState(data_t&& vals)
    {
        assert(R::state_);
        R::state_->GetInitialState() = std::move(vals);
    }

    // Sets the initial state
    inline void SetInitialState(const data_t& vals)
    {
        data_t valsCopy = vals;
        SetInitialState(std::move(valsCopy));
    }

    // Inserts state data at current position. Use with Copy() in order to "resume" a state.
    void Insert(const data_t& vals)
    {
        // Calls Set() for each element in vals
        detail::insert_state_ctf<R::enum_lower_bound_, R::enum_upper_bound_>(this, vals);
    }

    // Call this at the start of an inner loop before Set() is called
    void SetWritePos(pos_t pos)
    {
        R::cur_pos_ = pos;

        // If SetSingle() was used, the next values are set here
        if (has_next_vals_)
        {
            detail::optional_insert_state_ctf<R::enum_lower_bound_, R::enum_upper_bound_>(this, next_vals_);
            next_vals_ = {}; // Clear the tuple and reset optionals
            has_next_vals_ = false;
        }
    }

    // Call this at the start of an inner loop before Set() is called
    inline void SetWritePos(order_index_t order, row_index_t row)
    {
        SetWritePos(GetPos(order, row));
    }

private:

    detail::optional_state_data_t<data_t> next_vals_;
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

    void SetReadPos(pos_t newPos)
    {
        global_reader.SetReadPos();
        for (auto& temp : channel_readers)
            temp.SetReadPos();
    }

    void SetReadPos(order_index_t order, row_index_t row) { SetReadPos(GetPos(order, row)); }

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

    void SetReadPos(pos_t newPos)
    {
        global_reader_writer.SetReadPos();
        for (auto& temp : channel_reader_writers)
            temp.SetReadPos();
    }

    void SetReadPos(order_index_t order, row_index_t row) { SetReadPos(GetPos(order, row)); }

    void SetWritePos(pos_t pos)
    {
        global_reader_writer.SetWritePos(pos);
        for (auto& temp : channel_reader_writers)
            temp.SetWritePos(pos);
    }

    void SetWritePos(order_index_t order, row_index_t row) { SetWritePos(GetPos(order, row)); }

    void Reset()
    {
        global_reader_writer.Reset();
        for (auto& temp : channel_reader_writers)
            temp.Reset();
    }
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
