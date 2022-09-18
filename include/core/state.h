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
#include <memory>

namespace d2m {

// Unique, quickly calculated value encoding order # (not pattern #!) and pattern row #. Easily and quickly comparable.
using global_pos_t = uint32_t;

// Same as global_pos_t but on a per-channel basis
using channel_pos_t = uint32_t;

///////////////////////////////////////////////////////////
// COMMON STATE DATA TYPES
///////////////////////////////////////////////////////////

struct TempoNode
{
    uint16_t num;
    uint16_t den;
};

///////////////////////////////////////////////////////////
// GLOBAL STATE
///////////////////////////////////////////////////////////

// Global data that all modules are expected to implement. Specialize this to add supported global state data.
template<class ModuleClass>
class GlobalState
{
    enum GlobalStateEnum
    {
        kCount=0 // count
    };
};

///////////////////////////////////////////////////////////
// PER-CHANNEL STATE
///////////////////////////////////////////////////////////

// Per-channel data that all modules are expected to implement. Specialize this to add supported per-channel state data.
template<class ModuleClass>
class ChannelState
{
    enum ChannelStateEnum
    {
        kCount=0 // count
    };
};

///////////////////////////////////////////////////////////
// STATE READER
///////////////////////////////////////////////////////////

namespace detail {

// Compile-time for loop adapted from: https://stackoverflow.com/a/55648874/8704745
template<size_t start, class T, typename F, size_t... Is>
void ctf_helper(T const* reader, F f, std::index_sequence<Is...>)
{
    (f(reader->template GetVec<start + Is>(), start + Is), ...);
}

template<size_t start, size_t end, class T, typename F>
void compile_time_for(T const* reader, F f)
{
    ctf_helper<start>(reader, f, std::make_index_sequence<end-start>{});
}

template<size_t I, class StateT>
using state_vec_t = std::tuple_element_t<I, typename StateT::data_t>;

template<size_t I, class StateT>
using state_vec_elem_t = typename state_vec_t<I, StateT>::value_type;

template<size_t I, class StateT>
using state_data_t = typename state_vec_elem_t<I, StateT>::second_type;

} // namespace detail


// Allows easy, efficient reading/traversal of GlobalState/ChannelState
template<class StateT>
class StateReader
{
public:
    StateReader(StateT const* state) : state_(state), cur_pos_(0), cur_indexes_{} {}

    // Set current read position to the beginning of the Module's state data
    void Reset()
    {
        cur_pos_ = 0;
        cur_indexes_.fill(0);
    }

    // Get the specified state data (I) at the current index
    template<size_t I>
    const detail::state_data_t<I, StateT>& Get() const
    {
        return GetVec<I>().at(cur_indexes_[I]).second;
    }

    // Get the specified state data (I) at the specified index (index)
    template<size_t I>
    const detail::state_data_t<I, StateT>& Get(size_t index) const
    {
        return GetVec<I>().at(index).second;
    }

    // Get the specified state data vector (I)
    template<size_t I>
    inline const detail::state_vec_t<I, StateT>& GetVec() const
    {
        return state_->template Get<I>();
    }

    // Advances read position to the next row in the state data; newPos should be the position after the last time this method was called.
    void Next(uint32_t newPos)
    {
        cur_pos_ = newPos;

        detail::compile_time_for<0, enum_max_>(this, [this](const auto& vec, size_t N)
        {
            size_t& index = cur_indexes_[N]; // Current index within state data for data type N
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

private:

    static constexpr int enum_max_ = StateT::kCount; // kCount is required to exist for every state data StateT

    StateT const* state_; // The state this reader is reading from
    global_pos_t cur_pos_; // The current read position in terms of order and pattern row
    std::array<size_t, enum_max_> cur_indexes_; // array of state data vector indexes
};

// Type aliases for convenience
template<class T> using GlobalStateReader = StateReader<GlobalState<T>>;
template<class T> using ChannelStateReader = StateReader<ChannelState<T>>;

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

private:
    GlobalState<ModuleClass> global_state_;
    std::vector<ChannelState<ModuleClass>> channel_states_;
};

} // namespace d2m
