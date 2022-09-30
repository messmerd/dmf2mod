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

    std::map<std::pair<ChannelIndex, PatternIndex>, std::string> channelPatternIdToPatternNameMap;

    for (ChannelIndex channel = 0; channel < moduleData.GetNumChannels(); ++channel)
    {
        for (OrderIndex order = 0; order < moduleData.GetNumOrders(); ++order)
        {
            const PatternIndex patternId = fin.ReadInt();
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

    std::unordered_set<std::pair<ChannelIndex, PatternIndex>, PairHash> patternsVisited; // Storing channel/patternId pairs

    for (ChannelIndex channel = 0; channel < moduleData.GetNumChannels(); ++channel)
    {
        channelMetadata[channel].effectColumnsCount = fin.ReadInt();

        for (OrderIndex order = 0; order < moduleData.GetNumOrders(); ++order)
        {
            const PatternIndex patternId = moduleData.GetPatternId(channel, order);

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

            for (RowIndex row = 0; row < moduleData.GetNumRows(); ++row)
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
    uint8_t tempOctave = fin.ReadInt<false, 2>(); // Upper byte is unused

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
            assert(tempPitch <= 12);
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
        const int16_t dmf_effect_code = fin.ReadInt<true, 2>();
        row.effect[col].value = fin.ReadInt<true, 2>();
        assert(kEffectValueless == -1); // DMF valueless effect magic number is -1, and so must be kEffectValueless

        // Convert DMF effect code to dmf2mod's internal representation
        EffectCode effect_code = Effects::kNoEffect;
        switch (dmf_effect_code)
        {
            case dmf::EffectCode::kNoEffect:             effect_code = Effects::kNoEffect; break;
            case dmf::EffectCode::kArp:                  effect_code = Effects::kArp; break;
            case dmf::EffectCode::kPortUp:               effect_code = Effects::kPortUp; break;
            case dmf::EffectCode::kPortDown:             effect_code = Effects::kPortDown; break;
            case dmf::EffectCode::kPort2Note:            effect_code = Effects::kPort2Note; break;
            case dmf::EffectCode::kVibrato:              effect_code = Effects::kVibrato; break;
            case dmf::EffectCode::kPort2NoteVolSlide:    effect_code = Effects::kPort2NoteVolSlide; break;
            case dmf::EffectCode::kVibratoVolSlide:      effect_code = Effects::kVibratoVolSlide; break;
            case dmf::EffectCode::kTremolo:              effect_code = Effects::kTremolo; break;
            case dmf::EffectCode::kPanning:              effect_code = Effects::kPanning; break;
            case dmf::EffectCode::kSetSpeedVal1:         effect_code = Effects::kSpeedA; break;
            case dmf::EffectCode::kVolSlide:             effect_code = Effects::kVolSlide; break;
            case dmf::EffectCode::kPosJump:              effect_code = Effects::kPosJump; break;
            case dmf::EffectCode::kRetrig:               effect_code = Effects::kRetrigger; break;
            case dmf::EffectCode::kPatBreak:             effect_code = Effects::kPatBreak; break;
            case dmf::EffectCode::kArpTickSpeed:         effect_code = dmf::Effects::kArpTickSpeed; break;
            case dmf::EffectCode::kNoteSlideUp:          effect_code = dmf::Effects::kNoteSlideUp; break;
            case dmf::EffectCode::kNoteSlideDown:        effect_code = dmf::Effects::kNoteSlideDown; break;
            case dmf::EffectCode::kSetVibratoMode:       effect_code = dmf::Effects::kSetVibratoMode; break;
            case dmf::EffectCode::kSetFineVibratoDepth:  effect_code = dmf::Effects::kSetFineVibratoDepth; break;
            case dmf::EffectCode::kSetFinetune:          effect_code = dmf::Effects::kSetFinetune; break;
            case dmf::EffectCode::kSetSamplesBank:       effect_code = dmf::Effects::kSetSamplesBank; break;
            case dmf::EffectCode::kNoteCut:              effect_code = Effects::kNoteCut; break;
            case dmf::EffectCode::kNoteDelay:            effect_code = Effects::kNoteDelay; break;
            case dmf::EffectCode::kSyncSignal:           effect_code = dmf::Effects::kSyncSignal; break;
            case dmf::EffectCode::kSetGlobalFinetune:    effect_code = dmf::Effects::kSetGlobalFinetune; break;
            case dmf::EffectCode::kSetSpeedVal2:         effect_code = Effects::kSpeedB; break;

            // Game Boy exclusive:
            case dmf::EffectCode::kGameBoySetWave:                   effect_code = dmf::Effects::kGameBoySetWave; break;
            case dmf::EffectCode::kGameBoySetNoisePolyCounterMode:   effect_code = dmf::Effects::kGameBoySetNoisePolyCounterMode; break;
            case dmf::EffectCode::kGameBoySetDutyCycle:              effect_code = dmf::Effects::kGameBoySetDutyCycle; break;
            case dmf::EffectCode::kGameBoySetSweepTimeShift:         effect_code = dmf::Effects::kGameBoySetSweepTimeShift; break;
            case dmf::EffectCode::kGameBoySetSweepDir:               effect_code = dmf::Effects::kGameBoySetSweepDir; break;

            default:
                // Set a warning here? (unable to parse effect code)
                break;
        }
        row.effect[col].code = effect_code;
    }

    // Initialize the rest to zero
    for (uint8_t col = effectsColumnsCount; col < 4; ++col) // Max total of 4 effects columns in Deflemask
    {
        row.effect[col] = {Effects::kNoEffect, 0};
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
    auto& gen_data = *GetGeneratedData();
    const auto& data = GetData();

    // Currently can only generate data for the Game Boy system
    if (GetSystem().type != System::Type::GameBoy)
        return 0;

    // Initialize state
    auto& state_data = gen_data.GetState().emplace();
    state_data.Initialize(GetSystem().channels);

    // Get reader/writers
    auto state_reader_writers = state_data.GetReaderWriters();
    auto& global_state = state_reader_writers->global_reader_writer;
    auto& channel_states = state_reader_writers->channel_reader_writers;

    // Initialize other generated data
    using GenDataEnumCommon = ModuleGeneratedData<DMF>::GenDataEnumCommon;
    auto& sound_indexes_used = gen_data.Get<GenDataEnumCommon::kSoundIndexesUsed>().emplace();
    auto& sound_index_note_extremes = gen_data.Get<GenDataEnumCommon::kSoundIndexNoteExtremes>().emplace();
    //auto& channel_note_extremes = gen_data.Get<GenDataEnumCommon::kChannelNoteExtremes>().emplace();
    auto& loopback_points = gen_data.Get<GenDataEnumCommon::kLoopbackPoints>().emplace();
    gen_data.Get<GenDataEnumCommon::kNoteOffUsed>() = false;

    // For convenience:
    using GlobalEnumCommon = GlobalState<DMF>::StateEnumCommon;
    //using GlobalEnum = GlobalState<DMF>::StateEnum;
    using ChannelEnumCommon = ChannelState<DMF>::StateEnumCommon;
    //using ChannelEnum = ChannelState<DMF>::StateEnum;

    // For portamento auto-off: -1 = port not on; 0 = need to turn port off; >0 = rows until port auto off
    std::vector<int> rows_until_port_auto_off(data.GetNumChannels(), -1);

    /*
     * In spite of what the Deflemask manual says, portamento effects are automatically turned off if they
     *  stay on long enough without a new note being played. I believe it's until C-2 is reached for port down
     *  and C-8 for port up. Port2Note also seems to have an "auto-off" point, but I haven't looked into it.
     * See order 0x14 in the "i wanna eat my ice cream alone (reprise)" demo song for an example of "auto-off" behavior.
     * In that order, for the F-2 note on SQ2, the port down effect turns off automatically if the next note
     *   comes 21 or more rows later. The number of rows it takes depends on the note pitch, port effect parameter,
     * and the tempo denominator (Speed A/B and the base time).
     * The lambda functions below return the number of rows until auto-off given this information.
     * While they are based on the formulas apparently used by Deflemask, they are still not 100% accurate.
     */
    auto RowsUntilPortUpAutoOff = [](unsigned ticksPerRowPair, const NoteSlot& note, int portUpParam)
    {
        // Note: This is not always 100% accurate, probably due to differences in rounding/truncating at intermediate steps of the calculation, but it's very close.
        // TODO: Need to take into account odd/even rows rather than using the average ticks per row
        if (!NoteHasPitch(note)) return 0;
        assert((int)GetNote(note).pitch >= 0);
        constexpr double highestPeriod = GetPeriod({NotePitch::C, 8}); // C-8
        // Not sure why the 0.75 is needed
        return static_cast<int>(std::max(std::ceil(0.75 * (highestPeriod - GetPeriod(GetNote(note))) / ((ticksPerRowPair / 2.0) * portUpParam * -1.0)), 1.0));
    };

    auto RowsUntilPortDownAutoOff = [](unsigned ticksPerRowPair, const NoteSlot& note, int portDownParam)
    {
        // Note: This is not always 100% accurate, probably due to differences in rounding/truncating at intermediate steps of the calculation, but it's very close.
        // TODO: Need to take into account odd/even rows rather than using the average ticks per row
        if (!NoteHasPitch(note)) return 0;
        assert((int)GetNote(note).pitch >= 0);
        constexpr double lowestPeriod = GetPeriod({NotePitch::C, 2}); // C-2
        return static_cast<int>(std::max(std::ceil((lowestPeriod - GetPeriod(GetNote(note))) / ((ticksPerRowPair / 2.0) * portDownParam)), 1.0));
    };

    const unsigned ticks_per_row_pair = m_ModuleInfo.timeBase * (m_ModuleInfo.tickTime1 + m_ModuleInfo.tickTime2);

    // Notes can be "cancelled" by Port2Note effects under certain conditions
    std::vector<bool> note_cancelled(data.GetNumChannels(), false);

    // Set up for suspending/restoring states:
    int jump_destination_order = -1;
    int jump_destination_row = -1;

    // TODO: Remove InitialState methods and just use 1st row of state?

    // Set initial state (global)
    global_state.Reset(); // Just in case
    global_state.Set<GlobalEnumCommon::kSpeedA>(m_ModuleInfo.tickTime1); // * timebase?
    global_state.Set<GlobalEnumCommon::kSpeedB>(m_ModuleInfo.tickTime2); // * timebase?
    global_state.Set<GlobalEnumCommon::kTempo>(0); // TODO: How should tempo/speed info be stored?

    // Set initial state (per-channel)
    for (unsigned i = 0; i < channel_states.size(); ++i)
    {
        auto& channel_state = channel_states[i];
        channel_state.Reset(); // Just in case

        SoundIndex<DMF>::type si;
        switch (i)
        {
        case dmf::GameBoyChannel::SQW1:
        case dmf::GameBoyChannel::SQW2:
            si = SoundIndex<DMF>::Square{0}; // Default: 12.5% duty cycle
            break;
        case dmf::GameBoyChannel::WAVE:
            si = SoundIndex<DMF>::Wave{0}; // Default: wavetable #0
            break;
        case dmf::GameBoyChannel::NOISE:
            si = SoundIndex<DMF>::Noise{0}; // Placeholder
            break;
        default:
            assert(0);
            break;
        }

        channel_state.Set<ChannelEnumCommon::kSoundIndex>(si);
        channel_state.Set<ChannelEnumCommon::kNoteSlot>(NoteTypes::Empty{});
        channel_state.Set<ChannelEnumCommon::kVolume>(15);
        channel_state.Set<ChannelEnumCommon::kArp>(0);
        channel_state.Set<ChannelEnumCommon::kPort>({PortamentoStateData::kNone, 0});
        channel_state.Set<ChannelEnumCommon::kVibrato>(0);
        channel_state.Set<ChannelEnumCommon::kPort2NoteVolSlide>(0);
        channel_state.Set<ChannelEnumCommon::kVibratoVolSlide>(0);
        channel_state.Set<ChannelEnumCommon::kTremolo>(0);
        channel_state.Set<ChannelEnumCommon::kPanning>(127);
        channel_state.Set<ChannelEnumCommon::kVolSlide>(0);
    }

    // Main loop
    for (ChannelIndex channel = 0; channel < data.GetNumChannels(); ++channel)
    {
        auto& channel_state = channel_states[channel];
        for (OrderIndex order = 0; order < data.GetNumOrders(); ++order)
        {
            for (RowIndex row = 0; row < data.GetNumRows(); ++row)
            {
                state_reader_writers->SetWritePos(order, row);
                const auto& row_data = data.GetRow(channel, order, row);

                // If just arrived at jump destination:
                if (static_cast<int>(order) == jump_destination_order && static_cast<int>(row) == jump_destination_row)
                {
                    // Restore state copies
                    state_reader_writers->Restore();
                    jump_destination_order = -1;
                    jump_destination_row = -1;
                }

                // CHANNEL STATE - PORT2NOTE
                if (!NoteIsEmpty(row_data.note))
                {
                    // Portamento to note stops when next note is reached or on Note OFF
                    if (channel_state.Get<ChannelEnumCommon::kPort>().type == PortamentoStateData::kToNote)
                        channel_state.Set<ChannelEnumCommon::kPort>(PortamentoStateData{PortamentoStateData::kNone, 0});
                }

                if (rows_until_port_auto_off[channel] != -1)
                {
                    if (rows_until_port_auto_off[channel] == 0)
                    {
                        // Automatically turn port effects off by using the previous port type and setting the value to 0
                        PortamentoStateData temp_prev_port = channel_state.Get<ChannelEnumCommon::kPort>();
                        temp_prev_port.value = 0;
                        channel_state.Set<ChannelEnumCommon::kPort>(temp_prev_port);
                        rows_until_port_auto_off[channel] = -1;
                    }
                    else if (NoteHasPitch(row_data.note))
                    {
                        const auto& temp_port = channel_state.Get<ChannelEnumCommon::kPort>();
                        // Reset the time until port effects automatically turn off
                        if (temp_port.type == PortamentoStateData::kUp)
                            rows_until_port_auto_off[channel] = RowsUntilPortUpAutoOff(ticks_per_row_pair, row_data.note, temp_port.value);
                        else if (temp_port.type == PortamentoStateData::kDown)
                            rows_until_port_auto_off[channel] = RowsUntilPortDownAutoOff(ticks_per_row_pair, row_data.note, temp_port.value);
                        else
                        {
                            throw std::runtime_error{"Invalid portamento type."};
                        }
                    }
                    else
                    {
                        --rows_until_port_auto_off[channel];
                    }
                }

                // CHANNEL STATE - EFFECTS
                // TODO: Could this be done during the import step for greater efficiency?
                if (channel != dmf::GameBoyChannel::NOISE) // Currently not using per-channel effects on noise channel
                {
                    // -1 means the port effect wasn't used in this row, else it is the active (left-most) port's effect value.
                    // If the left-most port effect was valueless, it is set to 0 since valueless/0 seem to have the same behavior in Deflemask.
                    int16_t port_up = -1, port_down = -1, port2note = -1;

                    // Any port effect regardless of value/valueless/priority cancels any active port effect from a previous row.
                    bool prev_port_cancelled = false;

                    // When no note w/ pitch has played in the channel yet, and there is a note with pitch
                    // on the current row, if the left-most Port2Note were to be used with value > 0, that note will not play.
                    // In addition, all subsequent notes in the channel will also be cancelled until the port2note is stopped by
                    // a future port effect, note OFF, or it auto-off's. Port2Note auto-off is not implemented here though.
                    const bool port2note_note_cancellation_possible = channel_state.GetSize<ChannelEnumCommon::kNoteSlot>() == 1 && NoteHasPitch(row_data.note);
                    bool just_cancelled_note = false;
                    bool temp_note_cancelled = note_cancelled[channel];

                    // Other effects:
                    int16_t vibrato = -1, port2note_volslide = -1, vibrato_volslide = -1, tremolo = -1, panning = -1, volslide = -1, retrigger = -1, note_cut = -1, note_delay = -1;

                    auto sound_index = channel_state.Get<ChannelEnumCommon::kSoundIndex>();

                    // Loop right to left because left-most effects in effects column have priority
                    for (auto iter = std::crbegin(row_data.effect); iter != std::crend(row_data.effect); ++iter)
                    {
                        const auto& effect = *iter;
                        if (effect.code == Effects::kNoEffect)
                            continue;

                        const EffectValue effect_value = effect.value;
                        const uint8_t effect_value_normal = effect_value != kEffectValueless ? effect_value : 0;

                        switch (effect.code)
                        {
                        case Effects::kArp:
                            channel_state.Set<ChannelEnumCommon::kArp>(effect_value > 0 ? effect_value : 0); break;
                        case Effects::kPortUp:
                            prev_port_cancelled = true;
                            temp_note_cancelled = false; // Will "uncancel" notes if a port2note in this row isn't cancelling them
                            port_up = effect_value_normal;
                            break;
                        case Effects::kPortDown:
                            prev_port_cancelled = true;
                            temp_note_cancelled = false; // Will "uncancel" notes if a port2note in this row isn't cancelling them
                            port_down = effect_value_normal;
                            break;
                        case Effects::kPort2Note:
                            prev_port_cancelled = true;

                            if (port2note_note_cancellation_possible)
                            {
                                note_cancelled[channel] = effect_value > 0;
                                just_cancelled_note = effect_value > 0;
                            }
                            else
                            {
                                // Will "uncancel" notes
                                temp_note_cancelled = false;
                            }
                            port2note = effect_value_normal;
                            break;
                        case Effects::kVibrato:
                            vibrato = effect_value_normal;
                            break;
                        case Effects::kPort2NoteVolSlide:
                            port2note_volslide = effect_value_normal;
                            break;
                        case Effects::kVibratoVolSlide:
                            vibrato_volslide = effect_value_normal;
                            break;
                        case Effects::kTremolo:
                            tremolo = effect_value_normal;
                            break;
                        case Effects::kPanning:
                            panning = effect_value_normal;
                            break;
                        case Effects::kSpeedA:
                            // Handled by global state
                            break;
                        case Effects::kVolSlide:
                            volslide = effect_value_normal;
                            break;
                        case Effects::kPosJump:
                            // Handled by global state
                            break;
                        case Effects::kRetrigger:
                            retrigger = effect_value_normal;
                            break;
                        case Effects::kPatBreak:
                            // Handled by global state
                            break;
                        case Effects::kNoteCut:
                            note_cut = effect_value_normal;
                            break;
                        case Effects::kNoteDelay:
                            note_delay = effect_value_normal;
                            break;
                        case Effects::kTempo:
                            // Handled by global state
                            break;
                        case Effects::kSpeedB:
                            // Handled by global state
                            break;

                        // DMF-specific effects

                        case dmf::Effects::kGameBoySetWave:
                            if (channel != dmf::GameBoyChannel::WAVE || effect_value < 0 || effect_value >= GetTotalWavetables())
                                break; // TODO: Is this behavior correct?
                            sound_index = SoundIndex<DMF>::Wave{effect_value_normal};
                            // TODO: If a sound index is set but a note with it is never played, it should later be removed from the channel state
                            break;
                        case dmf::Effects::kGameBoySetDutyCycle:
                            if (channel > dmf::GameBoyChannel::SQW2 || effect_value < 0 || effect_value >= 4)
                                break; // Valueless of invalid 12xx effects do not do anything. TODO: What is the effect in WAVE and NOISE channels?
                            sound_index = SoundIndex<DMF>::Square{effect_value_normal};
                            // TODO: If a sound index is set but a note with it is never played, it should later be removed from the channel state
                            break;
                        default:
                            break;
                        }
                    }

                    if (!just_cancelled_note && !temp_note_cancelled)
                    {
                        // A port up/down/2note "uncancelled" the notes
                        note_cancelled[channel] = false;
                    }

                    if (!just_cancelled_note) // No port effects are set if port2note just cancelled notes
                    {
                        // Set port effects in order of priority:
                        if (port2note != -1)
                        {
                            channel_state.Set<ChannelEnumCommon::kPort>(PortamentoStateData{PortamentoStateData::kToNote, static_cast<uint8_t>(port2note)});
                            rows_until_port_auto_off[channel] = -1; // ???
                            // TODO: Handle port2note auto-off
                        }
                        else if (port_down != -1)
                        {
                            channel_state.Set<ChannelEnumCommon::kPort>(PortamentoStateData{PortamentoStateData::kDown, static_cast<uint8_t>(port_down)});
                            if (rows_until_port_auto_off[channel] == -1)
                            {
                                NoteSlot temp_note_slot = NoteHasPitch(row_data.note) ? row_data.note : channel_state.Get<ChannelEnumCommon::kNoteSlot>();
                                rows_until_port_auto_off[channel] = RowsUntilPortDownAutoOff(ticks_per_row_pair, temp_note_slot, port_down);
                            }
                        }
                        else if (port_up != -1)
                        {
                            channel_state.Set<ChannelEnumCommon::kPort>(PortamentoStateData{PortamentoStateData::kUp, static_cast<uint8_t>(port_up)});
                            if (rows_until_port_auto_off[channel] == -1)
                            {
                                NoteSlot temp_note_slot = NoteHasPitch(row_data.note) ? row_data.note : channel_state.Get<ChannelEnumCommon::kNoteSlot>();
                                rows_until_port_auto_off[channel] = RowsUntilPortUpAutoOff(ticks_per_row_pair, temp_note_slot, port_up);
                            }
                        }
                        else if (prev_port_cancelled)
                        {
                            // Cancel the previous port by using the same port type and setting the value to 0
                            PortamentoStateData prev_port = channel_state.Get<ChannelEnumCommon::kPort>();
                            prev_port.value = 0;
                            channel_state.Set<ChannelEnumCommon::kPort>(prev_port);
                        }
                    }

                    if (just_cancelled_note)
                    {
                        // TODO: Set warning here
                        // Port2Note auto-off's are not handled, so notes may be cancelled for longer than they should be.
                    }

                    // Set other effects' states (WIP)
                    if (vibrato != -1)
                        channel_state.Set<ChannelEnumCommon::kVibrato>(vibrato);
                    if (port2note_volslide != -1)
                        channel_state.Set<ChannelEnumCommon::kPort2NoteVolSlide>(port2note_volslide);
                    if (vibrato_volslide != -1)
                        channel_state.Set<ChannelEnumCommon::kVibratoVolSlide>(vibrato_volslide);
                    if (tremolo != -1)
                        channel_state.Set<ChannelEnumCommon::kTremolo>(tremolo);
                    if (panning != -1)
                        channel_state.Set<ChannelEnumCommon::kPanning>(panning);
                    if (volslide != -1)
                        channel_state.Set<ChannelEnumCommon::kVolSlide>(volslide);
                    if (retrigger != -1)
                        channel_state.Set<ChannelEnumCommon::kRetrigger>(retrigger);
                    if (note_cut != -1)
                        channel_state.Set<ChannelEnumCommon::kNoteCut>(note_cut);
                    if (note_delay != -1)
                        channel_state.Set<ChannelEnumCommon::kNoteDelay>(note_delay);

                    // TODO: This sound index may end up being unused but we have no way of knowing right now.
                    //       Should compare with sound_indexes_used at the end and remove entries that aren't used?
                    channel_state.Set<ChannelEnumCommon::kSoundIndex>(sound_index);
                }

                // CHANNEL STATE - NOTES AND SOUND INDEXES
                const NoteSlot& note_slot = row_data.note;
                if (NoteIsEmpty(note_slot))
                {
                    channel_state.Set<ChannelEnumCommon::kNoteSlot>(note_slot);
                }
                if (NoteIsOff(note_slot))
                {
                    channel_state.Set<ChannelEnumCommon::kNoteSlot>(note_slot);
                    gen_data.Get<GenDataEnumCommon::kNoteOffUsed>() = true;
                    note_cancelled[channel] = false; // An OFF also "uncancels" notes cancelled by a port2note effect
                }
                else if (NoteHasPitch(note_slot) && channel != dmf::GameBoyChannel::NOISE && !note_cancelled[channel])
                {
                    // NoteTypes::Empty should never appear in state data (but can in initial state)
                    channel_state.Set<ChannelEnumCommon::kNoteSlot, true>(note_slot);
                    const Note& note = GetNote(note_slot);

                    const auto& sound_index = channel_state.Get<ChannelEnumCommon::kSoundIndex>();

                    // Mark this square wave or wavetable as used
                    sound_indexes_used.insert(sound_index);

                    // Get lowest/highest notes
                    if (sound_index_note_extremes.count(sound_index) == 0) // 1st time
                    {
                        sound_index_note_extremes[sound_index] = { note, note };
                    }
                    else
                    {
                        auto& note_pair = sound_index_note_extremes[sound_index];
                        if (note > note_pair.second)
                        {
                            // Found a new highest note
                            note_pair.second = note;
                        }
                        if (note < note_pair.first)
                        {
                            // Found a new lowest note
                            note_pair.first = note;
                        }
                    }
                }


                // CHANNEL STATE - VOLUME
                if (row_data.volume != kDMFNoVolume)
                {
                    // The WAVE channel volume changes whether a note is attached or not, but SQ1/SQ2 need a note
                    if (channel == dmf::GameBoyChannel::WAVE)
                    {
                        // WAVE volume is actually more quantized:
                        switch (row_data.volume)
                        {
                            case 0: case 1: case 2: case 3:
                                channel_state.Set<ChannelEnumCommon::kVolume>(0); break;
                            case 4: case 5: case 6: case 7:
                                channel_state.Set<ChannelEnumCommon::kVolume>(5); break;
                            case 8: case 9: case 10: case 11:
                                channel_state.Set<ChannelEnumCommon::kVolume>(10); break;
                            case 12: case 13: case 14: case 15:
                                channel_state.Set<ChannelEnumCommon::kVolume>(15); break;
                            default:
                                assert(false && "Invalid DMF volume");
                                break;
                        }
                    }
                    else if (NoteHasPitch(row_data.note))
                    {
                        channel_state.Set<ChannelEnumCommon::kVolume>(row_data.volume);
                    }
                }

                // GLOBAL STATE
                if (channel == data.GetNumChannels() - 1) // Only update global state after all channel states for this row have been updated
                {
                    // Deflemask PosJump/PatBreak behavior (experimentally determined in Deflemask 1.1.3):
                    // The left-most PosJump or PatBreak in a given row is the one that takes effect.
                    // If the left-most PosJump or PatBreak is invalid (no value or invalid value),
                    //  every other effect of that type in the row is ignored.
                    // PosJump effects are ignored if a valid and non-ignored PatBreak is present in the row.
                    int16_t pos_jump = -1, pat_break = -1, speed_a = -1, speed_b = -1, tempo = -1;
                    bool ignore_pos_jump = false, ignore_pat_break = false;

                    // Want to check all channels to update the global state for this row
                    for (ChannelIndex channel2 = 0; channel2 < data.GetNumChannels(); ++channel2)
                    {
                        for (const auto& effect : row_data.effect)
                        {
                            switch (effect.code)
                            {
                                case Effects::kPosJump:
                                    if (ignore_pos_jump)
                                        break;
                                    if (pos_jump < 0 && (effect.value < 0 || effect.value >= data.GetNumOrders()))
                                    {
                                        ignore_pos_jump = true;
                                        break;
                                    }
                                    pos_jump = effect.value;
                                    break;
                                case Effects::kPatBreak:
                                    if (ignore_pat_break)
                                        break;
                                    if (pat_break < 0 && (effect.value < 0 || effect.value >= data.GetNumRows()))
                                    {
                                        ignore_pat_break = true;
                                        break;
                                    }
                                    pat_break = effect.value;
                                    break;
                                case Effects::kSpeedA:
                                    // TODO
                                    //speed = effect.value;
                                    break;
                                case Effects::kSpeedB:
                                    // TODO
                                    //speed = effect.value;
                                    break;
                                case Effects::kTempo:
                                    // TODO
                                    //tempo = effect.value;
                                    break;
                                default:
                                    break;
                            }
                        }
                    }

                    // Set the global state if needed

                    if (speed_a >= 0)
                        global_state.Set<GlobalEnumCommon::kSpeedA>(speed_a);
                    if (speed_b >= 0)
                        global_state.Set<GlobalEnumCommon::kSpeedB>(speed_b);
                    if (tempo >= 0)
                        global_state.Set<GlobalEnumCommon::kTempo>(tempo);

                    if (pat_break >= 0)
                    {
                        // All state data for this row (besides PatBreak) must be ready when Copy occurs
                        state_reader_writers->Save();
                        jump_destination_order = order + 1;
                        jump_destination_row = pat_break;
                        global_state.Set<GlobalEnumCommon::kPatBreak>(pat_break); // Don't want the PatBreak in the copy
                    }
                    else if (pos_jump >= 0) // PosJump only takes effect if PatBreak isn't used
                    {
                        if (pos_jump >= order) // If not a loop
                        {
                            // All state data for this row (besides PosJump) must be ready when Copy occurs
                            state_reader_writers->Save();
                            jump_destination_order = pos_jump;
                            jump_destination_row = 0;
                        }
                        else
                        {
                            // TODO: This could be overwritten by future PosJumps to the same order
                            loopback_points[pos_jump] = GetOrderRowPosition(order, row);
                        }

                        global_state.Set<GlobalEnumCommon::kPosJump>(pos_jump); // Don't want the PosJump in the copy
                    }
                }

                // TODO: Would COR and ORC affect this? EDIT: Shouldn't matter as long as O comes before R?
            }
        }
    }

    return 0;
}
