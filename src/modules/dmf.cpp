/*
    dmf.cpp
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Implements a ModuleInterface-derived class for Deflemask's 
    DMF files.

    DMF file support was written according to the specs at 
    http://www.deflemask.com/DMF_SPECS.txt.
*/

#include "dmf.h"
#include "utils/utils.h"

// For inflating .dmf files so that they can be read
#include <zlib.h>
#include <zconf.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <map>

using namespace d2m;
using namespace d2m::dmf;
// DO NOT use any module namespace other than d2m::dmf

// Define module info
MODULE_DEFINE(DMF, DMFConversionOptions, ModuleType::DMF, "Deflemask", "dmf", {})

#define DMF_FILE_VERSION_MIN 17 // DMF files as old as version 17 (0x11) are supported
#define DMF_FILE_VERSION_MAX 25 // DMF files as new as version 25 (0x19) are supported

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
    {DMF::SystemType::NeoGeo_CH2, System(DMF::SystemType::NeoGeo_CH2, 0x49, "Neo Geo (Ext. CH2)", 16)}
};

const System& DMF::Systems(DMF::SystemType systemType) { return DMFSystems.at(systemType); }

DMF::DMF()
{
    // Initialize pointers to nullptr to prevent segfault when freeing memory if the import fails:
    m_VisualInfo.songName.clear();
    m_VisualInfo.songAuthor.clear();
    m_PatternValues = nullptr;
    m_ChannelEffectsColumnsCount = nullptr;
    m_PatternMatrixValues = nullptr;
    m_PatternMatrixMaxValues = nullptr;
    m_Instruments = nullptr;
    m_WavetableSizes = nullptr;
    m_WavetableValues = nullptr;
    m_PCMSamples = nullptr;
    m_PatternNames.clear();
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

    m_PatternNames.clear();
}

void DMF::ImportRaw(const std::string& filename)
{
    CleanUp();

    const bool verbose = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::Verbose).GetValue<bool>();

    if (verbose)
        std::cout << "Starting to import the DMF file...\n";

    if (Registrar::GetTypeFromFilename(filename) != ModuleType::DMF)
    {
        throw ModuleException(ModuleException::Category::Import, DMF::ImportError::UnspecifiedError, "Input file has the wrong file extension.\nPlease use a DMF file.");
    }

    if (verbose)
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
    
    m_System = GetSystem(fin.get());

    if (verbose)
        std::cout << "System: " << m_System.name << " (channels: " << std::to_string(m_System.channels) << ")\n";

    ///////////////// VISUAL INFORMATION
    LoadVisualInfo(fin);
    if (verbose)
    {
        std::cout << "Title: " << m_VisualInfo.songName << "\n";
        std::cout << "Author: " << m_VisualInfo.songAuthor << "\n";
        std::cout << "Loaded visual information." << "\n";
    }

    ///////////////// MODULE INFORMATION
    LoadModuleInfo(fin);
    if (verbose)
        std::cout << "Loaded module.\n";

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
        std::cout << "Loaded PCM Samples.\n";

    if (verbose)
        std::cout << "Done importing DMF file.\n\n";
}

void DMF::ExportRaw(const std::string& filename)
{
    // Not implemented
    throw NotImplementedException();
}

void DMF::ConvertRaw(const Module* input)
{
    // Not implemented
    throw NotImplementedException();
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

            // Version 1.1 introduces pattern names
            if (m_DMFFileVersion >= 25) // DMF version 25 (0x19) and newer
            {
                const int patternNameLength = fin.get();
                char* tempStr = new char[patternNameLength + 1];
                fin.read(tempStr, patternNameLength);
                tempStr[patternNameLength] = '\0';
                m_PatternNames[(j * m_System.channels) + i] = tempStr;
                delete[] tempStr;
            }
        }
    }
}

void DMF::LoadInstrumentsData(zstr::ifstream& fin)
{
    m_TotalInstruments = fin.get();
    m_Instruments = new Instrument[m_TotalInstruments];

    for (int i = 0; i < m_TotalInstruments; i++)
    {
        m_Instruments[i] = LoadInstrument(fin, m_System.type);
    }
}

Instrument DMF::LoadInstrument(zstr::ifstream& fin, DMF::SystemType systemType)
{
    Instrument inst = {};

    uint8_t name_size = fin.get();

    char* tempStr = new char[name_size + 1];
    fin.read(tempStr, name_size);
    tempStr[name_size] = '\0';
    inst.name = tempStr;
    delete[] tempStr;

    // Get instrument mode (Standard or FM)
    inst.mode = Instrument::InvalidMode;
    switch (fin.get())
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
            inst.std.volEnvSize = fin.get();
            inst.std.volEnvValue = new int32_t[inst.std.volEnvSize];
            
            for (int i = 0; i < inst.std.volEnvSize; i++)
            {
                // 4 bytes, little-endian
                inst.std.volEnvValue[i] = fin.get();
                inst.std.volEnvValue[i] |= fin.get() << 8;
                inst.std.volEnvValue[i] |= fin.get() << 16;
                inst.std.volEnvValue[i] |= fin.get() << 24;
            }

            // Always get envelope loop position byte regardless of envelope size
            inst.std.volEnvLoopPos = fin.get();
        }
        else if (systemType != DMF::SystemType::GameBoy) // Not a Game Boy and DMF version 18 (0x12) or newer
        {
            // Volume macro
            inst.std.volEnvSize = fin.get();
            inst.std.volEnvValue = new int32_t[inst.std.volEnvSize];
            
            for (int i = 0; i < inst.std.volEnvSize; i++)
            {
                // 4 bytes, little-endian
                inst.std.volEnvValue[i] = fin.get();
                inst.std.volEnvValue[i] |= fin.get() << 8;
                inst.std.volEnvValue[i] |= fin.get() << 16;
                inst.std.volEnvValue[i] |= fin.get() << 24;
            }

            if (inst.std.volEnvSize > 0)
                inst.std.volEnvLoopPos = fin.get();
        }

        // Arpeggio macro
        inst.std.arpEnvSize = fin.get();
        inst.std.arpEnvValue = new int32_t[inst.std.arpEnvSize];
        
        for (int i = 0; i < inst.std.arpEnvSize; i++)
        {
            // 4 bytes, little-endian
            inst.std.arpEnvValue[i] = fin.get();
            inst.std.arpEnvValue[i] |= fin.get() << 8;
            inst.std.arpEnvValue[i] |= fin.get() << 16;
            inst.std.arpEnvValue[i] |= fin.get() << 24;
        }

        if (inst.std.arpEnvSize > 0 || m_DMFFileVersion <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.std.arpEnvLoopPos = fin.get();
        
        inst.std.arpMacroMode = fin.get();

        // Duty/Noise macro
        inst.std.dutyNoiseEnvSize = fin.get();
        inst.std.dutyNoiseEnvValue = new int32_t[inst.std.dutyNoiseEnvSize];
        
        for (int i = 0; i < inst.std.dutyNoiseEnvSize; i++)
        {
            // 4 bytes, little-endian
            inst.std.dutyNoiseEnvValue[i] = fin.get();
            inst.std.dutyNoiseEnvValue[i] |= fin.get() << 8;
            inst.std.dutyNoiseEnvValue[i] |= fin.get() << 16;
            inst.std.dutyNoiseEnvValue[i] |= fin.get() << 24;
        }

        if (inst.std.dutyNoiseEnvSize > 0 || m_DMFFileVersion <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.std.dutyNoiseEnvLoopPos = fin.get();

        // Wavetable macro
        inst.std.wavetableEnvSize = fin.get();
        inst.std.wavetableEnvValue = new int32_t[inst.std.wavetableEnvSize];
        
        for (int i = 0; i < inst.std.wavetableEnvSize; i++)
        {
            // 4 bytes, little-endian
            inst.std.wavetableEnvValue[i] = fin.get();
            inst.std.wavetableEnvValue[i] |= fin.get() << 8;
            inst.std.wavetableEnvValue[i] |= fin.get() << 16;
            inst.std.wavetableEnvValue[i] |= fin.get() << 24;
        }

        if (inst.std.wavetableEnvSize > 0 || m_DMFFileVersion <= 17) // DMF version 17 and older always gets envelope loop position byte
            inst.std.wavetableEnvLoopPos = fin.get();

        // Per system data
        if (systemType == DMF::SystemType::C64_SID_8580 || systemType == DMF::SystemType::C64_SID_6581) // Using Commodore 64
        {
            inst.std.c64TriWaveEn = fin.get();
            inst.std.c64SawWaveEn = fin.get();
            inst.std.c64PulseWaveEn = fin.get();
            inst.std.c64NoiseWaveEn = fin.get();
            inst.std.c64Attack = fin.get();
            inst.std.c64Decay = fin.get();
            inst.std.c64Sustain = fin.get();
            inst.std.c64Release = fin.get();
            inst.std.c64PulseWidth = fin.get();
            inst.std.c64RingModEn = fin.get();
            inst.std.c64SyncModEn = fin.get();
            inst.std.c64ToFilter = fin.get();
            inst.std.c64VolMacroToFilterCutoffEn = fin.get();
            inst.std.c64UseFilterValuesFromInst = fin.get();
            
            // Filter globals
            inst.std.c64FilterResonance = fin.get();
            inst.std.c64FilterCutoff = fin.get();
            inst.std.c64FilterHighPass = fin.get();
            inst.std.c64FilterLowPass = fin.get();
            inst.std.c64FilterCH2Off = fin.get();
        }
        else if (systemType == DMF::SystemType::GameBoy && m_DMFFileVersion >= 18) // Using Game Boy and DMF version is 18 or newer
        {
            inst.std.gbEnvVol = fin.get();
            inst.std.gbEnvDir = fin.get();
            inst.std.gbEnvLen = fin.get();
            inst.std.gbSoundLen = fin.get();
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
                inst.fm.sus = fin.get();
                inst.fm.fb = fin.get();
                inst.fm.dc = fin.get();
                inst.fm.dm = fin.get();
            }
            else
            {
                inst.fm.alg = fin.get();
                inst.fm.fb = fin.get();
                inst.fm.lfo = fin.get();
                inst.fm.lfo2 = fin.get();
            }

            inst.fm.numOperators = 4;
        }
        else
        {
            inst.fm.alg = fin.get();
            fin.get(); // Reserved byte (must be 0)
            inst.fm.fb = fin.get();
            fin.get(); // Reserved byte (must be 0)
            inst.fm.lfo = fin.get();
            fin.get(); // Reserved byte (must be 0)

            const bool totalOperatorsBool = fin.get();
            inst.fm.numOperators = totalOperatorsBool ? 4 : 2;

            inst.fm.lfo2 = fin.get();
        }

        for (int i = 0; i < inst.fm.numOperators; i++)
        {
            if (m_DMFFileVersion > 18) // Newer than DMF version 18 (0x12)
            {
                inst.fm.ops[i].am = fin.get();
                inst.fm.ops[i].ar = fin.get();
                inst.fm.ops[i].dr = fin.get();
                inst.fm.ops[i].mult = fin.get();
                inst.fm.ops[i].rr = fin.get();
                inst.fm.ops[i].sl = fin.get();
                inst.fm.ops[i].tl = fin.get();

                if (systemType == DMF::SystemType::SMS_OPLL || systemType == DMF::SystemType::NES_VRC7)
                {
                    const uint8_t opllPreset = fin.get();
                    if (i == 0)
                        inst.fm.opllPreset = opllPreset;
                    
                    inst.fm.ops[i].ksr = fin.get();
                    inst.fm.ops[i].vib = fin.get();
                    inst.fm.ops[i].ksl = fin.get();
                    inst.fm.ops[i].egs = fin.get(); // EG-S in Deflemask. 0 if OFF; 8 if ON.
                }
                else
                {
                    inst.fm.ops[i].dt2 = fin.get();
                    inst.fm.ops[i].rs = fin.get();
                    inst.fm.ops[i].dt = fin.get();
                    inst.fm.ops[i].d2r = fin.get();
                    inst.fm.ops[i].SSGMode = fin.get();
                }
            }
            else // DMF version 17 (0x11) or older
            {
                inst.fm.ops[i].am = fin.get();
                inst.fm.ops[i].ar = fin.get();
                inst.fm.ops[i].dam = fin.get();
                inst.fm.ops[i].dr = fin.get();
                inst.fm.ops[i].dvb = fin.get();
                inst.fm.ops[i].egt = fin.get();
                inst.fm.ops[i].ksl = fin.get();
                inst.fm.ops[i].mult = fin.get();
                inst.fm.ops[i].rr = fin.get();
                inst.fm.ops[i].sl = fin.get();
                inst.fm.ops[i].sus = fin.get();
                inst.fm.ops[i].tl = fin.get();
                inst.fm.ops[i].vib = fin.get();
                inst.fm.ops[i].ws = fin.get();
                inst.fm.ops[i].ksr = fin.get(); // RS on SEGA Genesis
                inst.fm.ops[i].dt = fin.get();
                inst.fm.ops[i].d2r = fin.get();
                inst.fm.ops[i].SSGMode = fin.get();
            }
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
    m_PatternValues = new ChannelRow**[m_System.channels];
    m_ChannelEffectsColumnsCount = new uint8_t[m_System.channels];
    
    uint8_t patternMatrixNumber;

    for (unsigned channel = 0; channel < m_System.channels; channel++)
    {
        m_ChannelEffectsColumnsCount[channel] = fin.get();

        m_PatternValues[channel] = new ChannelRow*[m_PatternMatrixMaxValues[channel] + 1]();

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

            m_PatternValues[channel][patternMatrixNumber] = new ChannelRow[m_ModuleInfo.totalRowsPerPattern];

            for (uint32_t row = 0; row < m_ModuleInfo.totalRowsPerPattern; row++)
            {
                m_PatternValues[channel][patternMatrixNumber][row] = LoadPatternRow(fin, m_ChannelEffectsColumnsCount[channel]);
            }
        }
    }
}

ChannelRow DMF::LoadPatternRow(zstr::ifstream& fin, int effectsColumnsCount)
{
    ChannelRow pat;
    pat.note.pitch = fin.get();
    pat.note.pitch |= fin.get() << 8; // Unused byte. Storing it anyway.
    pat.note.octave = fin.get();
    pat.note.octave |= fin.get() << 8; // Unused byte. Storing it anyway.
    pat.volume = fin.get();
    pat.volume |= fin.get() << 8;

    // Apparently, the note pitch for C- can be either 0 or 12. I'm setting it to 0 always.
    if (pat.note.pitch == NotePitch::C_Alt)
    {
        pat.note.pitch = (uint16_t)NotePitch::C;
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
        pat.effect[col] = {(int16_t)EffectCode::NoEffect, (int16_t)EffectCode::NoEffectVal};
    }

    pat.instrument = fin.get();
    pat.instrument |= fin.get() << 8;

    return pat;
}

void DMF::LoadPCMSamplesData(zstr::ifstream& fin)
{
    m_TotalPCMSamples = fin.get();
    m_PCMSamples = new PCMSample[m_TotalPCMSamples];

    for (unsigned sample = 0; sample < m_TotalPCMSamples; sample++)
    {
        m_PCMSamples[sample] = LoadPCMSample(fin);
    }
}

PCMSample DMF::LoadPCMSample(zstr::ifstream& fin)
{
    PCMSample sample;

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
    numerator = 15 * globalTick;
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

int dmf::GetNoteRange(const Note& low, const Note& high)
{
    // Returns range in semitones. Assumes notes have octave and pitch.
    // Range is inclusive on both ends.

    return (high.octave - low.octave) * 12 + (high.pitch - low.pitch) + 1;
}

bool Note::operator==(const Note& rhs) const
{
    return this->octave == rhs.octave && this->pitch == rhs.pitch;
}

bool Note::operator!=(const Note& rhs) const
{
    return this->octave != rhs.octave || this->pitch != rhs.pitch;
}

// The following operators assume notes aren't Note OFF or Empty note

bool Note::operator>(const Note& rhs) const
{
    return (this->octave << 4) + this->pitch > (rhs.octave << 4) + rhs.pitch;
}

bool Note::operator<(const Note& rhs) const
{
    return (this->octave << 4) + this->pitch < (rhs.octave << 4) + rhs.pitch;
}

bool Note::operator>=(const Note& rhs) const
{
    return (this->octave << 4) + this->pitch >= (rhs.octave << 4) + rhs.pitch;
}

bool Note::operator<=(const Note& rhs) const
{
    return (this->octave << 4) + this->pitch <= (rhs.octave << 4) + rhs.pitch;
}
