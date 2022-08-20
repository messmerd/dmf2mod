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

/*
    Global data for a module. This is information such as the title and author.
    Can be customized if a module type has more global information to be stored.
*/

namespace PatternStorageType
{
    enum PatternStorageType : uint16_t
    {
        NONE = 0,
        COR,
        ORC,
    };
};

template <uint16_t StorageType = PatternStorageType::COR>
struct ModuleGlobalDataGeneric
{
    static constexpr uint16_t _StorageType = StorageType;
    std::string title;
    std::string author;
};

template <class ModuleClass>
struct ModuleGlobalData : public ModuleGlobalDataGeneric<> {};

/*
    Different modules have significantly different per-channel row contents, so
    providing one single generic implementation for use by every module doesn't
    make much sense. Each module should provide their own Row implementation.
*/

struct RowGeneric
{
    NoteSlot note;
};

template <class ModuleClass>
struct Row : public RowGeneric {};

/*
    Some module formats contain additional data for each channel or row.
    Specializations for ChannelMetadata and PatternMetadata can be created
    for any module format which requires it.
*/

struct ChannelMetadataGeneric
{
    // No defaults at this time
};

template <class ModuleClass>
struct ChannelMetadata : public ChannelMetadata {};

struct PatternMetadataGeneric
{
    // No defaults at this time
};

template <class ModuleClass>
struct PatternMetadata : public PatternMetadataGeneric {};


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
        virtual void CleanUp() = 0;
        virtual void AllocatePatternMatrix(unsigned channels, unsigned orders) = 0;
        virtual void AllocatePatterns() = 0;
        virtual void AllocateChannels(unsigned channels) = 0;
    };

    template <uint16_t StorageType, class ModuleClass>
    class ModuleDataStorage
    {
    public:
        static_assert(false, "This non-specialized primary template should never be used");
        void CleanUp() override {}
        void AllocatePatternMatrix(unsigned channels, unsigned orders) override {}
        void AllocatePatterns(unsigned channels, unsigned patterns, unsigned rows) override {}
        void AllocateChannels(unsigned channels) override {}
    };

    template <class ModuleClass>
    class ModuleDataStorage<PatternStorageType::COR, ModuleClass>
    {
    public:
        ModuleDataStorage() {}
        ~ModuleDataStorage() { CleanUp(); }
        void CleanUp()
        {
            if (!m_NumPatterns.empty())
            {
                unsigned channel = 0;
                for (const auto& numPatterns : m_NumPatterns)
                {
                    for (unsigned patternId = 0; patternId < numPatterns; ++patternId)
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

        void AllocatePatternMatrix(unsigned channels, unsigned orders) override
        {
            // m_PatterMatrixValues[channel][pattern matrix row]
            m_PatternMatrix.resize(channels);

            for (unsigned i = 0; i < channels; ++i)
            {
                m_PatternMatrix[i].resize(orders);
            }
        }
        void AllocatePatterns(unsigned channels, unsigned patterns, unsigned rows) override
        {
            // m_Patterns[channel][pattern id (not order!)][pattern row number]
            m_Patterns.resize(channels);
            if constexpr (!std::is_empty_v<PatternMetadataType>)
            {
                // Only set it if it's going to be used
                m_PatternMetadata.resize(channels);
            }

            for (unsigned channel = 0; channel < channels; ++channel)
            {
                m_Patterns[channel] = new PatternType[patterns];

                for (unsigned patternId = 0; patternId < patterns; ++patternId)
                {
                    m_Patterns[channel][patternId] = new RowType[rows]();
                    if constexpr (!std::is_empty_v<PatternMetadataType>)
                    {
                        // Only set it if it's going to be used
                        m_PatternMetadata[channel].resize(patterns);
                    }
                }
            }
        }
        void AllocateChannels(unsigned channels) override
        {
            m_NumPatterns.resize(channels);

            for (unsigned chan = 0; chan < channels; ++chan)
            {
                m_NumPatterns[chan] = *std::max_element(m_PatternMatrix[chan].begin(), m_PatternMatrix[chan].end()) + 1;
            }
        }

        using RowType = Row<ModuleClass>;
        using PatternType = RowType*; // [row]

        using PatternMatrixType = std::vector<std::vector<uint8_t>>; // [channel][order]
        using NumPatternsType = std::vector<uint8_t>; // [channel]
        using PatternStorageType = std::vector<PatternType*>; // [channel][pattern id]
        using PatternMetadataType = PatternMetadata<ModuleClass>;
        using PatternMetadataStorageType = std::vector<std::vector<PatternMetadataType>>; // [channel][pattern id]

    protected:
        PatternMatrixType m_PatternMatrix{};  // Stores patterns IDs for each order and channel in the pattern matrix
        NumPatternsType m_NumPatterns{}; // Patterns per channel
        PatternStorageType m_Patterns{}; // [channel][pattern id]
        PatternMetadataStorageType m_PatternMetadata{}; // [channel][pattern id]
    };

    /*
        TODO: Define additional ModuleDataStorage specializations here
    */
}

/*
    ModuleData stores and provides access to song data such as
    orders, patterns, rows, and other information.
*/

template <class ModuleClass>
class ModuleData : public detail::ModuleDataStorage<ModuleGlobalData<ModuleClass>::_StorageType, ModuleClass>
{
public:
    using RowType = Row<ModuleClass>;
    using ChannelMetadataType = ChannelMetadata<ModuleClass>;
    using PatternMetadataType = PatternMetadata<ModuleClass>;
    using GlobalDataType = ModuleGlobalData<ModuleClass>;

    static constexpr uint16_t m_StorageType = GlobalDataType::_StorageType;
    static_assert(m_StorageType != 0, "Storage type must be defined for ModuleData through the ModuleGlobalData struct");

    using Storage = detail::ModuleDataStorage<m_StorageType, ModuleClass>;

    ModuleData() { CleanUp(); }
    ~ModuleData() { CleanUp(); }

    // This is the 1st initialization method to call
    void AllocatePatternMatrix(unsigned channels, unsigned orders, unsigned rows)
    {
        CleanUp();
        Storage::CleanUp();

        m_NumChannels = channels;
        m_NumOrders = orders;
        m_NumRows = rows;

        Storage::AllocatePatternMatrix(channels, orders);
    }

    // This is the 2nd initialization method to call
    void AllocatePatterns()
    {
        Storage::AllocatePatterns();
    }

    // This is the 3rd and final initialization method to call
    void AllocateChannels()
    {
        // Call this after all the pattern IDs are set
        Storage::AllocateChannels(m_NumChannels);

        if constexpr (!std::is_empty_v<ChannelMetadataType>)
        {
            // Only set it if it's going to be used
            m_ChannelMetadata.resize(m_NumChannels);
        }
    }

    /////// DIRECT ACCESS GETTERS ///////

    inline const PatternMatrixType& PatternMatrixRef() const { return m_PatternMatrix; }
    inline PatternMatrixType& PatternMatrixRef() { return m_PatternMatrix; }

    inline const NumPatternsType& NumPatternsRef() const { return m_NumPatterns; }
    inline NumPatternsType& NumPatternsRef() { return m_NumPatterns; }

    inline const PatternStorageType& PatternsRef() const { return m_Patterns; }
    inline PatternStorageType& PatternsRef() { return m_Patterns; }

    inline const PatternMetadataStorageType& PatternMetadataRef() const { return m_PatternMetadata; }
    inline PatternMetadataStorageType& PatternMetadataRef() { return m_PatternMetadata; }

    inline const std::vector<ChannelMetadataType>& ChannelMetadataRef() const { return m_ChannelMetadata; }
    inline std::vector<ChannelMetadataType>& ChannelMetadataRef() { return m_ChannelMetadata; }

    inline const std::unique_ptr<GlobalDataType>& GlobalData() const { return m_GlobalData; }
    inline std::unique_ptr<GlobalDataType>& GlobalData() { return m_GlobalData; }

    /////// GETTERS / SETTERS ///////

    inline unsigned GetNumChannels() const { return m_NumChannels; }
    inline unsigned GetNumOrders() const { return m_NumOrders; }
    inline unsigned GetNumRows() const { return m_NumRows; }

    inline const ChannelMetadataType& GetChannelMetadata(unsigned channel) const { return m_ChannelMetadata[channel]; }
    inline void SetChannelMetadata(unsigned channel, const ChannelMetadataType& channelMetadata) { m_ChannelMetadata[channel] = channelMetadata; }

    static inline constexpr PatternStorageType GetStorageType() const { return m_StorageType; }

    /////// StorageType-dependent getters/setters (for convenience) ///////

    // TODO: Define these in the specializations instead

    // PatternId
    if constexpr (m_StorageType == PatternStorageType::ORC) {
        inline uint8_t GetPatternId(unsigned order) const { return m_PatternMatrix[order]; }
        inline void SetPatternId(unsigned order, uint8_t patternId) { m_PatternMatrix[order] = patternId; }
    } else if constexpr (m_StorageType == PatternStorageType::COR) {
        inline uint8_t GetPatternId(unsigned channel, unsigned order) const { return m_PatternMatrix[channel][order]; }
        inline void SetPatternId(unsigned channel, unsigned order, uint8_t patternId) { m_PatternMatrix[channel][order] = patternId; }
    }

    // NumPatterns
    if constexpr (m_StorageType == PatternStorageType::ORC) {
        inline uint8_t GetNumPatterns() const { return m_NumPatterns; }
        inline void SetNumPatterns(uint8_t numPatterns) { m_NumPatterns = numPatterns; }
    } else if constexpr (m_StorageType == PatternStorageType::COR) {
        inline uint8_t GetNumPatterns(unsigned channel) const { return m_NumPatterns[channel]; }
        inline void SetNumPatterns(unsigned channel, uint8_t numPatterns) { m_NumPatterns[channel] = numPatterns; }
    }

    // Pattern / PatternById
    if constexpr (m_StorageType == PatternStorageType::ORC) {
        inline PatternType GetPattern(unsigned order) const { return m_Patterns[GetPatternId(order)]; }
        inline void SetPattern(unsigned order, PatternType&& pattern) { m_Patterns[GetPatternId(order)] = std::move(pattern); } // TODO: Deep copy?
        inline PatternType GetPatternById(unsigned patternId) const { return m_Patterns[patternId]; }
        inline void SetPatternById(unsigned patternId, PatternType&& pattern) { m_Patterns[patternId] = std::move(pattern); } // TODO: Deep copy?
    } else if constexpr (m_StorageType == PatternStorageType::COR) {
        inline PatternType GetPattern(unsigned channel, unsigned order) const { return m_Patterns[channel][GetPatternId(channel, order)]; }
        inline void SetPattern(unsigned channel, unsigned order, PatternType&& pattern) { m_Patterns[channel][GetPatternId(channel, order)] = std::move(pattern); } // TODO: Deep copy?
        inline PatternType GetPatternById(unsigned channel, unsigned patternId) const { return m_Patterns[channel][patternId]; }
        inline void SetPatternById(unsigned channel, unsigned patternId, PatternType&& pattern) { m_Patterns[channel][patternId] = std::move(pattern); } // TODO: Deep copy?
    }

    // Row / RowById
    if constexpr (m_StorageType == PatternStorageType::ORC) {
        inline const RowType& GetRow(unsigned channel, unsigned order, unsigned row) const { return GetPattern(order)[row][channel]; }
        inline void SetRow(unsigned channel, unsigned order, unsigned row, const RowType& rowValue) { GetPattern(order)[row][channel] = rowValue; }
        inline const RowType& GetRowById(unsigned channel, unsigned patternId, unsigned row) const { return GetPatternById(patternId)[row][channel]; }
        inline void SetRowById(unsigned channel, unsigned patternId, unsigned row, const RowType& rowValue) { GetPatternById(patternId)[row][channel] = rowValue; }
    } else if constexpr (m_StorageType == PatternStorageType::COR) {
        inline const RowType& GetRow(unsigned channel, unsigned order, unsigned row) const { return GetPattern(channel, order)[row]; }
        inline void SetRow(unsigned channel, unsigned order, unsigned row, const RowType& rowValue) { GetPattern(channel, order)[row] = rowValue; }
        inline const RowType& GetRowById(unsigned channel, unsigned patternId, unsigned row) const { return GetPatternById(channel, patternId)[row]; }
        inline void SetRowById(unsigned channel, unsigned patternId, unsigned row, const RowType& rowValue) { GetPatternById(channel, patternId)[row] = rowValue; }
    }

    // PatternMetadata
    if constexpr (m_StorageType == PatternStorageType::ORC) {
        inline const PatternMetadataType& GetPatternMetadata(unsigned patternId) const { return m_PatternMetadata[patternId]; }
        inline void SetPatternMetadata(unsigned patternId, const PatternMetadataType& patternMetadata) { m_PatternMetadata[patternId] = patternMetadata; }
    } else if constexpr (m_StorageType == PatternStorageType::COR) {
        inline const PatternMetadataType& GetPatternMetadata(unsigned channel, unsigned patternId) const { return m_PatternMetadata[channel][patternId]; }
        inline void SetPatternMetadata(unsigned channel, unsigned patternId, const PatternMetadataType& patternMetadata) { m_PatternMetadata[channel][patternId] = patternMetadata; }
    }

    /////// Other

    void CleanUp()
    {
        m_NumChannels = 0;
        m_NumOrders = 0;
        m_NumRows = 0;

        m_ChannelMetadata.clear();
        m_GlobalData.reset();
    }

private:

    unsigned        m_NumChannels;
    unsigned        m_NumOrders;    // Total orders (pattern matrix rows)
    unsigned        m_NumRows;      // Rows per pattern

    // Metadata (optional module-specific info)
    std::vector<ChannelMetadataType> m_ChannelMetadata; // [channel]

    // Wrapping this in a smart pointer avoids the "explicit specialization must precede its first use" error:
    std::unique_ptr<GlobalDataType> m_GlobalData;   // Global information about a particular module file
};

} // namespace d2m
