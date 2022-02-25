/*
    dmf.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Implements the ModuleInterface-derived class for Deflemask's 
    DMF files.

    DMF file support was written according to the specs at 
    http://www.deflemask.com/DMF_SPECS.txt.

    Requires the zlib compression library from https://zlib.net.
*/

#include "dmf.h"

// For inflating .dmf files so that they can be read
#include "zlib.h"
#include "zconf.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <map>

// Finish setup
const std::vector<std::string> DMFOptions = {};
REGISTER_MODULE_CPP(DMF, DMFConversionOptions, ModuleType::DMF, "dmf", DMFOptions)

#define DMF_FILE_VERSION 17 // DMF files as old as version 17 (0x11) are supported

// Information about all the systems Deflemask supports
static const std::map<DMF::SystemType, DMFSystem> DMFSystems =
{
    {DMF::SystemType::Error, {.id = 0x00, .name = "ERROR", .channels = 0}},
    {DMF::SystemType::YMU759, {.id = 0x01, .name = "YMU759", .channels = 17}}, // Removed since DMF version 19 (0x13)
    {DMF::SystemType::Genesis, {.id = 0x02, .name = "GENESIS", .channels = 10}},
    {DMF::SystemType::Genesis_CH3, {.id = 0x42, .name = "GENESIS_CH3", .channels = 13}},
    {DMF::SystemType::SMS, {.id = 0x03, .name = "SMS", .channels = 4}},
    {DMF::SystemType::GameBoy, {.id = 0x04, .name = "GAMEBOY", .channels = 4}},
    {DMF::SystemType::PCEngine, {.id = 0x05, .name = "PCENGINE", .channels = 6}},
    {DMF::SystemType::NES, {.id = 0x06, .name = "NES", .channels = 5}},
    {DMF::SystemType::C64_SID_8580, {.id = 0x07, .name = "C64_SID_8580", .channels = 3}},
    {DMF::SystemType::C64_SID_6581, {.id = 0x47, .name = "C64_SID_6581", .channels = 3}},
    {DMF::SystemType::Arcade, {.id = 0x08, .name = "ARCADE", .channels = 13}},
    {DMF::SystemType::NeoGeo, {.id = 0x09, .name = "NEOGEO", .channels = 13}},
    {DMF::SystemType::NeoGeo_CH2, {.id = 0x49, .name = "NEOGEO_CH2", .channels = 16}}
};

const DMFSystem DMF::Systems(DMF::SystemType systemType) { return DMFSystems.at(systemType); }

DMF::DMF()
{
    // Initialize pointers to nullptr to prevent segfault when freeing memory if the import fails:
    m_VisualInfo.songName = "";
    m_VisualInfo.songAuthor = "";
    m_PatternValues = nullptr;
    m_ChannelEffectsColumnsCount = nullptr;
    m_PatternMatrixValues = nullptr;
    m_PatternMatrixMaxValues = nullptr;
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
    // Free memory allocated for members
    if (m_PatternMatrixMaxValues)
    {
        for (int channel = 0; channel < m_System.channels; channel++)
        {
            for (int i = 0; i < m_PatternMatrixMaxValues[channel] + 1; i++)
            {
                delete m_PatternValues[channel][i];
                m_PatternValues[channel][i] = nullptr;
            }
            delete[] m_PatternValues[channel];
            m_PatternValues[channel] = nullptr;
        }
        delete[] m_PatternValues;
        m_PatternValues = nullptr;
    }

    if (m_PatternMatrixValues)
    {
        for (int i = 0; i < m_System.channels; i++)
        {
            delete[] m_PatternMatrixValues[i];
            m_PatternMatrixValues[i] = nullptr;
        }
        delete[] m_PatternMatrixValues;
        m_PatternMatrixValues = nullptr;
    }
    
    delete[] m_PatternMatrixMaxValues;
    m_PatternMatrixMaxValues = nullptr;

    if (m_Instruments)
    {
        for (int i = 0; i < m_TotalInstruments; i++) 
        {
            delete[] m_Instruments[i].stdArpEnvValue;
            delete[] m_Instruments[i].stdDutyNoiseEnvValue;
            delete[] m_Instruments[i].stdVolEnvValue;
            delete[] m_Instruments[i].stdWavetableEnvValue;
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
    
    delete[] m_ChannelEffectsColumnsCount;
    m_ChannelEffectsColumnsCount = nullptr;

    if (m_PCMSamples)
    {
        for (int sample = 0; sample < m_TotalPCMSamples; sample++) 
        {
            delete[] m_PCMSamples[sample].data;
        }
        delete[] m_PCMSamples;
        m_PCMSamples = nullptr;
    }
}

void DMFConversionOptions::PrintHelp()
{ 
    std::cout << "DMF files have no conversion options.\n";
}

void DMF::Import(const std::string& filename)
{
    CleanUp();
    m_Status.Clear();

    const bool silent = ModuleUtils::GetCoreOptions().silent;

    if (!silent)
        std::cout << "Starting to import the DMF file...\n";

    if (ModuleUtils::GetTypeFromFilename(filename) != ModuleType::DMF)
    {
        throw ModuleException(ModuleException::Category::Import, DMF::ImportError::UnspecifiedError, "Input file has the wrong file extension.\nPlease use a DMF file.");
    }

    if (!silent)
        std::cout << "DMF Filename: " << filename << "\n";

    zstr::ifstream fin(filename, std::ios_base::binary);
    if (fin.fail())
    {
        throw ModuleException(ModuleException::Category::Import, DMF::ImportError::UnspecifiedError, "Failed to open DMF file.");
    }

    ///////////////// FORMAT FLAGS
    
    char header[17];
    fin.read(header, 16);
    header[16] = '\0';
    
    if (std::string(header) != ".DelekDefleMask.")
    {
        throw ModuleException(ModuleException::Category::Import, DMF::ImportError::UnspecifiedError, "DMF format header is bad.");
    }

    m_DMFFileVersion = fin.get();
    if (m_DMFFileVersion < DMF_FILE_VERSION)
    {
        std::stringstream stream;
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << DMF_FILE_VERSION;
        std::string hex = stream.str();

        std::string errorMsg = "Deflemask file version must be " + std::to_string(DMF_FILE_VERSION) + " (" + hex + ") or higher.\n";
        
        stream.clear();
        stream.str("");
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << (int)m_DMFFileVersion;
        hex = stream.str();

        errorMsg += "The given DMF file is version " + std::to_string(m_DMFFileVersion) + " (" + hex + ").\n";
        errorMsg += "       You can convert older DMF files to a supported version by opening them in a newer version of DefleMask and then saving them.";
        
        throw ModuleException(ModuleException::Category::Import, DMF::ImportError::UnspecifiedError, errorMsg);
    }
    else if (!silent)
    {
        std::stringstream stream;
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << (int)m_DMFFileVersion;
        std::string hex = stream.str();

        std::cout << "DMF version " + std::to_string(m_DMFFileVersion) + " (" + hex + ")\n";
    }

    ///////////////// SYSTEM SET
    
    m_System = GetSystem(fin.get());

    if (!silent)
        std::cout << "System: " << m_System.name << " (channels: " << std::to_string(m_System.channels) << ")\n";

    ///////////////// VISUAL INFORMATION
    LoadVisualInfo(fin);
    if (!silent)
    {
        std::cout << "Title: " << m_VisualInfo.songName << "\n";
        std::cout << "Author: " << m_VisualInfo.songAuthor << "\n";
        std::cout << "Loaded visual information." << "\n";
    }

    ///////////////// MODULE INFORMATION
    LoadModuleInfo(fin);
    if (!silent)
        std::cout << "Loaded module.\n";

    ///////////////// PATTERN MATRIX VALUES
    LoadPatternMatrixValues(fin);
    if (!silent)
        std::cout << "Loaded pattern matrix values.\n";

    ///////////////// INSTRUMENTS DATA
    LoadInstrumentsData(fin);
    if (!silent)
        std::cout << "Loaded instruments.\n";

    ///////////////// WAVETABLES DATA
    LoadWavetablesData(fin);
    if (!silent)
        std::cout << "Loaded " << std::to_string(m_TotalWavetables) << " wavetable(s).\n";

    ///////////////// PATTERNS DATA
    LoadPatternsData(fin);
    if (!silent)
        std::cout << "Loaded patterns.\n";

    ///////////////// PCM SAMPLES DATA
    LoadPCMSamplesData(fin);
    if (!silent)
        std::cout << "Loaded PCM Samples.\n";

    if (!silent)
        std::cout << "Done importing DMF file.\n\n";
}

void DMF::Export(const std::string& filename)
{
    // Not implemented
}

DMFSystem DMF::GetSystem(uint8_t systemByte) const
{
    for (const auto& mapPair : DMFSystems)
    {
        if (mapPair.second.id == systemByte)
            return mapPair.second;
    }
    return DMFSystems.at(DMF::SystemType::Error); // Error: System byte invalid
}

void DMF::LoadVisualInfo(zstr::ifstream& fin)
{
    m_VisualInfo.songNameLength = fin.get();

    char* tempStr = new char[m_VisualInfo.songNameLength + 1];
    fin.read(tempStr, m_VisualInfo.songNameLength);
    tempStr[m_VisualInfo.songNameLength] = '\0';
    m_VisualInfo.songName = tempStr;
    delete[] tempStr;

    m_VisualInfo.songAuthorLength = fin.get();

    tempStr = new char[m_VisualInfo.songAuthorLength + 1];
    fin.read(tempStr, m_VisualInfo.songAuthorLength);
    tempStr[m_VisualInfo.songAuthorLength] = '\0';
    m_VisualInfo.songAuthor = tempStr;
    delete[] tempStr;

    m_VisualInfo.highlightAPatterns = fin.get();
    m_VisualInfo.highlightBPatterns = fin.get();
}

void DMF::LoadModuleInfo(zstr::ifstream& fin)
{
    m_ModuleInfo.timeBase = fin.get();
    m_ModuleInfo.tickTime1 = fin.get();
    m_ModuleInfo.tickTime2 = fin.get();
    m_ModuleInfo.framesMode = fin.get();
    m_ModuleInfo.usingCustomHZ = fin.get();
    m_ModuleInfo.customHZValue1 = fin.get();
    m_ModuleInfo.customHZValue2 = fin.get();
    m_ModuleInfo.customHZValue3 = fin.get();

    if (m_DMFFileVersion >= 24) // DMF version 24 (0x18) and newer.
    {
        // Newer versions read 4 bytes here
        m_ModuleInfo.totalRowsPerPattern = fin.get();
        m_ModuleInfo.totalRowsPerPattern |= fin.get() << 8;
        m_ModuleInfo.totalRowsPerPattern |= fin.get() << 16;
        m_ModuleInfo.totalRowsPerPattern |= fin.get() << 24;
    }
    else // DMF version 23 (0x17) and older. WARNING: I don't have the specs for version 23 (0x17), so this may be wrong.
    {
        // Earlier versions such as 22 (0x16) only read one byte here
        m_ModuleInfo.totalRowsPerPattern = fin.get();
    }

    m_ModuleInfo.totalRowsInPatternMatrix = fin.get();

    // Prior to Deflemask Version 0.11.1, arpeggio tick speed was stored here
    // I don't have the specs for DMF version 20 (0x14), but based on a real DMF file of that version, it is the first DMF version 
    //      to NOT contain the arpeggio tick speed byte.
    if (m_DMFFileVersion <= 19) // DMF version 19 (0x13) and older
    {
        fin.get(); // arpTickSpeed: Discard for now
    }
}

void DMF::LoadPatternMatrixValues(zstr::ifstream& fin)
{
    // Format: patterMatrixValues[channel][pattern matrix row]
    m_PatternMatrixValues = new uint8_t*[m_System.channels];
    m_PatternMatrixMaxValues = new uint8_t[m_System.channels];

    for (int i = 0; i < m_System.channels; i++)
    {
        m_PatternMatrixMaxValues[i] = 0;
        m_PatternMatrixValues[i] = new uint8_t[m_ModuleInfo.totalRowsInPatternMatrix];
        
        for (int j = 0; j < m_ModuleInfo.totalRowsInPatternMatrix; j++)
        {
            m_PatternMatrixValues[i][j] = fin.get();
            
            if (m_PatternMatrixValues[i][j] > m_PatternMatrixMaxValues[i])
            {
                m_PatternMatrixMaxValues[i] = m_PatternMatrixValues[i][j];
            }
        }
    }
}

void DMF::LoadInstrumentsData(zstr::ifstream& fin)
{
    m_TotalInstruments = fin.get();
    m_Instruments = new DMFInstrument[m_TotalInstruments];

    for (int i = 0; i < m_TotalInstruments; i++)
    {
        m_Instruments[i] = LoadInstrument(fin, m_System);
    }
}

DMFInstrument DMF::LoadInstrument(zstr::ifstream& fin, DMFSystem systemType)
{
    DMFInstrument inst;

    uint8_t name_size = fin.get();

    char* tempStr = new char[name_size + 1];
    fin.read(tempStr, name_size);
    tempStr[name_size] = '\0';
    inst.name = tempStr;
    delete[] tempStr;

    inst.mode = fin.get(); // 1 = FM; 0 = Standard

    // Initialize to nullptr to prevent issues if they are freed later without ever dynamically allocating memory
    inst.stdVolEnvValue = nullptr;
    inst.stdArpEnvValue = nullptr;
    inst.stdDutyNoiseEnvValue = nullptr;
    inst.stdWavetableEnvValue = nullptr;

    if (inst.mode == 1) // FM instrument
    {
        int totalOperators;

        if (m_DMFFileVersion > 18) // Newer than DMF version 18 (0x12)
        {
            inst.fmALG = fin.get();
            inst.fmFB = fin.get();
            inst.fmLFO = fin.get();
            inst.fmLFO2 = fin.get();
            totalOperators = 4;
        }
        else
        {
            inst.fmALG = fin.get();
            fin.get(); // Reserved byte (must be 0)
            inst.fmFB = fin.get();
            fin.get(); // Reserved byte (must be 0)
            inst.fmLFO = fin.get();
            fin.get(); // Reserved byte (must be 0)

            const bool totalOperatorsBool = fin.get();
            totalOperators = totalOperatorsBool ? 4 : 2;

            inst.fmLFO2 = fin.get();
        }

        for (int i = 0; i < totalOperators; i++)
        {
            if (m_DMFFileVersion > 18) // Newer than DMF version 18 (0x12)
            {
                inst.fmAM = fin.get();
                inst.fmAR = fin.get();
                inst.fmDR = fin.get();
                inst.fmMULT = fin.get();
                inst.fmRR = fin.get();
                inst.fmSL = fin.get();
                inst.fmTL = fin.get();
                inst.fmDT2 = fin.get();
                inst.fmRS = fin.get();
                inst.fmDT = fin.get();
                inst.fmD2R = fin.get();
                inst.fmSSGMODE = fin.get();
            }
            else // DMF version 17 (0x11) or older
            {
                inst.fmAM = fin.get();
                inst.fmAR = fin.get();
                inst.fmDAM = fin.get();
                inst.fmDR = fin.get();
                inst.fmDVB = fin.get();
                inst.fmEGT = fin.get();
                inst.fmKSL = fin.get();
                inst.fmMULT = fin.get();
                inst.fmRR = fin.get();
                inst.fmSL = fin.get();
                inst.fmSUS = fin.get();
                inst.fmTL = fin.get();
                inst.fmVIB = fin.get();
                inst.fmWS = fin.get();
                inst.fmKSR = fin.get(); // RS on SEGA Genesis
                inst.fmDT = fin.get();
                inst.fmD2R = fin.get();
                inst.fmSSGMODE = fin.get();
            }
        }
    }
    else if (inst.mode == 0) // Standard instrument
    {
        if (m_DMFFileVersion <= 17) // DMF version 17 (0x11) or older
        {
            // Volume macro
            inst.stdVolEnvSize = fin.get();
            inst.stdVolEnvValue = new int32_t[inst.stdVolEnvSize];
            
            for (int i = 0; i < inst.stdVolEnvSize; i++)
            {
                // 4 bytes, little-endian
                inst.stdVolEnvValue[i] = fin.get();
                inst.stdVolEnvValue[i] |= fin.get() << 8;
                inst.stdVolEnvValue[i] |= fin.get() << 16;
                inst.stdVolEnvValue[i] |= fin.get() << 24;
            }

            // Always get envelope loop position byte regardless of envelope size
            inst.stdVolEnvLoopPos = fin.get();
        }
        else if (systemType.id != DMFSystems.at(DMF::SystemType::GameBoy).id) // Not a Game Boy and DMF version 18 (0x12) or newer
        {
            // Volume macro
            inst.stdVolEnvSize = fin.get();
            inst.stdVolEnvValue = new int32_t[inst.stdVolEnvSize];
            
            for (int i = 0; i < inst.stdVolEnvSize; i++)
            {
                // 4 bytes, little-endian
                inst.stdVolEnvValue[i] = fin.get();
                inst.stdVolEnvValue[i] |= fin.get() << 8;
                inst.stdVolEnvValue[i] |= fin.get() << 16;
                inst.stdVolEnvValue[i] |= fin.get() << 24;
            }

            if (inst.stdVolEnvSize > 0)
                inst.stdVolEnvLoopPos = fin.get();
        }

        // Arpeggio macro
        inst.stdArpEnvSize = fin.get();
        inst.stdArpEnvValue = new int32_t[inst.stdArpEnvSize];
        
        for (int i = 0; i < inst.stdArpEnvSize; i++)
        {
            // 4 bytes, little-endian
            inst.stdArpEnvValue[i] = fin.get();
            inst.stdArpEnvValue[i] |= fin.get() << 8;
            inst.stdArpEnvValue[i] |= fin.get() << 16;
            inst.stdArpEnvValue[i] |= fin.get() << 24;
        }

        if (inst.stdArpEnvSize > 0 || m_DMFFileVersion <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.stdArpEnvLoopPos = fin.get();
        
        inst.stdArpMacroMode = fin.get();

        // Duty/Noise macro
        inst.stdDutyNoiseEnvSize = fin.get();
        inst.stdDutyNoiseEnvValue = new int32_t[inst.stdDutyNoiseEnvSize];
        
        for (int i = 0; i < inst.stdDutyNoiseEnvSize; i++)
        {
            // 4 bytes, little-endian
            inst.stdDutyNoiseEnvValue[i] = fin.get();
            inst.stdDutyNoiseEnvValue[i] |= fin.get() << 8;
            inst.stdDutyNoiseEnvValue[i] |= fin.get() << 16;
            inst.stdDutyNoiseEnvValue[i] |= fin.get() << 24;
        }

        if (inst.stdDutyNoiseEnvSize > 0 || m_DMFFileVersion <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.stdDutyNoiseEnvLoopPos = fin.get();

        // Wavetable macro
        inst.stdWavetableEnvSize = fin.get();
        inst.stdWavetableEnvValue = new int32_t[inst.stdWavetableEnvSize];
        
        for (int i = 0; i < inst.stdWavetableEnvSize; i++)
        {
            // 4 bytes, little-endian
            inst.stdWavetableEnvValue[i] = fin.get();
            inst.stdWavetableEnvValue[i] |= fin.get() << 8;
            inst.stdWavetableEnvValue[i] |= fin.get() << 16;
            inst.stdWavetableEnvValue[i] |= fin.get() << 24;
        }

        if (inst.stdWavetableEnvSize > 0 || m_DMFFileVersion <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.stdWavetableEnvLoopPos = fin.get();

        // Per system data
        if (systemType.id == DMFSystems.at(DMF::SystemType::C64_SID_8580).id || systemType.id == DMFSystems.at(DMF::SystemType::C64_SID_6581).id) // Using Commodore 64
        {
            inst.stdC64TriWaveEn = fin.get();
            inst.stdC64SawWaveEn = fin.get();
            inst.stdC64PulseWaveEn = fin.get();
            inst.stdC64NoiseWaveEn = fin.get();
            inst.stdC64Attack = fin.get();
            inst.stdC64Decay = fin.get();
            inst.stdC64Sustain = fin.get();
            inst.stdC64Release = fin.get();
            inst.stdC64PulseWidth = fin.get();
            inst.stdC64RingModEn = fin.get();
            inst.stdC64SyncModEn = fin.get();
            inst.stdC64ToFilter = fin.get();
            inst.stdC64VolMacroToFilterCutoffEn = fin.get();
            inst.stdC64UseFilterValuesFromInst = fin.get();
            
            // Filter globals
            inst.stdC64FilterResonance = fin.get();
            inst.stdC64FilterCutoff = fin.get();
            inst.stdC64FilterHighPass = fin.get();
            inst.stdC64FilterLowPass = fin.get();
            inst.stdC64FilterCH2Off = fin.get();
        }
        else if (systemType.id == DMFSystems.at(DMF::SystemType::GameBoy).id && m_DMFFileVersion >= 18) // Using Game Boy and DMF version is 18 or newer
        {
            inst.stdGBEnvVol = fin.get();
            inst.stdGBEnvDir = fin.get();
            inst.stdGBEnvLen = fin.get();
            inst.stdGBSoundLen = fin.get();
        }
    }

    return inst;
}

void DMF::LoadWavetablesData(zstr::ifstream& fin)
{
    m_TotalWavetables = fin.get();
    
    m_WavetableSizes = new uint32_t[m_TotalWavetables];
    m_WavetableValues = new uint32_t*[m_TotalWavetables];

    for (int i = 0; i < m_TotalWavetables; i++)
    {
        m_WavetableSizes[i] = fin.get();
        m_WavetableSizes[i] |= fin.get() << 8;
        m_WavetableSizes[i] |= fin.get() << 16;
        m_WavetableSizes[i] |= fin.get() << 24;

        m_WavetableValues[i] = new uint32_t[m_WavetableSizes[i]];

        for (unsigned j = 0; j < m_WavetableSizes[i]; j++)
        {
            m_WavetableValues[i][j] = fin.get();
            m_WavetableValues[i][j] |= fin.get() << 8;
            m_WavetableValues[i][j] |= fin.get() << 16;
            m_WavetableValues[i][j] |= fin.get() << 24;
        }
    }
}

void DMF::LoadPatternsData(zstr::ifstream& fin)
{
    // patternValues[channel][pattern number][pattern row number]
    m_PatternValues = new DMFChannelRow**[m_System.channels];
    m_ChannelEffectsColumnsCount = new uint8_t[m_System.channels];
    
    uint8_t patternMatrixNumber;

    for (unsigned channel = 0; channel < m_System.channels; channel++)
    {
        m_ChannelEffectsColumnsCount[channel] = fin.get();

        m_PatternValues[channel] = new DMFChannelRow*[m_PatternMatrixMaxValues[channel] + 1]();

        for (unsigned rowInPatternMatrix = 0; rowInPatternMatrix < m_ModuleInfo.totalRowsInPatternMatrix; rowInPatternMatrix++)
        {
            patternMatrixNumber = m_PatternMatrixValues[channel][rowInPatternMatrix];

            if (m_PatternValues[channel][patternMatrixNumber]) // If pattern has been loaded previously
            {
                // Skip patterns that have already been loaded (unnecessary information)
                // zstr's seekg method does not seem to work, so I will use this:
                unsigned seekAmount = (8 + 4 * m_ChannelEffectsColumnsCount[channel]) * m_ModuleInfo.totalRowsPerPattern;
                while (0 < seekAmount--)
                {
                    fin.get();
                }
                continue;
            }

            m_PatternValues[channel][patternMatrixNumber] = new DMFChannelRow[m_ModuleInfo.totalRowsPerPattern];

            for (uint32_t row = 0; row < m_ModuleInfo.totalRowsPerPattern; row++)
            {
                m_PatternValues[channel][patternMatrixNumber][row] = LoadPatternRow(fin, m_ChannelEffectsColumnsCount[channel]);
            }
        }
    }
}

DMFChannelRow DMF::LoadPatternRow(zstr::ifstream& fin, int effectsColumnsCount)
{
    DMFChannelRow pat;
    pat.note.pitch = fin.get();
    pat.note.pitch |= fin.get() << 8; // Unused byte. Storing it anyway.
    pat.note.octave = fin.get();
    pat.note.octave |= fin.get() << 8; // Unused byte. Storing it anyway.
    pat.volume = fin.get();
    pat.volume |= fin.get() << 8;

    // Apparently, the note pitch for C- can be either 0 or 12. I'm setting it to 0 always.
    if (pat.note.pitch == DMFNotePitch::C_Alt)
    {
        pat.note.pitch = (uint16_t)DMFNotePitch::C;
        pat.note.octave++;
    }

    // NOTE: C# is considered the 1st note of an octave rather than C- like in the Deflemask program.

    for (int col = 0; col < effectsColumnsCount; col++)
    {
        pat.effect[col].code = fin.get();
        pat.effect[col].code |= fin.get() << 8;
        pat.effect[col].value = fin.get();
        pat.effect[col].value |= fin.get() << 8;
    }

    // Initialize the rest to zero
    for (int col = effectsColumnsCount; col < DMF_MAX_EFFECTS_COLUMN_COUNT; col++)
    {
        pat.effect[col] = {(int16_t)DMFEffectCode::NoEffect, (int16_t)DMFEffectCode::NoEffectVal};
    }

    pat.instrument = fin.get();
    pat.instrument |= fin.get() << 8;

    return pat;
}

void DMF::LoadPCMSamplesData(zstr::ifstream& fin)
{
    m_TotalPCMSamples = fin.get();
    m_PCMSamples = new DMFPCMSample[m_TotalPCMSamples];

    for (unsigned sample = 0; sample < m_TotalPCMSamples; sample++)
    {
        m_PCMSamples[sample] = LoadPCMSample(fin);
    }
}

DMFPCMSample DMF::LoadPCMSample(zstr::ifstream& fin)
{
    DMFPCMSample sample;

    sample.size = fin.get();
    sample.size |= fin.get() << 8;
    sample.size |= fin.get() << 16;
    sample.size |= fin.get() << 24;

    if (m_DMFFileVersion >= 24) // DMF version 24 (0x18)
    {
        // Read PCM sample name
        uint8_t name_size = fin.get();

        char* tempStr = new char[name_size];
        fin.read(tempStr, name_size);
        tempStr[name_size] = '\0';
        sample.name = tempStr;
        delete[] tempStr;
    }
    else // DMF version 23 (0x17) and older. WARNING: I don't have the specs for version 23 (0x17), so this may be wrong.
    {
        // PCM samples don't have names in this DMF version
        sample.name = "";
    }

    sample.rate = fin.get();
    sample.pitch = fin.get();
    sample.amp = fin.get();

    if (m_DMFFileVersion >= 22) // DMF version 22 (0x16) and newer
    {
        sample.bits = fin.get();
    }

    sample.data = new uint16_t[sample.size];
    for (uint32_t i = 0; i < sample.size; i++)
    {
        sample.data[i] = fin.get();
        sample.data[i] |= fin.get() << 8;
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
    numerator = 15.0 * globalTick;
    denominator = (m_ModuleInfo.timeBase + 1) * (m_ModuleInfo.tickTime1 + m_ModuleInfo.tickTime2);

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

int DMFGetNoteRange(const DMFNote& low, const DMFNote& high)
{
    // Returns range in semitones. -1 if an error occurred
    
    if (!low.HasPitch() || !high.HasPitch())
        return -1;
    
    return high.octave * 12 + high.pitch - low.octave * 12 - low.pitch;
}

bool operator==(const DMFNote& lhs, const DMFNote& rhs)
{
    return lhs.octave == rhs.octave && lhs.pitch == rhs.pitch;
}

bool operator!=(const DMFNote& lhs, const DMFNote& rhs)
{
    return lhs.octave != rhs.octave || lhs.pitch != rhs.pitch;
}

// For the following operators:
// Assumes note isn't Note OFF or Empty note
// Notes must use the DMF convention where the note C# is the 1st note of an octave rather than C-

bool operator>(const DMFNote& lhs, const DMFNote& rhs)
{
    return (lhs.octave << 4) + lhs.pitch > (rhs.octave << 4) + rhs.pitch;
}

bool operator<(const DMFNote& lhs, const DMFNote& rhs)
{
    return (lhs.octave << 4) + lhs.pitch < (rhs.octave << 4) + rhs.pitch;
}

bool operator>=(const DMFNote& lhs, const DMFNote& rhs)
{
    return (lhs.octave << 4) + lhs.pitch >= (rhs.octave << 4) + rhs.pitch;
}

bool operator<=(const DMFNote& lhs, const DMFNote& rhs)
{
    return (lhs.octave << 4) + lhs.pitch <= (rhs.octave << 4) + rhs.pitch;
}
