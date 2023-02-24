/*
 * data.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines a class template for storing and accessing module data (orders, patterns, rows, etc.)
 */

#pragma once

#include "core/note.h"

#include <vector>
#include <algorithm>
#include <type_traits>

namespace d2m {

using OrderIndex = uint16_t;
using PatternIndex = uint16_t;
using ChannelIndex = uint8_t;
using RowIndex = uint16_t;

enum class DataStorageType
{
    kNone,
    kCOR,  // Iteration order: Channels --> Orders --> (Pattern) Rows
    kORC,  // Iteration order: Orders --> (Pattern) Rows --> Channels
};

/*
    Global data for a module. This is information such as the title and author.
    Can be customized if a module type has more global information to be stored.
*/

template<DataStorageType data_storage_type = DataStorageType::kNone>
struct ModuleGlobalDataDefault
{
    static constexpr DataStorageType storage_type = data_storage_type;
    std::string title;
    std::string author;
};

template<class ModuleClass>
struct ModuleGlobalData : public ModuleGlobalDataDefault<> {};

/*
    Different modules have significantly different per-channel row contents, so
    providing one single generic implementation for use by every module doesn't
    make much sense. Each module should provide their own Row implementation.
*/

struct RowDefault
{
    NoteSlot note;
};

template<class ModuleClass>
struct Row : public RowDefault {};

/*
    Some module formats contain additional data for each channel or row.
    Specializations for ChannelMetadata and PatternMetadata can be created
    for any module format which requires it.
*/

struct ChannelMetadataDefault
{
    // No defaults at this time
};

template<class ModuleClass>
struct ChannelMetadata : public ChannelMetadataDefault {};

struct PatternMetadataDefault
{
    // No defaults at this time
};

template<class ModuleClass>
struct PatternMetadata : public PatternMetadataDefault {};


namespace detail
{
    /*
     * The class templates below allow different module formats in dmf2mod to choose an underlying
     * storage data structure that works best for them while maintaining a common interface to
     * that data.
     * 
     * The motivation for this is that different module formats store their pattern matrix and
     * pattern data differently, so this affects the kind of C++ data structures their module data
     * more naturally maps to. If a data structure that maps more naturally to the module's data is used,
     * better performance can be achieved when iterating through the data while importing/exporting.
     * Additionally, some formats such as MOD do not have per-channel patterns, meaning that all channels
     * are essentially linked together as far as patterns are concerned. This necessitates a flexible
     * way of storing module data which can work for any module format.
     */

    class ModuleDataStorageBase
    {
    public:
        inline ChannelIndex GetNumChannels() const { return num_channels_; }
        inline OrderIndex GetNumOrders() const { return num_orders_; }
        inline RowIndex GetNumRows() const { return num_rows_; }
    protected:
        virtual void CleanUpData() = 0;
        virtual void SetPatternMatrix() = 0;
        virtual void SetNumPatterns() = 0;
        virtual void SetPatterns() = 0;
        ChannelIndex num_channels_;
        OrderIndex num_orders_;    // Total orders (pattern matrix rows)
        RowIndex num_rows_;      // Rows per pattern
    };

    template<DataStorageType storage_type, class ModuleClass>
    class ModuleDataStorage : public ModuleDataStorageBase
    {
    protected:
        ModuleDataStorage() = delete; // This non-specialized primary template should never be used
        ModuleDataStorage(const ModuleDataStorage&) = delete;
        ModuleDataStorage(ModuleDataStorage&&) = delete;
        void CleanUpData() override {}
        void SetPatternMatrix() override {}
        void SetNumPatterns() override {}
        void SetPatterns() override {}
    };

    template<class ModuleClass>
    class ModuleDataStorage<DataStorageType::kCOR, ModuleClass> : public ModuleDataStorageBase
    {
    protected:
        ModuleDataStorage() {}
        ~ModuleDataStorage() { CleanUpData(); }
        void CleanUpData() override
        {
            if (!num_patterns_.empty())
            {
                ChannelIndex channel = 0;
                for (const auto& num_patterns : num_patterns_)
                {
                    for (PatternIndex pattern_id = 0; pattern_id < num_patterns; ++pattern_id)
                    {
                        delete[] patterns_[channel][pattern_id];
                        patterns_[channel][pattern_id] = nullptr;
                    }
                    delete[] patterns_[channel];
                    patterns_[channel] = nullptr;
                    ++channel;
                }
                patterns_.clear();
            }
            pattern_matrix_.clear();
            num_patterns_.clear();
            pattern_metadata_.clear();
        }

        void SetPatternMatrix() override
        {
            pattern_matrix_.resize(num_channels_);

            for (ChannelIndex channel = 0; channel < num_channels_; ++channel)
            {
                pattern_matrix_[channel].resize(num_orders_);
            }
        }
        void SetNumPatterns() override
        {
            num_patterns_.resize(num_channels_);

            for (ChannelIndex channel = 0; channel < num_channels_; ++channel)
            {
                num_patterns_[channel] = *std::max_element(pattern_matrix_[channel].begin(), pattern_matrix_[channel].end()) + 1;
            }
        }
        void SetPatterns() override
        {
            patterns_.resize(num_channels_);
            if constexpr (!std::is_empty_v<PatternMetadataType>)
            {
                // Only set it if it's going to be used
                pattern_metadata_.resize(num_channels_);
            }

            for (ChannelIndex channel = 0; channel < num_channels_; ++channel)
            {
                const PatternIndex num_patterns = num_patterns_[channel];
                patterns_[channel] = new PatternType[num_patterns];

                for (PatternIndex pattern_id = 0; pattern_id < num_patterns; ++pattern_id)
                {
                    patterns_[channel][pattern_id] = new RowType[num_rows_]();
                    if constexpr (!std::is_empty_v<PatternMetadataType>)
                    {
                        // Only set it if it's going to be used
                        pattern_metadata_[channel].resize(num_patterns);
                    }
                }
            }
        }

    public:
        using RowType = Row<ModuleClass>;
        using PatternType = RowType*; // [row]

        using PatternMatrixType = std::vector<std::vector<PatternIndex>>; // [channel][order]
        using NumPatternsType = std::vector<PatternIndex>; // [channel]
        using PatternStorageType = std::vector<PatternType*>; // [channel][pattern id]
        using PatternMetadataType = PatternMetadata<ModuleClass>;
        using PatternMetadataStorageType = std::vector<std::vector<PatternMetadataType>>; // [channel][pattern id]

        inline PatternIndex GetPatternId(ChannelIndex channel, OrderIndex order) const { return pattern_matrix_[channel][order]; }
        inline void SetPatternId(ChannelIndex channel, OrderIndex order, PatternIndex pattern_id) { pattern_matrix_[channel][order] = pattern_id; }
        inline PatternIndex GetNumPatterns(ChannelIndex channel) const { return num_patterns_[channel]; }
        inline void SetNumPatterns(ChannelIndex channel, PatternIndex num_patterns) { num_patterns_[channel] = num_patterns; }
        inline PatternType GetPattern(ChannelIndex channel, OrderIndex order) const { return patterns_[channel][GetPatternId(channel, order)]; }
        inline void SetPattern(ChannelIndex channel, OrderIndex order, PatternType&& pattern) { patterns_[channel][GetPatternId(channel, order)] = std::move(pattern); } // TODO: Deep copy?
        inline PatternType GetPatternById(ChannelIndex channel, PatternIndex pattern_id) const { return patterns_[channel][pattern_id]; }
        inline void SetPatternById(ChannelIndex channel, PatternIndex pattern_id, PatternType&& pattern) { patterns_[channel][pattern_id] = std::move(pattern); } // TODO: Deep copy?
        inline const RowType& GetRow(ChannelIndex channel, OrderIndex order, RowIndex row) const { return GetPattern(channel, order)[row]; }
        inline void SetRow(ChannelIndex channel, OrderIndex order, RowIndex row, const RowType& row_value) { GetPattern(channel, order)[row] = row_value; }
        inline const RowType& GetRowById(ChannelIndex channel, PatternIndex pattern_id, RowIndex row) const { return GetPatternById(channel, pattern_id)[row]; }
        inline void SetRowById(ChannelIndex channel, PatternIndex pattern_id, RowIndex row, const RowType& row_value) { GetPatternById(channel, pattern_id)[row] = row_value; }
        inline const PatternMetadataType& GetPatternMetadata(ChannelIndex channel, PatternIndex pattern_id) const { return pattern_metadata_[channel][pattern_id]; }
        inline void SetPatternMetadata(ChannelIndex channel, PatternIndex pattern_id, const PatternMetadataType& pattern_metadata) { pattern_metadata_[channel][pattern_id] = pattern_metadata; }

    protected:
        PatternMatrixType pattern_matrix_{}; // Stores patterns IDs for each channel and order in the pattern matrix
        NumPatternsType num_patterns_{}; // Patterns per channel
        PatternStorageType patterns_{}; // [channel][pattern id]
        PatternMetadataStorageType pattern_metadata_{}; // [channel][pattern id]
    };

    template<class ModuleClass>
    class ModuleDataStorage<DataStorageType::kORC, ModuleClass> : public ModuleDataStorageBase
    {
    protected:
        ModuleDataStorage() {}
        ~ModuleDataStorage() { CleanUpData(); }
        void CleanUpData() override
        {
            for (PatternIndex pattern_id = 0; pattern_id < num_patterns_; ++pattern_id)
            {
                for (RowIndex row = 0; row < num_rows_; ++row)
                {
                    delete[] patterns_[pattern_id][row];
                    patterns_[pattern_id][row] = nullptr;
                }
                delete[] patterns_[pattern_id];
                patterns_[pattern_id] = nullptr;
            }
            patterns_.clear();

            pattern_matrix_.clear();
            num_patterns_ = 0;
            pattern_metadata_.clear();
        }

        void SetPatternMatrix() override
        {
            pattern_matrix_.resize(num_orders_);
        }
        void SetNumPatterns() override
        {
            num_patterns_ = *std::max_element(pattern_matrix_.begin(), pattern_matrix_.end()) + 1;
        }
        void SetPatterns() override
        {
            patterns_.resize(num_patterns_);
            if constexpr (!std::is_empty_v<PatternMetadataType>)
            {
                // Only set it if it's going to be used
                pattern_metadata_.resize(num_patterns_);
            }

            for (PatternIndex pattern_id = 0; pattern_id < num_patterns_; ++pattern_id)
            {
                patterns_[pattern_id] = new RowType*[num_rows_];
                for (RowIndex row = 0; row < num_rows_; ++row)
                {
                    patterns_[pattern_id][row] = new RowType[num_channels_]();
                }
            }
        }

    public:
        using RowType = Row<ModuleClass>;
        using PatternType = RowType**; // [row][channel]

        using PatternMatrixType = std::vector<PatternIndex>; // [order] (No per-channel patterns)
        using NumPatternsType = PatternIndex; // (No per-channel patterns)
        using PatternStorageType = std::vector<PatternType>; // [order]
        using PatternMetadataType = PatternMetadata<ModuleClass>;
        using PatternMetadataStorageType = std::vector<PatternMetadataType>; // [pattern id] (No per-channel patterns)

        inline PatternIndex GetPatternId(OrderIndex order) const { return pattern_matrix_[order]; }
        inline void SetPatternId(OrderIndex order, PatternIndex pattern_id) { pattern_matrix_[order] = pattern_id; }
        inline PatternIndex GetNumPatterns() const { return num_patterns_; }
        inline void SetNumPatterns(PatternIndex num_patterns) { num_patterns_ = num_patterns; }
        inline PatternType GetPattern(OrderIndex order) const { return patterns_[GetPatternId(order)]; }
        inline void SetPattern(OrderIndex order, PatternType&& pattern) { patterns_[GetPatternId(order)] = std::move(pattern); } // TODO: Deep copy?
        inline PatternType GetPatternById(PatternIndex pattern_id) const { return patterns_[pattern_id]; }
        inline void SetPatternById(PatternIndex pattern_id, PatternType&& pattern) { patterns_[pattern_id] = std::move(pattern); } // TODO: Deep copy?
        inline const RowType& GetRow(ChannelIndex channel, OrderIndex order, RowIndex row) const { return GetPattern(order)[row][channel]; }
        inline void SetRow(ChannelIndex channel, OrderIndex order, RowIndex row, const RowType& row_value) { GetPattern(order)[row][channel] = row_value; }
        inline const RowType& GetRowById(ChannelIndex channel, PatternIndex pattern_id, RowIndex row) const { return GetPatternById(pattern_id)[row][channel]; }
        inline void SetRowById(ChannelIndex channel, PatternIndex pattern_id, RowIndex row, const RowType& row_value) { GetPatternById(pattern_id)[row][channel] = row_value; }
        inline const PatternMetadataType& GetPatternMetadata(PatternIndex pattern_id) const { return pattern_metadata_[pattern_id]; }
        inline void SetPatternMetadata(PatternIndex pattern_id, const PatternMetadataType& pattern_metadata) { pattern_metadata_[pattern_id] = pattern_metadata; }

    protected:
        PatternMatrixType pattern_matrix_{}; // Stores patterns IDs for each order in the pattern matrix
        NumPatternsType num_patterns_{}; // Number of patterns
        PatternStorageType patterns_{}; // [pattern id]
        PatternMetadataStorageType pattern_metadata_{}; // [pattern id]
    };

    /*
        Define additional ModuleDataStorage specializations here as needed
    */
}

/*
    ModuleData stores and provides access to song data such as
    orders, patterns, rows, and other information.
*/

template<class ModuleClass>
class ModuleData : public detail::ModuleDataStorage<ModuleGlobalData<ModuleClass>::storage_type, ModuleClass>
{
public:
    using RowType = Row<ModuleClass>;
    using ChannelMetadataType = ChannelMetadata<ModuleClass>;
    using PatternMetadataType = PatternMetadata<ModuleClass>;
    using GlobalDataType = ModuleGlobalData<ModuleClass>;

    static constexpr DataStorageType storage_type = GlobalDataType::storage_type;
    static_assert(storage_type != DataStorageType::kNone, "Storage type must be defined for ModuleData through the ModuleGlobalData struct");

    using Storage = typename detail::ModuleDataStorage<storage_type, ModuleClass>;

    ModuleData() { CleanUp(); }
    ~ModuleData() { CleanUp(); }

    // This is the 1st initialization method to call
    void AllocatePatternMatrix(ChannelIndex channels, OrderIndex orders, RowIndex rows)
    {
        Storage::CleanUpData();

        Storage::num_channels_ = channels;
        Storage::num_orders_ = orders;
        Storage::num_rows_ = rows;

        Storage::SetPatternMatrix();
    }

    // This is the 2nd initialization method to call
    void AllocateChannels()
    {
        // Call this after all the pattern IDs are set
        Storage::SetNumPatterns();

        if constexpr (!std::is_empty_v<ChannelMetadataType>)
        {
            // Only set it if it's going to be used
            channel_metadata_.resize(Storage::num_channels_);
        }
    }

    // This is the 3rd and final initialization method to call
    void AllocatePatterns()
    {
        Storage::SetPatterns();
    }

    /////// DIRECT ACCESS GETTERS ///////

    inline const typename Storage::PatternMatrixType& PatternMatrixRef() const { return Storage::pattern_matrix_; }
    inline typename Storage::PatternMatrixType& PatternMatrixRef() { return Storage::pattern_matrix_; }

    inline const typename Storage::NumPatternsType& NumPatternsRef() const { return Storage::num_patterns_; }
    inline typename Storage::NumPatternsType& NumPatternsRef() { return Storage::num_patterns_; }

    inline const typename Storage::PatternStorageType& PatternsRef() const { return Storage::patterns_; }
    inline typename Storage::PatternStorageType& PatternsRef() { return Storage::patterns_; }

    inline const typename Storage::PatternMetadataStorageType& PatternMetadataRef() const { return Storage::pattern_metadata_; }
    inline typename Storage::PatternMetadataStorageType& PatternMetadataRef() { return Storage::pattern_metadata_; }

    inline const std::vector<ChannelMetadataType>& ChannelMetadataRef() const { return channel_metadata_; }
    inline std::vector<ChannelMetadataType>& ChannelMetadataRef() { return channel_metadata_; }

    inline const GlobalDataType& GlobalData() const { return global_data_; }
    inline GlobalDataType& GlobalData() { return global_data_; }

    /////// GETTERS / SETTERS ///////

    inline const ChannelMetadataType& GetChannelMetadata(ChannelIndex channel) const { return channel_metadata_[channel]; }
    inline void SetChannelMetadata(ChannelIndex channel, const ChannelMetadataType& channelMetadata) { channel_metadata_[channel] = channelMetadata; }

    static inline constexpr DataStorageType GetStorageType() { return storage_type; }

    /////// Other

    void CleanUp()
    {
        Storage::CleanUpData();

        Storage::num_channels_ = 0;
        Storage::num_orders_ = 0;
        Storage::num_rows_ = 0;

        channel_metadata_.clear();
        global_data_ = {};
    }

private:

    // Metadata (optional module-specific info)
    std::vector<ChannelMetadataType> channel_metadata_; // [channel]

    // Global information about a particular module file
    GlobalDataType global_data_;
};

} // namespace d2m
