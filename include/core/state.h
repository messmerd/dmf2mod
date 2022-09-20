/*
    state.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines GlobalState, ChannelState, StateReader, and ModuleState
*/

#pragma once

#include "config_types.h"
#include "note.h"

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
        // Add additional variants here
        kTempo=-1
        // StateEnum contains values >= 0
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
        // Add additional variants here
        kPanning=-4,
        kVolume=-3,
        kNoteSlot=-2,
        kSoundIndex=-1
        // StateEnum contains values >= 0
    };

    // Define common data types
    using panning_data_t = panning_t;
    using volume_data_t = volume_t;
    using noteslot_data_t = NoteSlot;
    using sound_index_data_t = sound_index_t;

    // Lowest to highest
    using common_data_t = std::tuple<
        panning_data_t,
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
    constexpr const get_data_t<I>& Get() const
    {
        return GetVec<I>().at(cur_indexes_[I]).second;
    }

    // Get the specified state data (I) at the specified read index (index)
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

    // Returns a tuple of all the state values at the current read position
    data_t Copy() const
    {
        data_t retVal;
        detail::copy_state_ctf<
            enum_lower_bound_,
            enum_upper_bound_>
            (this, retVal, [this](auto& retValElem, const auto& val) constexpr
        {
            retValElem = val;
        });
        return retVal;
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

    StateT* state_; // The state this reader is reading from
    pos_t cur_pos_; // The current read position in terms of order and pattern row
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
template<int start, class T, class Tuple, int... Is>
void insert_state_ctf_helper(T* writer, Tuple& t, std::integer_sequence<int, Is...>)
{
    (writer->template Set<start + Is>(std::get<Is>(t)), ...);
}

// Calls writer->Set() for each element in the tuple t
template<int start, int end, class T, class Tuple>
void insert_state_ctf(T* writer, Tuple& t)
{
    insert_state_ctf_helper<start>(writer, t, std::make_integer_sequence<int, gcem::abs(start) + end>{});
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

    // Set the specified state data (I) at the current read/write position to val
    template<int I>
    void Set(const get_data_t<I>& val)
    {
        auto& vec = R::state_->template Get<I>();
        size_t& index = R::cur_indexes_[I + R::enum_common_count_]; // Current index within state data for data type I
        auto& vecElem = vec[index]; // Current vec element

        // There can only be one state data value for a given pos_t, so we won't always be adding a new element to the vector
        if (vecElem.first != R::cur_pos_)
        {
            vec.push_back({R::cur_pos_, val}); // Add new element
            ++index; // Adjust current index
        }
        else
            vecElem.second = val; // Update current element
    }

    // Inserts state data at current position (Use with Copy() in order to "resume" a state)
    void Insert(data_t& allInnerData)
    {
        // Calls Set() for each element in allInnerData
        detail::insert_state_ctf<R::enum_lower_bound_, R::enum_upper_bound_>(this, allInnerData);
    }

    void SetWritePos(pos_t pos) { R::cur_pos_ = pos; }
    void SetWritePos(order_index_t order, row_index_t row) { R::cur_pos_ = GetPos(order, row); }
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

    void Next(pos_t newPos)
    {
        global_reader.Next();
        for (auto& temp : channel_readers)
            temp.Next();
    }

    void Next(order_index_t order, row_index_t row) { Next(GetPos(order, row)); }

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

    void Next(pos_t newPos)
    {
        global_reader_writer.Next();
        for (auto& temp : channel_reader_writers)
            temp.Next();
    }

    void Next(order_index_t order, row_index_t row) { Next(GetPos(order, row)); }

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
