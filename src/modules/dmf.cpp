/*
    dmf.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines all classes used for Deflemask's DMF files.

    DMF file support was written according to the specs at
    http://www.deflemask.com/DMF_SPECS.txt.
*/

#include "dmf.h"
#include "utils/utils.h"
#include "utils/hash.h"

// For inflating DMF files so that they can be read
#include <zlib.h>
#include <zconf.h>

// Compile time math
#include <gcem.hpp>

#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <map>
#include <cmath>
#include <array>
#include <unordered_set>

using namespace d2m;
using namespace d2m::dmf;
// DO NOT use any module namespace other than d2m::dmf

static constexpr uint8_t DMF_FILE_VERSION_MIN = 17; // DMF files as old as version 17 (0x11) are supported
static constexpr uint8_t DMF_FILE_VERSION_MAX = 26; // DMF files as new as version 26 (0x1a) are supported

// Information about all the systems Deflemask supports
static const std::map<DMF::SystemType, System> DMFSystems =
{
    {DMF::SystemType::Error, System(DMF::SystemType::Error, 0x00, "ERROR", 0)},
    {DMF::SystemType::YMU759, System(DMF::SystemType::YMU759, 0x01, "YMU759", 17)}, // Removed since DMF version 19 (0x13)
    {DMF::SystemType::Genesis, System(DMF::SystemType::Genesis, 0x02, "Genesis", 10)},
    {DMF::SystemType::Genesis_CH3, System(DMF::SystemType::Genesis_CH3, 0x42, "Genesis (Ext. CH3)", 13)},
    {DMF::SystemType::SMS, System(DMF::SystemType::SMS, 0x03, "SMS", 4)},
    {DMF::SystemType::SMS_OPLL, System(DMF::SystemType::SMS_OPLL, 0x43, "SMS + OPLL", 13)},
    {DMF::SystemType::GameBoy, System(DMF::SystemType::GameBoy, 0x04, "Game Boy", 4)},
    {DMF::SystemType::PCEngine, System(DMF::SystemType::PCEngine, 0x05, "PC Engine", 6)},
    {DMF::SystemType::NES, System(DMF::SystemType::NES, 0x06, "NES", 5)},
    {DMF::SystemType::NES_VRC7, System(DMF::SystemType::NES_VRC7, 0x46, "NES + VRC7", 11)},
    {DMF::SystemType::C64_SID_8580, System(DMF::SystemType::C64_SID_8580, 0x07, "C64 (SID 8580)", 3)},
    {DMF::SystemType::C64_SID_6581, System(DMF::SystemType::C64_SID_6581, 0x47, "C64 (SID 6581)", 3)},
    {DMF::SystemType::Arcade, System(DMF::SystemType::Arcade, 0x08, "Arcade", 13)},
    {DMF::SystemType::NeoGeo, System(DMF::SystemType::NeoGeo, 0x09, "Neo Geo", 13)},
    {DMF::SystemType::NeoGeo_CH2, System(DMF::SystemType::NeoGeo_CH2, 0x49, "Neo Geo (Ext. CH2)", 16)},
    {DMF::SystemType::NES_FDS, System(DMF::SystemType::NES_FDS, 0x86, "NES + FDS", 6)}
};

const System& DMF::Systems(DMF::SystemType systemType) { return DMFSystems.at(systemType); }

static constexpr auto G_PeriodTable = []() constexpr
{
    std::array<double, 12 * 9> ret{};
    for (int i = 0; i < 12 * 9; ++i)
    {
        ret[i] = 262144.0 / (27.5 * gcem::pow(2, (i + 3) / 12.0));
    }
    return ret;
}();

static constexpr double GetPeriod(Note note)
{
    assert(static_cast<uint16_t>(note.pitch) < 12 && note.octave < 9);
    return G_PeriodTable[static_cast<uint16_t>(note.pitch) + 12*note.octave];
}

DMF::DMF()
{
    // Initialize pointers to nullptr to prevent segfault when freeing memory if the import fails:
    m_Instruments = nullptr;
    m_WavetableSizes = nullptr;
    m_WavetableValues = nullptr;
    m_PCMSamples = nullptr;
}

DMF::~DMF()
{
    CleanUp();
}

void DMF::CleanUp()
{
    // TODO: This function is old and nearly unchanged from when dmf2mod was a C program.
    //       Need to use RAII instead.

    // Free memory allocated for members
    if (m_Instruments)
    {
        for (int i = 0; i < m_TotalInstruments; i++) 
        {
            if (m_Instruments[i].mode == Instrument::StandardMode)
            {
                delete[] m_Instruments[i].std.arpEnvValue;
                delete[] m_Instruments[i].std.dutyNoiseEnvValue;
                delete[] m_Instruments[i].std.volEnvValue;
                delete[] m_Instruments[i].std.wavetableEnvValue;
            }
        }
        delete[] m_Instruments;
        m_Instruments = nullptr;
    }

    delete[] m_WavetableSizes;
    m_WavetableSizes = nullptr;

    if (m_WavetableValues)
    {
        for (int i = 0; i < m_TotalWavetables; i++)
        {
            delete[] m_WavetableValues[i];
            m_WavetableValues[i] = nullptr;
        }
        delete[] m_WavetableValues;
        m_WavetableValues = nullptr;
    }

    if (m_PCMSamples)
    {
        for (int sample = 0; sample < m_TotalPCMSamples; sample++) 
        {
            delete[] m_PCMSamples[sample].data;
        }
        delete[] m_PCMSamples;
        m_PCMSamples = nullptr;
    }

    GetData().CleanUp();
}

void DMF::ImportImpl(const std::string& filename)
{
    CleanUp();

    const bool verbose = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Verbose).GetValue<bool>();

    if (verbose)
        std::cout << "Starting to import the DMF file...\n";

    if (Utils::GetTypeFromFilename(filename) != ModuleType::DMF)
    {
        throw ModuleException(ModuleException::Category::Import, DMF::ImportError::UnspecifiedError, "Input file has the wrong file extension.\nPlease use a DMF file.");
    }

    if (verbose)
        std::cout << "DMF Filename: " << filename << "\n";

    Reader fin{filename, std::ios_base::binary};
    if (fin.stream().fail())
    {
        throw ModuleException(ModuleException::Category::Import, DMF::ImportError::UnspecifiedError, "Failed to open DMF file.");
    }

    ///////////////// FORMAT FLAGS

    // Check header
    if (fin.ReadStr(16) != ".DelekDefleMask.")
    {
        throw ModuleException(ModuleException::Category::Import, DMF::ImportError::UnspecifiedError, "DMF format header is bad.");
    }

    m_DMFFileVersion = fin.ReadInt();
    if (m_DMFFileVersion < DMF_FILE_VERSION_MIN || m_DMFFileVersion > DMF_FILE_VERSION_MAX)
    {
        const bool tooHigh = m_DMFFileVersion > DMF_FILE_VERSION_MAX;
        const int extremeVersion = tooHigh ? DMF_FILE_VERSION_MAX : DMF_FILE_VERSION_MIN;

        std::stringstream stream;
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << extremeVersion;
        std::string hex = stream.str();

        std::string errorMsg = "Deflemask file version must be " + std::to_string(extremeVersion) + " (" + hex + ") or ";
        errorMsg += tooHigh ? "lower.\n" : "higher.\n";
        
        stream.clear();
        stream.str("");
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << (int)m_DMFFileVersion;
        hex = stream.str();

        errorMsg += "The given DMF file is version " + std::to_string(m_DMFFileVersion) + " (" + hex + ").\n";
        if (tooHigh)
            errorMsg += "       Dmf2mod needs to be updated to support this newer version.";
        else
            errorMsg += "       You can convert older DMF files to a supported version by opening them in a newer version of DefleMask and then saving them.";

        throw ModuleException(ModuleException::Category::Import, DMF::ImportError::UnspecifiedError, errorMsg);
    }
    else if (verbose)
    {
        std::stringstream stream;
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << (int)m_DMFFileVersion;
        std::string hex = stream.str();

        std::cout << "DMF version " + std::to_string(m_DMFFileVersion) + " (" + hex + ")\n";
    }

    ///////////////// SYSTEM SET

    m_System = GetSystem(fin.ReadInt());

    if (verbose)
        std::cout << "System: " << m_System.name << " (channels: " << std::to_string(m_System.channels) << ")\n";

    ///////////////// VISUAL INFORMATION
    LoadVisualInfo(fin);
    if (verbose)
    {
        std::cout << "Title: " << GetTitle() << "\n";
        std::cout << "Author: " << GetAuthor() << "\n";
        std::cout << "Loaded visual information." << "\n";
    }

    ///////////////// MODULE INFORMATION
    LoadModuleInfo(fin);
    if (verbose)
        std::cout << "Loaded module information.\n";

    ///////////////// PATTERN MATRIX VALUES
    LoadPatternMatrixValues(fin);
    if (verbose)
        std::cout << "Loaded pattern matrix values.\n";

    ///////////////// INSTRUMENTS DATA
    LoadInstrumentsData(fin);
    if (verbose)
        std::cout << "Loaded instruments.\n";

    ///////////////// WAVETABLES DATA
    LoadWavetablesData(fin);
    if (verbose)
        std::cout << "Loaded " << std::to_string(m_TotalWavetables) << " wavetable(s).\n";

    ///////////////// PATTERNS DATA
    LoadPatternsData(fin);
    if (verbose)
        std::cout << "Loaded patterns.\n";

    ///////////////// PCM SAMPLES DATA
    LoadPCMSamplesData(fin);
    if (verbose)
        std::cout << "Loaded PCM samples.\n";

    if (verbose)
        std::cout << "Done importing DMF file.\n\n";
}

void DMF::ExportImpl(const std::string& filename)
{
    // Not implemented
    throw NotImplementedException{};
}

void DMF::ConvertImpl(const ModulePtr& input)
{
    // Not implemented
    throw NotImplementedException{};
}

System DMF::GetSystem(uint8_t systemByte) const
{
    for (const auto& mapPair : DMFSystems)
    {
        if (mapPair.second.id == systemByte)
            return mapPair.second;
    }
    return DMFSystems.at(DMF::SystemType::Error); // Error: System byte invalid
}

void DMF::LoadVisualInfo(Reader& fin)
{
    GetGlobalData().title = fin.ReadPStr();
    GetGlobalData().author = fin.ReadPStr();
    m_VisualInfo.highlightAPatterns = fin.ReadInt();
    m_VisualInfo.highlightBPatterns = fin.ReadInt();
}

void DMF::LoadModuleInfo(Reader& fin)
{
    m_ModuleInfo.timeBase = fin.ReadInt() + 1;
    m_ModuleInfo.tickTime1 = fin.ReadInt();
    m_ModuleInfo.tickTime2 = fin.ReadInt();
    m_ModuleInfo.framesMode = fin.ReadInt();
    m_ModuleInfo.usingCustomHZ = fin.ReadInt();
    m_ModuleInfo.customHZValue1 = fin.ReadInt();
    m_ModuleInfo.customHZValue2 = fin.ReadInt();
    m_ModuleInfo.customHZValue3 = fin.ReadInt();

    if (m_DMFFileVersion >= 24) // DMF version 24 (0x18) and newer.
    {
        // Newer versions read 4 bytes here
        m_ModuleInfo.totalRowsPerPattern = fin.ReadInt<false, 4>();
    }
    else // DMF version 23 (0x17) and older. WARNING: I don't have the specs for version 23 (0x17), so this may be wrong.
    {
        // Earlier versions such as 22 (0x16) only read one byte here
        m_ModuleInfo.totalRowsPerPattern = fin.ReadInt();
    }

    m_ModuleInfo.totalRowsInPatternMatrix = fin.ReadInt();

    // Prior to Deflemask Version 0.11.1, arpeggio tick speed was stored here
    // I don't have the specs for DMF version 20 (0x14), but based on a real DMF file of that version, it is the first DMF version 
    //      to NOT contain the arpeggio tick speed byte.
    if (m_DMFFileVersion <= 19) // DMF version 19 (0x13) and older
    {
        fin.ReadInt(); // arpTickSpeed: Discard for now
    }
}

void DMF::LoadPatternMatrixValues(Reader& fin)
{
    auto& moduleData = GetData();
    moduleData.AllocatePatternMatrix(
        m_System.channels,
        m_ModuleInfo.totalRowsInPatternMatrix,
        m_ModuleInfo.totalRowsPerPattern);

    std::map<std::pair<channel_index_t, pattern_index_t>, std::string> channelPatternIdToPatternNameMap;

    for (channel_index_t channel = 0; channel < moduleData.GetNumChannels(); ++channel)
    {
        for (order_index_t order = 0; order < moduleData.GetNumOrders(); ++order)
        {
            const pattern_index_t patternId = fin.ReadInt();
            moduleData.SetPatternId(channel, order, patternId);

            // Version 1.1 introduces pattern names
            if (m_DMFFileVersion >= 25) // DMF version 25 (0x19) and newer
            {
                std::string patternName = fin.ReadPStr();
                if (patternName.size() > 0)
                {
                    channelPatternIdToPatternNameMap[{channel, patternId}] = std::move(patternName);
                }
            }
        }
    }

    moduleData.AllocateChannels();
    moduleData.AllocatePatterns();

    // Pattern metadata must be set AFTER AllocatePatterns is called
    for (auto& [channelPatternId, patternName] : channelPatternIdToPatternNameMap)
    {
        moduleData.SetPatternMetadata(channelPatternId.first, channelPatternId.second, {std::move(patternName)});
    }
}

void DMF::LoadInstrumentsData(Reader& fin)
{
    m_TotalInstruments = fin.ReadInt();
    m_Instruments = new Instrument[m_TotalInstruments];

    for (int i = 0; i < m_TotalInstruments; i++)
    {
        m_Instruments[i] = LoadInstrument(fin, m_System.type);
    }
}

Instrument DMF::LoadInstrument(Reader& fin, DMF::SystemType systemType)
{
    Instrument inst = {};

    inst.name = fin.ReadPStr();

    // Get instrument mode (Standard or FM)
    inst.mode = Instrument::InvalidMode;
    switch (fin.ReadInt())
    {
        case 0:
            inst.mode = Instrument::StandardMode; break;
        case 1:
            inst.mode = Instrument::FMMode; break;
        default:
            throw ModuleException(Status::Category::Import, DMF::ImportError::UnspecifiedError, "Invalid instrument mode");
    }

    // Now we can import the instrument depending on the mode (Standard/FM)

    if (inst.mode == Instrument::StandardMode)
    {
        if (m_DMFFileVersion <= 17) // DMF version 17 (0x11) or older
        {
            // Volume macro
            inst.std.volEnvSize = fin.ReadInt();
            inst.std.volEnvValue = new int32_t[inst.std.volEnvSize];

            for (int i = 0; i < inst.std.volEnvSize; i++)
            {
                // 4 bytes, little-endian
                inst.std.volEnvValue[i] = fin.ReadInt<true, 4>();
            }

            // Always get envelope loop position byte regardless of envelope size
            inst.std.volEnvLoopPos = fin.ReadInt();
        }
        else if (systemType != DMF::SystemType::GameBoy) // Not a Game Boy and DMF version 18 (0x12) or newer
        {
            // Volume macro
            inst.std.volEnvSize = fin.ReadInt();
            inst.std.volEnvValue = new int32_t[inst.std.volEnvSize];

            for (int i = 0; i < inst.std.volEnvSize; i++)
            {
                // 4 bytes, little-endian
                inst.std.volEnvValue[i] = fin.ReadInt<true, 4>();
            }

            if (inst.std.volEnvSize > 0)
                inst.std.volEnvLoopPos = fin.ReadInt();
        }

        // Arpeggio macro
        inst.std.arpEnvSize = fin.ReadInt();
        inst.std.arpEnvValue = new int32_t[inst.std.arpEnvSize];

        for (int i = 0; i < inst.std.arpEnvSize; i++)
        {
            // 4 bytes, little-endian
            inst.std.arpEnvValue[i] = fin.ReadInt<true, 4>();
        }

        if (inst.std.arpEnvSize > 0 || m_DMFFileVersion <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.std.arpEnvLoopPos = fin.ReadInt();
        
        inst.std.arpMacroMode = fin.ReadInt();

        // Duty/Noise macro
        inst.std.dutyNoiseEnvSize = fin.ReadInt();
        inst.std.dutyNoiseEnvValue = new int32_t[inst.std.dutyNoiseEnvSize];

        for (int i = 0; i < inst.std.dutyNoiseEnvSize; i++)
        {
            // 4 bytes, little-endian
            inst.std.dutyNoiseEnvValue[i] = fin.ReadInt<true, 4>();
        }

        if (inst.std.dutyNoiseEnvSize > 0 || m_DMFFileVersion <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.std.dutyNoiseEnvLoopPos = fin.ReadInt();

        // Wavetable macro
        inst.std.wavetableEnvSize = fin.ReadInt();
        inst.std.wavetableEnvValue = new int32_t[inst.std.wavetableEnvSize];

        for (int i = 0; i < inst.std.wavetableEnvSize; i++)
        {
            // 4 bytes, little-endian
            inst.std.wavetableEnvValue[i] = fin.ReadInt<true, 4>();
        }

        if (inst.std.wavetableEnvSize > 0 || m_DMFFileVersion <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.std.wavetableEnvLoopPos = fin.ReadInt();

        // Per system data
        if (systemType == DMF::SystemType::C64_SID_8580 || systemType == DMF::SystemType::C64_SID_6581) // Using Commodore 64
        {
            inst.std.c64TriWaveEn = fin.ReadInt();
            inst.std.c64SawWaveEn = fin.ReadInt();
            inst.std.c64PulseWaveEn = fin.ReadInt();
            inst.std.c64NoiseWaveEn = fin.ReadInt();
            inst.std.c64Attack = fin.ReadInt();
            inst.std.c64Decay = fin.ReadInt();
            inst.std.c64Sustain = fin.ReadInt();
            inst.std.c64Release = fin.ReadInt();
            inst.std.c64PulseWidth = fin.ReadInt();
            inst.std.c64RingModEn = fin.ReadInt();
            inst.std.c64SyncModEn = fin.ReadInt();
            inst.std.c64ToFilter = fin.ReadInt();
            inst.std.c64VolMacroToFilterCutoffEn = fin.ReadInt();
            inst.std.c64UseFilterValuesFromInst = fin.ReadInt();

            // Filter globals
            inst.std.c64FilterResonance = fin.ReadInt();
            inst.std.c64FilterCutoff = fin.ReadInt();
            inst.std.c64FilterHighPass = fin.ReadInt();
            inst.std.c64FilterLowPass = fin.ReadInt();
            inst.std.c64FilterCH2Off = fin.ReadInt();
        }
        else if (systemType == DMF::SystemType::GameBoy && m_DMFFileVersion >= 18) // Using Game Boy and DMF version is 18 or newer
        {
            inst.std.gbEnvVol = fin.ReadInt();
            inst.std.gbEnvDir = fin.ReadInt();
            inst.std.gbEnvLen = fin.ReadInt();
            inst.std.gbSoundLen = fin.ReadInt();
        }
    }
    else if (inst.mode == Instrument::FMMode)
    {
        // Initialize to nullptr just in case
        inst.std.volEnvValue = nullptr;
        inst.std.arpEnvValue = nullptr;
        inst.std.dutyNoiseEnvValue = nullptr;
        inst.std.wavetableEnvValue = nullptr;

        if (m_DMFFileVersion > 18) // Newer than DMF version 18 (0x12)
        {
            if (systemType == DMF::SystemType::SMS_OPLL || systemType == DMF::SystemType::NES_VRC7)
            {
                inst.fm.sus = fin.ReadInt();
                inst.fm.fb = fin.ReadInt();
                inst.fm.dc = fin.ReadInt();
                inst.fm.dm = fin.ReadInt();
            }
            else
            {
                inst.fm.alg = fin.ReadInt();
                inst.fm.fb = fin.ReadInt();
                inst.fm.lfo = fin.ReadInt();
                inst.fm.lfo2 = fin.ReadInt();
            }

            inst.fm.numOperators = 4;
        }
        else
        {
            inst.fm.alg = fin.ReadInt();
            fin.ReadInt(); // Reserved byte (must be 0)
            inst.fm.fb = fin.ReadInt();
            fin.ReadInt(); // Reserved byte (must be 0)
            inst.fm.lfo = fin.ReadInt();
            fin.ReadInt(); // Reserved byte (must be 0)

            const bool totalOperatorsBool = fin.ReadInt();
            inst.fm.numOperators = totalOperatorsBool ? 4 : 2;

            inst.fm.lfo2 = fin.ReadInt();
        }

        for (int i = 0; i < inst.fm.numOperators; i++)
        {
            if (m_DMFFileVersion > 18) // Newer than DMF version 18 (0x12)
            {
                inst.fm.ops[i].am = fin.ReadInt();
                inst.fm.ops[i].ar = fin.ReadInt();
                inst.fm.ops[i].dr = fin.ReadInt();
                inst.fm.ops[i].mult = fin.ReadInt();
                inst.fm.ops[i].rr = fin.ReadInt();
                inst.fm.ops[i].sl = fin.ReadInt();
                inst.fm.ops[i].tl = fin.ReadInt();

                if (systemType == DMF::SystemType::SMS_OPLL || systemType == DMF::SystemType::NES_VRC7)
                {
                    const uint8_t opllPreset = fin.ReadInt();
                    if (i == 0)
                        inst.fm.opllPreset = opllPreset;

                    inst.fm.ops[i].ksr = fin.ReadInt();
                    inst.fm.ops[i].vib = fin.ReadInt();
                    inst.fm.ops[i].ksl = fin.ReadInt();
                    inst.fm.ops[i].egs = fin.ReadInt(); // EG-S in Deflemask. 0 if OFF; 8 if ON.
                }
                else
                {
                    inst.fm.ops[i].dt2 = fin.ReadInt();
                    inst.fm.ops[i].rs = fin.ReadInt();
                    inst.fm.ops[i].dt = fin.ReadInt();
                    inst.fm.ops[i].d2r = fin.ReadInt();
                    inst.fm.ops[i].SSGMode = fin.ReadInt();
                }
            }
            else // DMF version 17 (0x11) or older
            {
                inst.fm.ops[i].am = fin.ReadInt();
                inst.fm.ops[i].ar = fin.ReadInt();
                inst.fm.ops[i].dam = fin.ReadInt();
                inst.fm.ops[i].dr = fin.ReadInt();
                inst.fm.ops[i].dvb = fin.ReadInt();
                inst.fm.ops[i].egt = fin.ReadInt();
                inst.fm.ops[i].ksl = fin.ReadInt();
                inst.fm.ops[i].mult = fin.ReadInt();
                inst.fm.ops[i].rr = fin.ReadInt();
                inst.fm.ops[i].sl = fin.ReadInt();
                inst.fm.ops[i].sus = fin.ReadInt();
                inst.fm.ops[i].tl = fin.ReadInt();
                inst.fm.ops[i].vib = fin.ReadInt();
                inst.fm.ops[i].ws = fin.ReadInt();
                inst.fm.ops[i].ksr = fin.ReadInt(); // RS on SEGA Genesis
                inst.fm.ops[i].dt = fin.ReadInt();
                inst.fm.ops[i].d2r = fin.ReadInt();
                inst.fm.ops[i].SSGMode = fin.ReadInt();
            }
        }
    }

    return inst;
}

void DMF::LoadWavetablesData(Reader& fin)
{
    m_TotalWavetables = fin.ReadInt();

    m_WavetableSizes = new uint32_t[m_TotalWavetables];
    m_WavetableValues = new uint32_t*[m_TotalWavetables];

    uint32_t dataMask = 0xFFFFFFFF;
    if (GetSystem().type == DMF::SystemType::GameBoy)
    {
        dataMask = 0xF;
    }
    else if (GetSystem().type == DMF::SystemType::NES_FDS)
    {
        dataMask = 0x3F;
    }

    for (int i = 0; i < m_TotalWavetables; i++)
    {
        m_WavetableSizes[i] = fin.ReadInt<false, 4>();

        m_WavetableValues[i] = new uint32_t[m_WavetableSizes[i]];

        for (unsigned j = 0; j < m_WavetableSizes[i]; j++)
        {
            m_WavetableValues[i][j] = fin.ReadInt<false, 4>() & dataMask;

            // Bug fix for DMF version 25 (0x19): Transform 4-bit FDS wavetables into 6-bit
            if (GetSystem().type == DMF::SystemType::NES_FDS && m_DMFFileVersion <= 25)
            {
                m_WavetableValues[i][j] <<= 2; // x4
            }
        }
    }
}

void DMF::LoadPatternsData(Reader& fin)
{
    auto& moduleData = GetData();
    auto& channelMetadata = moduleData.ChannelMetadataRef();

    std::unordered_set<std::pair<channel_index_t, pattern_index_t>, PairHash> patternsVisited; // Storing channel/patternId pairs

    for (channel_index_t channel = 0; channel < moduleData.GetNumChannels(); ++channel)
    {
        channelMetadata[channel].effectColumnsCount = fin.ReadInt();

        for (order_index_t order = 0; order < moduleData.GetNumOrders(); ++order)
        {
            const pattern_index_t patternId = moduleData.GetPatternId(channel, order);

            if (patternsVisited.count({channel, patternId}) > 0) // If pattern has been loaded previously
            {
                // Skip patterns that have already been loaded (unnecessary information)
                // zstr's seekg method does not seem to work, so I will use this:
                unsigned seekAmount = (8 + 4 * channelMetadata[channel].effectColumnsCount) * moduleData.GetNumRows();
                while (0 < seekAmount--)
                {
                    fin.ReadInt();
                }
                continue;
            }
            else
            {
                // Mark this patternId for this channel as visited
                patternsVisited.insert({channel, patternId});
            }

            for (row_index_t row = 0; row < moduleData.GetNumRows(); ++row)
            {
                moduleData.SetRowById(channel, patternId, row, LoadPatternRow(fin, channelMetadata[channel].effectColumnsCount));
            }
        }
    }
}

Row<DMF> DMF::LoadPatternRow(Reader& fin, uint8_t effectsColumnsCount)
{
    Row<DMF> row;

    const uint16_t tempPitch = fin.ReadInt<false, 2>();
    uint8_t tempOctave = fin.ReadInt<false, 2>(); // Upper byte is unused to this conversion is okay

    switch (tempPitch)
    {
        case 0:
            if (tempOctave == 0)
                row.note = NoteTypes::Empty{};
            else
                row.note = NoteTypes::Note{static_cast<NotePitch>(tempPitch), tempOctave};
            break;
        case 100:
            row.note = NoteTypes::Off{};
            break;
        default:
            // Apparently, the note pitch for C- can be either 0 or 12. I'm setting it to 0 always.
            if (tempPitch == 12)
                row.note = NoteTypes::Note{NotePitch::C, ++tempOctave};
            else
                row.note = NoteTypes::Note{static_cast<NotePitch>(tempPitch), tempOctave};
            break;
    }

    row.volume = fin.ReadInt<true, 2>();

    for (uint8_t col = 0; col < effectsColumnsCount; ++col)
    {
        row.effect[col].code = fin.ReadInt<true, 2>();
        row.effect[col].value = fin.ReadInt<true, 2>();
    }

    // Initialize the rest to zero
    for (uint8_t col = effectsColumnsCount; col < 4; ++col) // Max total of 4 effects columns in Deflemask
    {
        row.effect[col] = {(int16_t)EffectCode::NoEffect, (int16_t)EffectCode::NoEffectVal};
    }

    row.instrument = fin.ReadInt<true, 2>();

    return row;
}

void DMF::LoadPCMSamplesData(Reader& fin)
{
    m_TotalPCMSamples = fin.ReadInt();
    m_PCMSamples = new PCMSample[m_TotalPCMSamples];

    for (unsigned sample = 0; sample < m_TotalPCMSamples; sample++)
    {
        m_PCMSamples[sample] = LoadPCMSample(fin);
    }
}

PCMSample DMF::LoadPCMSample(Reader& fin)
{
    PCMSample sample;

    sample.size = fin.ReadInt<false, 4>();

    if (m_DMFFileVersion >= 24) // DMF version 24 (0x18)
    {
        // Read PCM sample name
        sample.name = fin.ReadPStr();
    }
    else // DMF version 23 (0x17) and older. WARNING: I don't have the specs for version 23 (0x17), so this may be wrong.
    {
        // PCM samples don't have names in this DMF version
        sample.name = "";
    }

    sample.rate = fin.ReadInt();
    sample.pitch = fin.ReadInt();
    sample.amp = fin.ReadInt();

    if (m_DMFFileVersion >= 22) // DMF version 22 (0x16) and newer
    {
        sample.bits = fin.ReadInt();
    }

    sample.data = new uint16_t[sample.size];
    for (uint32_t i = 0; i < sample.size; i++)
    {
        sample.data[i] = fin.ReadInt<false, 2>();
    }

    return sample;
}

void DMF::GetBPM(unsigned& numerator, unsigned& denominator) const
{
    // Gets the initial BPM of the module
    unsigned int globalTick;
    if (m_ModuleInfo.usingCustomHZ)
    {
        if (m_ModuleInfo.customHZValue1 == 0) // No digits filled in
        {
            // NTSC is used by default if custom global tick box is selected but the value is left blank:
            globalTick = 60;
        }
        else if (m_ModuleInfo.customHZValue2 == 0) // One digit filled in
        {
            globalTick = (m_ModuleInfo.customHZValue1 - 48);
        }
        else if (m_ModuleInfo.customHZValue3 == 0) // Two digits filled in
        {
            globalTick = (m_ModuleInfo.customHZValue1 - 48) * 10 + (m_ModuleInfo.customHZValue2 - 48);
        }
        else // All three digits filled in
        {
            globalTick = (m_ModuleInfo.customHZValue1 - 48) * 100 + (m_ModuleInfo.customHZValue2 - 48) * 10 + (m_ModuleInfo.customHZValue3 - 48);
        }
    }
    else
    {
        globalTick = m_ModuleInfo.framesMode ? 60 : 50; // NTSC (60 Hz) or PAL (50 Hz)
    }

    // Experimentally determined equation for BPM:
    numerator = 15 * globalTick;
    denominator = GetTicksPerRowPair();

    if (denominator == 0)
        throw std::runtime_error("Tried to divide by zero when calculating BPM.\n");
}

double DMF::GetBPM() const
{
    // Returns the initial BPM of the module
    unsigned numerator, denominator;
    GetBPM(numerator, denominator);
    return numerator * 1.0 / denominator;
}

int DMF::GetRowsUntilPortUpAutoOff(const NoteSlot& note, int portUpParam) const
{
    const unsigned ticksPerRowPair = GetTicksPerRowPair();
    return GetRowsUntilPortUpAutoOff(ticksPerRowPair, note, portUpParam);
}

int DMF::GetRowsUntilPortUpAutoOff(unsigned ticksPerRowPair, const NoteSlot& note, int portUpParam)
{
    // Note: This is not always 100% accurate, probably due to differences in rounding/truncating at intermediate steps of the calculation, but it's very close.
    // TODO: Need to take into account odd/even rows rather than using the average ticks per row
    if (!NoteHasPitch(note))
        return 0;

    constexpr double highestPeriod = GetPeriod({NotePitch::C, 8}); // C-8

    // Not sure why the 0.75 is needed
    return static_cast<int>(std::max(std::ceil(0.75 * (highestPeriod - GetPeriod(GetNote(note))) / ((ticksPerRowPair / 2.0) * portUpParam * -1.0)), 1.0));
}

int DMF::GetRowsUntilPortDownAutoOff(const NoteSlot& note, int portDownParam) const
{
    const unsigned ticksPerRowPair = GetTicksPerRowPair();
    return GetRowsUntilPortDownAutoOff(ticksPerRowPair, note, portDownParam);
}

int DMF::GetRowsUntilPortDownAutoOff(unsigned ticksPerRowPair, const NoteSlot& note, int portDownParam)
{
    // Note: This is not always 100% accurate, probably due to differences in rounding/truncating at intermediate steps of the calculation, but it's very close.
    // TODO: Need to take into account odd/even rows rather than using the average ticks per row
    if (!NoteHasPitch(note))
        return 0;

    constexpr double lowestPeriod = GetPeriod({NotePitch::C, 2}); // C-2
    return static_cast<int>(std::max(std::ceil((lowestPeriod - GetPeriod(GetNote(note))) / ((ticksPerRowPair / 2.0) * portDownParam)), 1.0));
}

size_t DMF::GenerateDataImpl(size_t dataFlags) const
{
    auto& genData = *GetGeneratedData();

    // Currently can only generate data for the Game Boy system
    if (GetSystem().type != System::Type::GameBoy)
        return 0;

    // Initialize state
    auto& stateData = genData.GetState().emplace();
    stateData.Initialize(GetSystem().channels);
    auto globalState = stateData.GetGlobalReaderWriter();

    // Initialize other generated data
    using DataEnum = ModuleGeneratedDataMethods<DMF>::DataEnum;
    auto& soundIndexesUsed = genData.Get<DataEnum::kSoundIndexesUsed>().emplace();
    auto& soundIndexNoteExtremes = genData.Get<DataEnum::kSoundIndexNoteExtremes>().emplace();

    // For convenience:
    //using GlobalEnumCommon = GlobalState<DMF>::StateEnumCommon;
    //using GlobalEnum = GlobalState<DMF>::StateEnum;
    using ChannelEnumCommon = ChannelState<DMF>::StateEnumCommon;
    //using ChannelEnum = ChannelState<DMF>::StateEnum;

    // Set up for suspending/restoring states:
    std::optional<ChannelState<DMF>::data_t> channelStateCopy = std::nullopt;
    int jumpDestination = -1;
    bool stateSuspended = false;

    // Main loop
    const auto& data = GetData();
    for (channel_index_t channel = 0; channel < data.GetNumChannels(); ++channel)
    {
        auto& channelState = *stateData.GetChannelReaderWriter(channel);
        for (order_index_t order = 0; order < data.GetNumOrders(); ++order)
        {
            for (row_index_t row = 0; row < data.GetNumRows(); ++row)
            {
                const auto& rowData = data.GetRow(channel, order, row);

                // If just arrived at jump destination:
                if (static_cast<int>(order) == jumpDestination && row == 0 && stateSuspended)
                {
                    // Restore state copies
                    channelState.Insert(channelStateCopy.value());
                }

                /*
                PriorityEffectsMap modEffects;
                //mod_sample_id_t modSampleId = 0;
                //uint16_t period = 0;

                if (channel == static_cast<channel_index_t>(dmf::GameBoyChannel::NOISE))
                {
                    modEffects = DMFConvertEffects_NoiseChannel(chanRow);
                    DMFUpdateStatePre(dmf, state, modEffects);
                    continue;
                }

                modEffects = DMFConvertEffects(chanRow, state);
                DMFUpdateStatePre(dmf, state, modEffects);
                DMFGetAdditionalEffects(dmf, state, chanRow, modEffects);

                //DMFConvertNote(state, chanRow, sampleMap, modEffects, modSampleId, period);

                // TODO: More state-related stuff could be extracted from DMFConvertNote and put into separate 
                //  method so that I don't have to copy code from it to put here.

                // Convert note - Note cut effect
                auto sampleChangeEffects = modEffects.equal_range(EffectPrioritySampleChange);
                if (sampleChangeEffects.first != sampleChangeEffects.second) // If sample change occurred (duty cycle, wave, or note cut effect)
                {
                    for (auto& iter = sampleChangeEffects.first; iter != sampleChangeEffects.second; )
                    {
                        Effect& modEffect = iter->second;
                        if (modEffect.effect == EffectCode::CutSample && modEffect.value == 0) // Note cut
                        {
                            // Silent sample is needed
                            if (sampleMap.count(-1) == 0)
                                sampleMap[-1] = {};

                            chanState.notePlaying = false;
                            chanState.currentNote = {};
                            iter = modEffects.erase(iter); // Consume the effect
                        }
                        else
                        {
                            // Only increment if an element wasn't erased
                            ++iter;
                        }
                    }
                }
                */

                // Convert note - Note OFF
                if (NoteIsOff(rowData.note)) // Note OFF. Use silent sample and handle effects.
                {
                    channelState.Set<(int)ChannelEnumCommon::kNoteSlot>(rowData.note);
                    //chanState.notePlaying = false;
                    //chanState.currentNote = NoteTypes::Off{};
                }

                // A note on the SQ1, SQ2, or WAVE channels:
                if (NoteHasPitch(rowData.note) && channel != dmf::GameBoyChannel::NOISE)
                {
                    const Note& dmfNote = GetNote(rowData.note);
                    //chanState.notePlaying = true;
                    //chanState.currentNote = chanRow.note;
                    channelState.Set<(int)ChannelEnumCommon::kNoteSlot>(rowData.note);

                    // TODO:
                    sound_index_t soundIndex = 0;///channel == dmf::GameBoyChannel::WAVE ? chanState.wavetable + 4 : chanState.dutyCycle;

                    // Mark this square wave or wavetable as used
                    soundIndexesUsed.insert(soundIndex);

                    // Get lowest/highest notes
                    if (soundIndexNoteExtremes.count(soundIndex) == 0) // 1st time
                    {
                        soundIndexNoteExtremes[soundIndex] = { dmfNote, dmfNote };
                    }
                    else
                    {
                        auto& notePair = soundIndexNoteExtremes[soundIndex];
                        if (dmfNote > notePair.second)
                        {
                            // Found a new highest note
                            notePair.second = dmfNote;
                        }
                        if (dmfNote < notePair.first)
                        {
                            // Found a new lowest note
                            notePair.first = dmfNote;
                        }
                    }
                }
            }
        }
    }

    return 0;
}
