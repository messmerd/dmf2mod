/*
    state.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines GlobalState, ChannelState, StateReader, and ModuleState
*/

#pragma once

#include "config_types.h"

#include <vector>
#include <tuple>
#include <array>
#include <optional>
#include <memory>
#include <cstdint>
#include <type_traits>

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

/*
template<typename T>
struct wrapped_state_data
{
    using type = std::tuple<>;
};

template<typename T>
using wrapped_state_data_t = typename wrapped_state_data<T>::type;
*/

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

using volume_t = uint16_t;
using panning_t = int16_t;

struct tempo_t
{
    uint16_t num;
    uint16_t den;
};

///////////////////////////////////////////////////////////
// COMMON STATE DEFINITIONS
///////////////////////////////////////////////////////////

struct GlobalStateCommonDefinition : public detail::state_definition_tag
{
    static constexpr int kCommonCount = 1; // # of variants in StateEnumCommon (remember to update this after changing the enum)
    static constexpr int kLowerBound = -kCommonCount;

    // Common state data have negative indexes
    enum StateEnumCommon
    {
        kTempo=-1
    };

    // Define common data types
    using tempo_data_t = tempo_t;

    // Lowest to highest
    using common_data_t = std::tuple<
        tempo_data_t
        >;
};

struct ChannelStateCommonDefinition : public detail::state_definition_tag
{
    static constexpr int kCommonCount = 4; // # of variants in StateEnumCommon (remember to update this after changing the enum)
    static constexpr int kLowerBound = -kCommonCount;

    // Common state data have negative indexes
    enum StateEnumCommon
    {
        kSoundIndex=-1,
        kVolume=-2,
        kNotePlaying=-3,
        kPanning=-4
    };

    // Define common data types
    using sound_index_data_t = sound_index_t;
    using volume_data_t = volume_t;
    using note_playing_data_t = bool;
    using panning_data_t = panning_t;

    // Lowest to highest
    using common_data_t = std::tuple<
        sound_index_data_t,
        volume_data_t,
        note_playing_data_t,
        panning_data_t
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

    /*
    // Returns a mutable reference to state data at index I
    template<int I>
    constexpr auto& Get()
    {
        return std::get<I + CommonDefT::kCommonCount>(data_);
    }
    */

private:
    data_wrapped_t data_; // Stores all state data
};

///////////////////////////////////////////////////////////
// GLOBAL/PER-CHANNEL STATE PRIMARY TEMPLATES
///////////////////////////////////////////////////////////

/*
 * The following are the global and per-channel state storage primary class templates.
 * They can be specialized to add additional supported state data if desired.
 * Any specializations must inherit from StateStorage and pass the correct common definition
 * struct plus the new module-specific types to the template parameter.
 */

template<class ModuleClass>
struct GlobalState : public StateStorage<GlobalStateCommonDefinition /* Module-specific types go here in any specializations */> {};

template<class ModuleClass>
struct ChannelState : public StateStorage<ChannelStateCommonDefinition /* Module-specific types go here in any specializations */> {};

///////////////////////////////////////////////////////////
// STATE READER
///////////////////////////////////////////////////////////

namespace detail {

// Compile-time for loop adapted from: https://stackoverflow.com/a/55648874/8704745
template<int start, class T, typename F, size_t... Is>
void next_state_ctf_helper(T const* reader, F f, std::index_sequence<Is...>)
{
    (f(reader->template GetVec<start + Is>(), start + Is), ...);
}

template<int start, int end, class T, typename F>
void next_state_ctf(T const* reader, F f)
{
    next_state_ctf_helper<start>(reader, f, std::make_index_sequence<gcem::abs(start) + end>{});
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

    using state_data_t = typename StateT::data_t;
    using state_data_wrapped_t = typename StateT::data_wrapped_t;

    template<int I>
    using get_data_t = std::tuple_element_t<I + enum_common_count_, state_data_t>;

    template<int I>
    using get_data_wrapped_t = std::tuple_element_t<I + enum_common_count_, state_data_wrapped_t>;

public:
    StateReader(StateT const* state) : state_(state), cur_pos_(0), cur_indexes_{} {}

    // Set current read position to the beginning of the Module's state data
    void Reset()
    {
        cur_pos_ = 0;
        cur_indexes_.fill(0);
    }

    // Get the specified state data (I) at the current index
    template<int I>
    constexpr const get_data_t<I>& Get() const
    {
        return GetVec<I>().at(cur_indexes_[I]).second;
    }

    // Get the specified state data (I) at the specified index (index)
    template<int I>
    constexpr const get_data_t<I>& Get(size_t index) const
    {
        return GetVec<I>().at(index).second;
    }

    // Get the specified state data vector (I)
    template<int I>
    inline constexpr const get_data_wrapped_t<I>& GetVec() const
    {
        return state_->template Get<I>();
    }

    // Advances read position to the next row in the state data; newPos should be the position after the last time this method was called.
    void Next(pos_t newPos)
    {
        cur_pos_ = newPos;

        detail::next_state_ctf<enum_lower_bound_, enum_upper_bound_>(this, [this](const auto& vec, int N) constexpr
        {
            size_t& index = cur_indexes_[N + enum_common_count_]; // Current index within state data for data type N
            const size_t vec_size = vec.size();

            if (vec_size == 0 || index + 1 == vec_size)
                return; // No state data for data type N, or on last element in state data

            // There's a next state that we could potentially need to advance to
            if (cur_pos_ >= vec.at(index+1).first)
            {
                // Need to advance
                ++index;
            }
        });
    }

    void Next(order_index_t order, row_index_t row)
    {
        Next(GetPos(order, row));
    }

protected:

    StateT const* state_; // The state this reader is reading from
    global_pos_t cur_pos_; // The current read position in terms of order and pattern row
    std::array<size_t, enum_total_count_> cur_indexes_; // array of state data vector indexes
    // TODO: Add a "delta" array which keeps track of state data which recently changed
};

// Type aliases for convenience
template<class T> using GlobalStateReader = StateReader<GlobalState<T>>;
template<class T> using ChannelStateReader = StateReader<ChannelState<T>>;

///////////////////////////////////////////////////////////
// STATE READER/WRITER
///////////////////////////////////////////////////////////

namespace detail {

// Compile-time for loop helper
template<int start, class T, class Tuple, typename F, size_t... Is>
void copy_state_ctf_helper(T const* reader, Tuple& t, F f, std::index_sequence<Is...>)
{
    (f(std::get<Is>(t), reader->template Get<start + Is>()), ...);
}

// Function F arguments are: (inner data tuple element reference, inner data)
template<int start, int end, class T, class Tuple, typename F>
void copy_state_ctf(T const* reader, Tuple& t, F f)
{
    copy_state_ctf_helper<start>(reader, t, f, std::make_index_sequence<gcem::abs(start) + end>{});
}

// Compile-time for loop helper
template<int start, class T, class Tuple, typename F, size_t... Is>
void insert_state_ctf_helper(T const* reader, Tuple& t, F f, std::index_sequence<Is...>)
{
    (f(reader->template GetVec<start + Is>(), std::get<Is>(t)), ...);
}

// Function F arguments are: (state data vector reference, inner data to insert)
template<int start, int end, class T, class Tuple, typename F>
void insert_state_ctf(T const* reader, Tuple& t, F f)
{
    insert_state_ctf_helper<start>(reader, t, f, std::make_index_sequence<gcem::abs(start) + end>{});
}

} // namespace detail

template<class StateT>
class StateReaderWriter : public StateReader<StateT>
{
public:

    using StateReader<StateT>::StateReader;

    using state_data_t = typename StateReader<StateT>::state_data_t;

    state_data_t Copy() const
    {
        state_data_t retVal;
        detail::copy_state_ctf<
            StateReader<StateT>::enum_lower_bound_,
            StateReader<StateT>::enum_upper_bound_>
            (this, retVal, [this](auto& retValElem, const auto& val) constexpr
        {
            retValElem = val;
        });
        return retVal;
    }

    void Insert(state_data_t& allInnerData)
    {
        detail::insert_state_ctf<
            StateReader<StateT>::enum_lower_bound_,
            StateReader<StateT>::enum_upper_bound_>
            (this, allInnerData, [this](auto& stateVec, auto& innerData) constexpr
        {
            // Create a (position, data) pair and add it to the end of the state vector
            stateVec.push_back({StateReader<StateT>::cur_pos_, std::move(innerData)});
        });
    }

};

// Type aliases for convenience
template<class T> using GlobalStateReaderWriter = StateReaderWriter<GlobalState<T>>;
template<class T> using ChannelStateReaderWriter = StateReaderWriter<ChannelState<T>>;

///////////////////////////////////////////////////////////
// MODULE STATE
///////////////////////////////////////////////////////////

template<class ModuleClass>
class ModuleState
{
public:
    void Initialize(unsigned numChannels)
    {
        channel_states_.resize(numChannels);
    }

    // Creates and returns a GlobalStateReader. The reader is valid only for as long as ModuleState is valid.
    std::shared_ptr<GlobalStateReader<ModuleClass>> GetGlobalReader() const { return std::make_shared<GlobalStateReader<ModuleClass>>(&global_state_); }

    // Creates and returns a ChannelStateReader for the given channel. The reader is valid only for as long as ModuleState is valid.
    std::shared_ptr<ChannelStateReader<ModuleClass>> GetChannelReader(channel_index_t channel) const { return std::make_shared<ChannelStateReader<ModuleClass>>(&channel_states_[channel]); }

    // TODO: These methods should not be public:

    // Creates and returns a GlobalStateReaderWriter. The reader/writer is valid only for as long as ModuleState is valid.
    std::shared_ptr<GlobalStateReaderWriter<ModuleClass>> GetGlobalReaderWriter() const { return std::make_shared<GlobalStateReaderWriter<ModuleClass>>(&global_state_); }

    // Creates and returns a ChannelStateReaderWriter for the given channel. The reader/writer is valid only for as long as ModuleState is valid.
    std::shared_ptr<ChannelStateReaderWriter<ModuleClass>> GetChannelReaderWriter(channel_index_t channel) const { return std::make_shared<ChannelStateReaderWriter<ModuleClass>>(&channel_states_[channel]); }

    GlobalState<ModuleClass>* GetGlobalState() { return global_state_; }
    ChannelState<ModuleClass>* GetChannelState(channel_index_t channel) { return channel_states_[channel]; }

private:
    GlobalState<ModuleClass> global_state_;
    std::vector<ChannelState<ModuleClass>> channel_states_;
};

} // namespace d2m
