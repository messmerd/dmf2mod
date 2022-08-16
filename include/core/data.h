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
    Different modules have significantly different per-channel row contents, so
    providing one single generic implementation for use by every module doesn't
    make much sense. Each module should provide their own Row implementation.
*/

template <class ModuleClass>
struct Row
{
    NoteSlot note;
};

/*
    Some module formats contain additional data for each channel or row.
    Specializations for ChannelMetadata and PatternMetadata can be created
    for any module format which requires it.
*/

template <class ModuleClass>
struct ChannelMetadata {};

template <class ModuleClass>
struct PatternMetadata {};

/*
    ModuleData stores and provides access to song data such as
    orders, patterns, rows, and other information.
*/

template <class ModuleClass>
class ModuleData
{
public:
    using RowType = Row<ModuleClass>;
    using ChannelMetadataType = ChannelMetadata<ModuleClass>;
    using PatternMetadataType = PatternMetadata<ModuleClass>;
    using PatternType = RowType*; // A pattern is an array of rows

    ModuleData() { CleanUp(); }
    ~ModuleData() { CleanUp(); }

    // This is the 1st initialization method to call
    void InitializePatternMatrix(unsigned channels, unsigned orders, unsigned rows)
    {
        CleanUp();
        m_NumChannels = channels;
        m_NumOrders = orders;
        m_NumRows = rows;

        // m_PatterMatrixValues[channel][pattern matrix row]
        m_PatternIds.resize(channels);

        for (unsigned i = 0; i < channels; ++i)
        {
            m_PatternIds[i].resize(orders);
        }
    }

    // This is the 2nd initialization method to call
    void InitializePatterns()
    {
        // m_Patterns[channel][pattern id (not order!)][pattern row number]
        m_Patterns.resize(GetNumChannels());
        if constexpr (!std::is_empty_v<PatternMetadataType>)
        {
            // Only set it if it's going to be used
            m_PatternMetadata.resize(GetNumChannels());
        }

        for (unsigned channel = 0; channel < GetNumChannels(); ++channel)
        {
            const unsigned totalPatterns = m_NumPatterns[channel];
            m_Patterns[channel] = new PatternType[totalPatterns];

            for (unsigned patternNumber = 0; patternNumber < totalPatterns; ++patternNumber)
            {
                m_Patterns[channel][patternNumber] = new RowType[GetNumRows()]();
                if constexpr (!std::is_empty_v<PatternMetadataType>)
                {
                    // Only set it if it's going to be used
                    m_PatternMetadata[channel].resize(totalPatterns);
                }
            }
        }
    }

    // This is the 3rd and final initialization method to call
    void InitializeChannels()
    {
        // Call this after all the pattern IDs are set
        m_NumPatterns.resize(m_NumChannels);

        for (unsigned chan = 0; chan < m_NumChannels; ++chan)
        {
            m_NumPatterns[chan] = *std::max_element(m_PatternIds[chan].begin(), m_PatternIds[chan].end()) + 1;
        }

        if constexpr (!std::is_empty_v<ChannelMetadataType>)
        {
            // Only set it if it's going to be used
            m_ChannelMetadata.resize(m_NumChannels);
        }
    }

    /////// DIRECT ACCESS GETTERS ///////

    inline const std::vector<std::vector<uint8_t>>& PatternIdsRef() const { return m_PatternIds; }
    inline std::vector<std::vector<uint8_t>>& PatternIdsRef() { return m_PatternIds; }

    inline const std::vector<uint8_t>& NumPatternsRef() const { return m_NumPatterns; }
    inline std::vector<uint8_t>& NumPatternsRef() { return m_NumPatterns; }

    inline const std::vector<PatternType*>& PatternsRef() const { return m_Patterns; }
    inline std::vector<PatternType*>& PatternsRef() { return m_Patterns; }

    inline const std::vector<ChannelMetadataType>& ChannelMetadataRef() const { return m_ChannelMetadata; }
    inline std::vector<ChannelMetadataType>& ChannelMetadataRef() { return m_ChannelMetadata; }

    inline const std::vector<std::vector<PatternMetadataType>>& PatternMetadataRef() const { return m_PatternMetadata; }
    inline std::vector<std::vector<PatternMetadataType>>& PatternMetadataRef() { return m_PatternMetadata; }

    /////// GETTERS ///////

    inline unsigned GetNumChannels() const { return m_NumChannels; }
    inline unsigned GetNumOrders() const { return m_NumOrders; }
    inline unsigned GetNumRows() const { return m_NumRows; }

    inline uint8_t GetPatternId(unsigned channel, unsigned order) const { return m_PatternIds[channel][order]; }

    inline uint8_t GetNumPatterns(unsigned channel) const { return m_NumPatterns[channel]; }

    inline PatternType GetPattern(unsigned channel, unsigned order) const { return m_Patterns[channel][GetPatternId(channel, order)]; }

    inline PatternType GetPatternById(unsigned channel, unsigned patternId) const { return m_Patterns[channel][patternId]; }

    inline const RowType& GetRow(unsigned channel, unsigned order, unsigned row) const
    {
        return m_Patterns[channel][m_PatternIds[channel][order]][row];
    }

    inline const RowType& GetRowById(unsigned channel, unsigned patternId, unsigned row) const
    {
        return m_Patterns[channel][patternId][row];
    }

    inline const ChannelMetadataType& GetChannelMetadata(unsigned channel) const
    {
        return m_ChannelMetadata[channel];
    }

    inline const PatternMetadataType& GetPatternMetadata(unsigned channel, unsigned pattern) const
    {
        return m_PatternMetadata[channel][pattern];
    }

    /////// SETTERS ///////

    inline void SetPatternId(unsigned channel, unsigned order, uint8_t patternId) { m_PatternIds[channel][order] = patternId; }

    inline void SetNumPatterns(unsigned channel, uint8_t numPatterns) { m_NumPatterns[channel] = numPatterns; }

    // TODO: Deep copy?
    inline void SetPattern(unsigned channel, unsigned order, PatternType&& pattern) { m_Patterns[channel][GetPatternId(channel, order)] = std::move(pattern); }

    // TODO: Deep copy?
    inline void GetPatternById(unsigned channel, unsigned patternId, PatternType&& pattern) { m_Patterns[channel][patternId] = std::move(pattern); }

    inline void SetRow(unsigned channel, unsigned order, unsigned row, const RowType& rowValue)
    {
        m_Patterns[channel][m_PatternIds[channel][order]][row] = rowValue;
    }

    inline void SetRowById(unsigned channel, unsigned patternId, unsigned row, const RowType& rowValue)
    {
        m_Patterns[channel][patternId][row] = rowValue;
    }

    inline void SetChannelMetadata(unsigned channel, const ChannelMetadataType& channelMetadata)
    {
        m_ChannelMetadata[channel] = channelMetadata;
    }

    inline void SetPatternMetadata(unsigned channel, unsigned pattern, const PatternMetadataType& patternMetadata)
    {
        m_PatternMetadata[channel][pattern] = patternMetadata;
    }

    void CleanUp()
    {
        if (!m_NumPatterns.empty())
        {
            for (unsigned channel = 0; channel < m_NumChannels; ++channel)
            {
                for (unsigned patternId = 0; patternId < m_NumPatterns[channel]; ++patternId)
                {
                    delete[] m_Patterns[channel][patternId];
                    m_Patterns[channel][patternId] = nullptr;
                }
                delete[] m_Patterns[channel];
                m_Patterns[channel] = nullptr;
            }
            m_Patterns.clear();
        }

        m_NumChannels = 0;
        m_NumOrders = 0;
        m_NumRows = 0;

        m_PatternIds.clear();
        m_NumPatterns.clear();
        m_ChannelMetadata.clear();
        m_PatternMetadata.clear();
    }

private:

    unsigned        m_NumChannels;
    unsigned        m_NumOrders;    // Total orders (pattern matrix rows)
    unsigned        m_NumRows;      // Rows per pattern

    // Pattern matrix
    std::vector<std::vector<uint8_t>>   m_PatternIds;  // Stores patterns IDs for each order and channel in the pattern matrix
    std::vector<uint8_t>                m_NumPatterns; // Patterns per channel

    // Pattern info
    std::vector<PatternType*>   m_Patterns; // [channel][pattern id]

    // Metadata (optional module-specific info)
    std::vector<ChannelMetadataType> m_ChannelMetadata; // [channel]
    std::vector<std::vector<PatternMetadataType>> m_PatternMetadata; // [channel][pattern id]
};

} // namespace d2m
