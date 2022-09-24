/*
    data.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines a class for storing and accessing module data (orders, patterns, rows, etc.)
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
    None,
    COR,  // Iteration order: Channels --> Orders --> (Pattern) Rows
    ORC,  // Iteration order: Orders --> (Pattern) Rows --> Channels
};

/*
    Global data for a module. This is information such as the title and author.
    Can be customized if a module type has more global information to be stored.
*/

template<DataStorageType DataStorage = DataStorageType::None>
struct ModuleGlobalDataDefault
{
    static constexpr DataStorageType StorageType = DataStorage;
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

template <class ModuleClass>
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
        inline ChannelIndex GetNumChannels() const { return m_NumChannels; }
        inline OrderIndex GetNumOrders() const { return m_NumOrders; }
        inline RowIndex GetNumRows() const { return m_NumRows; }
    protected:
        virtual void CleanUpData() = 0;
        virtual void SetPatternMatrix() = 0;
        virtual void SetNumPatterns() = 0;
        virtual void SetPatterns() = 0;
        ChannelIndex m_NumChannels;
        OrderIndex m_NumOrders;    // Total orders (pattern matrix rows)
        RowIndex m_NumRows;      // Rows per pattern
    };

    template<DataStorageType StorageType, class ModuleClass>
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
    class ModuleDataStorage<DataStorageType::COR, ModuleClass> : public ModuleDataStorageBase
    {
    protected:
        ModuleDataStorage() {}
        ~ModuleDataStorage() { CleanUpData(); }
        void CleanUpData() override
        {
            if (!m_NumPatterns.empty())
            {
                ChannelIndex channel = 0;
                for (const auto& numPatterns : m_NumPatterns)
                {
                    for (PatternIndex patternId = 0; patternId < numPatterns; ++patternId)
                    {
                        delete[] m_Patterns[channel][patternId];
                        m_Patterns[channel][patternId] = nullptr;
                    }
                    delete[] m_Patterns[channel];
                    m_Patterns[channel] = nullptr;
                    ++channel;
                }
                m_Patterns.clear();
            }
            m_PatternMatrix.clear();
            m_NumPatterns.clear();
            m_PatternMetadata.clear();
        }

        void SetPatternMatrix() override
        {
            m_PatternMatrix.resize(m_NumChannels);

            for (ChannelIndex channel = 0; channel < m_NumChannels; ++channel)
            {
                m_PatternMatrix[channel].resize(m_NumOrders);
            }
        }
        void SetNumPatterns() override
        {
            m_NumPatterns.resize(m_NumChannels);

            for (ChannelIndex channel = 0; channel < m_NumChannels; ++channel)
            {
                m_NumPatterns[channel] = *std::max_element(m_PatternMatrix[channel].begin(), m_PatternMatrix[channel].end()) + 1;
            }
        }
        void SetPatterns() override
        {
            m_Patterns.resize(m_NumChannels);
            if constexpr (!std::is_empty_v<PatternMetadataType>)
            {
                // Only set it if it's going to be used
                m_PatternMetadata.resize(m_NumChannels);
            }

            for (ChannelIndex channel = 0; channel < m_NumChannels; ++channel)
            {
                const PatternIndex numPatterns = m_NumPatterns[channel];
                m_Patterns[channel] = new PatternType[numPatterns];

                for (PatternIndex patternId = 0; patternId < numPatterns; ++patternId)
                {
                    m_Patterns[channel][patternId] = new RowType[m_NumRows]();
                    if constexpr (!std::is_empty_v<PatternMetadataType>)
                    {
                        // Only set it if it's going to be used
                        m_PatternMetadata[channel].resize(numPatterns);
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

        inline PatternIndex GetPatternId(ChannelIndex channel, OrderIndex order) const { return m_PatternMatrix[channel][order]; }
        inline void SetPatternId(ChannelIndex channel, OrderIndex order, PatternIndex patternId) { m_PatternMatrix[channel][order] = patternId; }
        inline PatternIndex GetNumPatterns(ChannelIndex channel) const { return m_NumPatterns[channel]; }
        inline void SetNumPatterns(ChannelIndex channel, PatternIndex numPatterns) { m_NumPatterns[channel] = numPatterns; }
        inline PatternType GetPattern(ChannelIndex channel, OrderIndex order) const { return m_Patterns[channel][GetPatternId(channel, order)]; }
        inline void SetPattern(ChannelIndex channel, OrderIndex order, PatternType&& pattern) { m_Patterns[channel][GetPatternId(channel, order)] = std::move(pattern); } // TODO: Deep copy?
        inline PatternType GetPatternById(ChannelIndex channel, PatternIndex patternId) const { return m_Patterns[channel][patternId]; }
        inline void SetPatternById(ChannelIndex channel, PatternIndex patternId, PatternType&& pattern) { m_Patterns[channel][patternId] = std::move(pattern); } // TODO: Deep copy?
        inline const RowType& GetRow(ChannelIndex channel, OrderIndex order, RowIndex row) const { return GetPattern(channel, order)[row]; }
        inline void SetRow(ChannelIndex channel, OrderIndex order, RowIndex row, const RowType& rowValue) { GetPattern(channel, order)[row] = rowValue; }
        inline const RowType& GetRowById(ChannelIndex channel, PatternIndex patternId, RowIndex row) const { return GetPatternById(channel, patternId)[row]; }
        inline void SetRowById(ChannelIndex channel, PatternIndex patternId, RowIndex row, const RowType& rowValue) { GetPatternById(channel, patternId)[row] = rowValue; }
        inline const PatternMetadataType& GetPatternMetadata(ChannelIndex channel, PatternIndex patternId) const { return m_PatternMetadata[channel][patternId]; }
        inline void SetPatternMetadata(ChannelIndex channel, PatternIndex patternId, const PatternMetadataType& patternMetadata) { m_PatternMetadata[channel][patternId] = patternMetadata; }

    protected:
        PatternMatrixType m_PatternMatrix{}; // Stores patterns IDs for each channel and order in the pattern matrix
        NumPatternsType m_NumPatterns{}; // Patterns per channel
        PatternStorageType m_Patterns{}; // [channel][pattern id]
        PatternMetadataStorageType m_PatternMetadata{}; // [channel][pattern id]
    };

    template<class ModuleClass>
    class ModuleDataStorage<DataStorageType::ORC, ModuleClass> : public ModuleDataStorageBase
    {
    protected:
        ModuleDataStorage() {}
        ~ModuleDataStorage() { CleanUpData(); }
        void CleanUpData() override
        {
            for (PatternIndex patternId = 0; patternId < m_NumPatterns; ++patternId)
            {
                for (RowIndex row = 0; row < m_NumRows; ++row)
                {
                    delete[] m_Patterns[patternId][row];
                    m_Patterns[patternId][row] = nullptr;
                }
                delete[] m_Patterns[patternId];
                m_Patterns[patternId] = nullptr;
            }
            m_Patterns.clear();

            m_PatternMatrix.clear();
            m_NumPatterns = 0;
            m_PatternMetadata.clear();
        }

        void SetPatternMatrix() override
        {
            m_PatternMatrix.resize(m_NumOrders);
        }
        void SetNumPatterns() override
        {
            m_NumPatterns = *std::max_element(m_PatternMatrix.begin(), m_PatternMatrix.end()) + 1;
        }
        void SetPatterns() override
        {
            m_Patterns.resize(m_NumPatterns);
            if constexpr (!std::is_empty_v<PatternMetadataType>)
            {
                // Only set it if it's going to be used
                m_PatternMetadata.resize(m_NumPatterns);
            }

            for (PatternIndex patternId = 0; patternId < m_NumPatterns; ++patternId)
            {
                m_Patterns[patternId] = new RowType*[m_NumRows];
                for (RowIndex row = 0; row < m_NumRows; ++row)
                {
                    m_Patterns[patternId][row] = new RowType[m_NumChannels]();
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

        inline PatternIndex GetPatternId(OrderIndex order) const { return m_PatternMatrix[order]; }
        inline void SetPatternId(OrderIndex order, PatternIndex patternId) { m_PatternMatrix[order] = patternId; }
        inline PatternIndex GetNumPatterns() const { return m_NumPatterns; }
        inline void SetNumPatterns(PatternIndex numPatterns) { m_NumPatterns = numPatterns; }
        inline PatternType GetPattern(OrderIndex order) const { return m_Patterns[GetPatternId(order)]; }
        inline void SetPattern(OrderIndex order, PatternType&& pattern) { m_Patterns[GetPatternId(order)] = std::move(pattern); } // TODO: Deep copy?
        inline PatternType GetPatternById(PatternIndex patternId) const { return m_Patterns[patternId]; }
        inline void SetPatternById(PatternIndex patternId, PatternType&& pattern) { m_Patterns[patternId] = std::move(pattern); } // TODO: Deep copy?
        inline const RowType& GetRow(ChannelIndex channel, OrderIndex order, RowIndex row) const { return GetPattern(order)[row][channel]; }
        inline void SetRow(ChannelIndex channel, OrderIndex order, RowIndex row, const RowType& rowValue) { GetPattern(order)[row][channel] = rowValue; }
        inline const RowType& GetRowById(ChannelIndex channel, PatternIndex patternId, RowIndex row) const { return GetPatternById(patternId)[row][channel]; }
        inline void SetRowById(ChannelIndex channel, PatternIndex patternId, RowIndex row, const RowType& rowValue) { GetPatternById(patternId)[row][channel] = rowValue; }
        inline const PatternMetadataType& GetPatternMetadata(PatternIndex patternId) const { return m_PatternMetadata[patternId]; }
        inline void SetPatternMetadata(PatternIndex patternId, const PatternMetadataType& patternMetadata) { m_PatternMetadata[patternId] = patternMetadata; }

    protected:
        PatternMatrixType m_PatternMatrix{}; // Stores patterns IDs for each order in the pattern matrix
        NumPatternsType m_NumPatterns{}; // Number of patterns
        PatternStorageType m_Patterns{}; // [pattern id]
        PatternMetadataStorageType m_PatternMetadata{}; // [pattern id]
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
class ModuleData : public detail::ModuleDataStorage<ModuleGlobalData<ModuleClass>::StorageType, ModuleClass>
{
public:
    using RowType = Row<ModuleClass>;
    using ChannelMetadataType = ChannelMetadata<ModuleClass>;
    using PatternMetadataType = PatternMetadata<ModuleClass>;
    using GlobalDataType = ModuleGlobalData<ModuleClass>;

    static constexpr DataStorageType StorageType = GlobalDataType::StorageType;
    static_assert(StorageType != DataStorageType::None, "Storage type must be defined for ModuleData through the ModuleGlobalData struct");

    using Storage = typename detail::ModuleDataStorage<StorageType, ModuleClass>;

    ModuleData() { CleanUp(); }
    ~ModuleData() { CleanUp(); }

    // This is the 1st initialization method to call
    void AllocatePatternMatrix(ChannelIndex channels, OrderIndex orders, RowIndex rows)
    {
        Storage::CleanUpData();

        Storage::m_NumChannels = channels;
        Storage::m_NumOrders = orders;
        Storage::m_NumRows = rows;

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
            m_ChannelMetadata.resize(Storage::m_NumChannels);
        }
    }

    // This is the 3rd and final initialization method to call
    void AllocatePatterns()
    {
        Storage::SetPatterns();
    }

    /////// DIRECT ACCESS GETTERS ///////

    inline const typename Storage::PatternMatrixType& PatternMatrixRef() const { return Storage::m_PatternMatrix; }
    inline typename Storage::PatternMatrixType& PatternMatrixRef() { return Storage::m_PatternMatrix; }

    inline const typename Storage::NumPatternsType& NumPatternsRef() const { return Storage::m_NumPatterns; }
    inline typename Storage::NumPatternsType& NumPatternsRef() { return Storage::m_NumPatterns; }

    inline const typename Storage::PatternStorageType& PatternsRef() const { return Storage::m_Patterns; }
    inline typename Storage::PatternStorageType& PatternsRef() { return Storage::m_Patterns; }

    inline const typename Storage::PatternMetadataStorageType& PatternMetadataRef() const { return Storage::m_PatternMetadata; }
    inline typename Storage::PatternMetadataStorageType& PatternMetadataRef() { return Storage::m_PatternMetadata; }

    inline const std::vector<ChannelMetadataType>& ChannelMetadataRef() const { return m_ChannelMetadata; }
    inline std::vector<ChannelMetadataType>& ChannelMetadataRef() { return m_ChannelMetadata; }

    inline const GlobalDataType& GlobalData() const { return m_GlobalData; }
    inline GlobalDataType& GlobalData() { return m_GlobalData; }

    /////// GETTERS / SETTERS ///////

    inline const ChannelMetadataType& GetChannelMetadata(ChannelIndex channel) const { return m_ChannelMetadata[channel]; }
    inline void SetChannelMetadata(ChannelIndex channel, const ChannelMetadataType& channelMetadata) { m_ChannelMetadata[channel] = channelMetadata; }

    static inline constexpr DataStorageType GetStorageType() { return StorageType; }

    /////// Other

    void CleanUp()
    {
        Storage::CleanUpData();

        Storage::m_NumChannels = 0;
        Storage::m_NumOrders = 0;
        Storage::m_NumRows = 0;

        m_ChannelMetadata.clear();
        m_GlobalData = {};
    }

private:

    // Metadata (optional module-specific info)
    std::vector<ChannelMetadataType> m_ChannelMetadata; // [channel]

    // Global information about a particular module file
    GlobalDataType m_GlobalData;
};

} // namespace d2m
