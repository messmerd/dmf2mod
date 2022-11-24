/*
    data.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines a class template for storing and accessing module data (orders, patterns, rows, etc.)
*/

#pragma once

#include "note.h"

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
        inline ChannelIndex GetNumChannels() const { return m_num_channels; }
        inline OrderIndex GetNumOrders() const { return m_num_orders; }
        inline RowIndex GetNumRows() const { return m_num_rows; }
    protected:
        virtual void CleanUpData() = 0;
        virtual void SetPatternMatrix() = 0;
        virtual void SetNumPatterns() = 0;
        virtual void SetPatterns() = 0;
        ChannelIndex m_num_channels;
        OrderIndex m_num_orders;    // Total orders (pattern matrix rows)
        RowIndex m_num_rows;      // Rows per pattern
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
            if (!m_num_patterns.empty())
            {
                ChannelIndex channel = 0;
                for (const auto& num_patterns : m_num_patterns)
                {
                    for (PatternIndex pattern_id = 0; pattern_id < num_patterns; ++pattern_id)
                    {
                        delete[] m_patterns[channel][pattern_id];
                        m_patterns[channel][pattern_id] = nullptr;
                    }
                    delete[] m_patterns[channel];
                    m_patterns[channel] = nullptr;
                    ++channel;
                }
                m_patterns.clear();
            }
            m_pattern_matrix.clear();
            m_num_patterns.clear();
            m_pattern_metadata.clear();
        }

        void SetPatternMatrix() override
        {
            m_pattern_matrix.resize(m_num_channels);

            for (ChannelIndex channel = 0; channel < m_num_channels; ++channel)
            {
                m_pattern_matrix[channel].resize(m_num_orders);
            }
        }
        void SetNumPatterns() override
        {
            m_num_patterns.resize(m_num_channels);

            for (ChannelIndex channel = 0; channel < m_num_channels; ++channel)
            {
                m_num_patterns[channel] = *std::max_element(m_pattern_matrix[channel].begin(), m_pattern_matrix[channel].end()) + 1;
            }
        }
        void SetPatterns() override
        {
            m_patterns.resize(m_num_channels);
            if constexpr (!std::is_empty_v<PatternMetadataType>)
            {
                // Only set it if it's going to be used
                m_pattern_metadata.resize(m_num_channels);
            }

            for (ChannelIndex channel = 0; channel < m_num_channels; ++channel)
            {
                const PatternIndex num_patterns = m_num_patterns[channel];
                m_patterns[channel] = new PatternType[num_patterns];

                for (PatternIndex pattern_id = 0; pattern_id < num_patterns; ++pattern_id)
                {
                    m_patterns[channel][pattern_id] = new RowType[m_num_rows]();
                    if constexpr (!std::is_empty_v<PatternMetadataType>)
                    {
                        // Only set it if it's going to be used
                        m_pattern_metadata[channel].resize(num_patterns);
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

        inline PatternIndex GetPatternId(ChannelIndex channel, OrderIndex order) const { return m_pattern_matrix[channel][order]; }
        inline void SetPatternId(ChannelIndex channel, OrderIndex order, PatternIndex pattern_id) { m_pattern_matrix[channel][order] = pattern_id; }
        inline PatternIndex GetNumPatterns(ChannelIndex channel) const { return m_num_patterns[channel]; }
        inline void SetNumPatterns(ChannelIndex channel, PatternIndex num_patterns) { m_num_patterns[channel] = num_patterns; }
        inline PatternType GetPattern(ChannelIndex channel, OrderIndex order) const { return m_patterns[channel][GetPatternId(channel, order)]; }
        inline void SetPattern(ChannelIndex channel, OrderIndex order, PatternType&& pattern) { m_patterns[channel][GetPatternId(channel, order)] = std::move(pattern); } // TODO: Deep copy?
        inline PatternType GetPatternById(ChannelIndex channel, PatternIndex pattern_id) const { return m_patterns[channel][pattern_id]; }
        inline void SetPatternById(ChannelIndex channel, PatternIndex pattern_id, PatternType&& pattern) { m_patterns[channel][pattern_id] = std::move(pattern); } // TODO: Deep copy?
        inline const RowType& GetRow(ChannelIndex channel, OrderIndex order, RowIndex row) const { return GetPattern(channel, order)[row]; }
        inline void SetRow(ChannelIndex channel, OrderIndex order, RowIndex row, const RowType& row_value) { GetPattern(channel, order)[row] = row_value; }
        inline const RowType& GetRowById(ChannelIndex channel, PatternIndex pattern_id, RowIndex row) const { return GetPatternById(channel, pattern_id)[row]; }
        inline void SetRowById(ChannelIndex channel, PatternIndex pattern_id, RowIndex row, const RowType& row_value) { GetPatternById(channel, pattern_id)[row] = row_value; }
        inline const PatternMetadataType& GetPatternMetadata(ChannelIndex channel, PatternIndex pattern_id) const { return m_pattern_metadata[channel][pattern_id]; }
        inline void SetPatternMetadata(ChannelIndex channel, PatternIndex pattern_id, const PatternMetadataType& pattern_metadata) { m_pattern_metadata[channel][pattern_id] = pattern_metadata; }

    protected:
        PatternMatrixType m_pattern_matrix{}; // Stores patterns IDs for each channel and order in the pattern matrix
        NumPatternsType m_num_patterns{}; // Patterns per channel
        PatternStorageType m_patterns{}; // [channel][pattern id]
        PatternMetadataStorageType m_pattern_metadata{}; // [channel][pattern id]
    };

    template<class ModuleClass>
    class ModuleDataStorage<DataStorageType::kORC, ModuleClass> : public ModuleDataStorageBase
    {
    protected:
        ModuleDataStorage() {}
        ~ModuleDataStorage() { CleanUpData(); }
        void CleanUpData() override
        {
            for (PatternIndex pattern_id = 0; pattern_id < m_num_patterns; ++pattern_id)
            {
                for (RowIndex row = 0; row < m_num_rows; ++row)
                {
                    delete[] m_patterns[pattern_id][row];
                    m_patterns[pattern_id][row] = nullptr;
                }
                delete[] m_patterns[pattern_id];
                m_patterns[pattern_id] = nullptr;
            }
            m_patterns.clear();

            m_pattern_matrix.clear();
            m_num_patterns = 0;
            m_pattern_metadata.clear();
        }

        void SetPatternMatrix() override
        {
            m_pattern_matrix.resize(m_num_orders);
        }
        void SetNumPatterns() override
        {
            m_num_patterns = *std::max_element(m_pattern_matrix.begin(), m_pattern_matrix.end()) + 1;
        }
        void SetPatterns() override
        {
            m_patterns.resize(m_num_patterns);
            if constexpr (!std::is_empty_v<PatternMetadataType>)
            {
                // Only set it if it's going to be used
                m_pattern_metadata.resize(m_num_patterns);
            }

            for (PatternIndex pattern_id = 0; pattern_id < m_num_patterns; ++pattern_id)
            {
                m_patterns[pattern_id] = new RowType*[m_num_rows];
                for (RowIndex row = 0; row < m_num_rows; ++row)
                {
                    m_patterns[pattern_id][row] = new RowType[m_num_channels]();
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

        inline PatternIndex GetPatternId(OrderIndex order) const { return m_pattern_matrix[order]; }
        inline void SetPatternId(OrderIndex order, PatternIndex pattern_id) { m_pattern_matrix[order] = pattern_id; }
        inline PatternIndex GetNumPatterns() const { return m_num_patterns; }
        inline void SetNumPatterns(PatternIndex num_patterns) { m_num_patterns = num_patterns; }
        inline PatternType GetPattern(OrderIndex order) const { return m_patterns[GetPatternId(order)]; }
        inline void SetPattern(OrderIndex order, PatternType&& pattern) { m_patterns[GetPatternId(order)] = std::move(pattern); } // TODO: Deep copy?
        inline PatternType GetPatternById(PatternIndex pattern_id) const { return m_patterns[pattern_id]; }
        inline void SetPatternById(PatternIndex pattern_id, PatternType&& pattern) { m_patterns[pattern_id] = std::move(pattern); } // TODO: Deep copy?
        inline const RowType& GetRow(ChannelIndex channel, OrderIndex order, RowIndex row) const { return GetPattern(order)[row][channel]; }
        inline void SetRow(ChannelIndex channel, OrderIndex order, RowIndex row, const RowType& row_value) { GetPattern(order)[row][channel] = row_value; }
        inline const RowType& GetRowById(ChannelIndex channel, PatternIndex pattern_id, RowIndex row) const { return GetPatternById(pattern_id)[row][channel]; }
        inline void SetRowById(ChannelIndex channel, PatternIndex pattern_id, RowIndex row, const RowType& row_value) { GetPatternById(pattern_id)[row][channel] = row_value; }
        inline const PatternMetadataType& GetPatternMetadata(PatternIndex pattern_id) const { return m_pattern_metadata[pattern_id]; }
        inline void SetPatternMetadata(PatternIndex pattern_id, const PatternMetadataType& pattern_metadata) { m_pattern_metadata[pattern_id] = pattern_metadata; }

    protected:
        PatternMatrixType m_pattern_matrix{}; // Stores patterns IDs for each order in the pattern matrix
        NumPatternsType m_num_patterns{}; // Number of patterns
        PatternStorageType m_patterns{}; // [pattern id]
        PatternMetadataStorageType m_pattern_metadata{}; // [pattern id]
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

        Storage::m_num_channels = channels;
        Storage::m_num_orders = orders;
        Storage::m_num_rows = rows;

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
            m_channel_metadata.resize(Storage::m_num_channels);
        }
    }

    // This is the 3rd and final initialization method to call
    void AllocatePatterns()
    {
        Storage::SetPatterns();
    }

    /////// DIRECT ACCESS GETTERS ///////

    inline const typename Storage::PatternMatrixType& PatternMatrixRef() const { return Storage::m_pattern_matrix; }
    inline typename Storage::PatternMatrixType& PatternMatrixRef() { return Storage::m_pattern_matrix; }

    inline const typename Storage::NumPatternsType& NumPatternsRef() const { return Storage::m_num_patterns; }
    inline typename Storage::NumPatternsType& NumPatternsRef() { return Storage::m_num_patterns; }

    inline const typename Storage::PatternStorageType& PatternsRef() const { return Storage::m_patterns; }
    inline typename Storage::PatternStorageType& PatternsRef() { return Storage::m_patterns; }

    inline const typename Storage::PatternMetadataStorageType& PatternMetadataRef() const { return Storage::m_pattern_metadata; }
    inline typename Storage::PatternMetadataStorageType& PatternMetadataRef() { return Storage::m_pattern_metadata; }

    inline const std::vector<ChannelMetadataType>& ChannelMetadataRef() const { return m_channel_metadata; }
    inline std::vector<ChannelMetadataType>& ChannelMetadataRef() { return m_channel_metadata; }

    inline const GlobalDataType& GlobalData() const { return m_global_data; }
    inline GlobalDataType& GlobalData() { return m_global_data; }

    /////// GETTERS / SETTERS ///////

    inline const ChannelMetadataType& GetChannelMetadata(ChannelIndex channel) const { return m_channel_metadata[channel]; }
    inline void SetChannelMetadata(ChannelIndex channel, const ChannelMetadataType& channelMetadata) { m_channel_metadata[channel] = channelMetadata; }

    static inline constexpr DataStorageType GetStorageType() { return storage_type; }

    /////// Other

    void CleanUp()
    {
        Storage::CleanUpData();

        Storage::m_num_channels = 0;
        Storage::m_num_orders = 0;
        Storage::m_num_rows = 0;

        m_channel_metadata.clear();
        m_global_data = {};
    }

private:

    // Metadata (optional module-specific info)
    std::vector<ChannelMetadataType> m_channel_metadata; // [channel]

    // Global information about a particular module file
    GlobalDataType m_global_data;
};

} // namespace d2m
