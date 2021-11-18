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

// Finish setup
const std::vector<std::string> DMFOptions = {};
REGISTER_MODULE_CPP(DMF, DMFConversionOptions, ModuleType::DMF, "dmf", DMFOptions)

#define DMF_FILE_VERSION 22 // 0x16 - Only DefleMask v0.12.0 files and above are supported

// Information about all the systems Deflemask supports
// Using designated initialization for the array
const System DMF::m_Systems[] = {
    [SYS_ERROR] = {.id = 0x00, .name = "ERROR", .channels = 0},
    [SYS_GENESIS] = {.id = 0x02, .name = "GENESIS", .channels = 10},
    [SYS_GENESIS_CH3] = {.id = 0x12, .name = "GENESIS_CH3", .channels = 13},
    [SYS_SMS] = {.id = 0x03, .name = "SMS", .channels = 4},
    [SYS_GAMEBOY] = {.id = 0x04, .name = "GAMEBOY", .channels = 4},
    [SYS_PCENGINE] = {.id = 0x05, .name = "PCENGINE", .channels = 6},
    [SYS_NES] = {.id = 0x06, .name = "NES", .channels = 5},
    [SYS_C64_SID_8580] = {.id = 0x07, .name = "C64_SID_8580", .channels = 3},
    [SYS_C64_SID_6581] = {.id = 0x17, .name = "C64_SID_6581", .channels = 3},
    [SYS_ARCADE] = {.id = 0x08, .name = "ARCADE", .channels = 13},
    [SYS_NEOGEO] = {.id = 0x09, .name = "NEOGEO", .channels = 13},
    [SYS_NEOGEO_CH2] = {.id = 0x49, .name = "NEOGEO_CH2", .channels = 16}
};

DMF::DMF()
{
    // Initialize pointers to nullptr to prevent segfault when freeing memory if the import fails:
    m_VisualInfo.songName = nullptr;
    m_VisualInfo.songAuthor = nullptr;
    m_PatternValues = nullptr;
    m_ChannelEffectsColumnsCount = nullptr;
    m_PatternMatrixValues = nullptr;
    m_PatternMatrixMaxValues = nullptr;
    m_Instruments = nullptr;
    m_WavetableSizes = nullptr;
    m_WavetableValues = nullptr;
    m_ChannelEffectsColumnsCount = nullptr;
    m_PCMSamples = nullptr;
}

DMF::~DMF()
{
    CleanUp();
}

void DMF::CleanUp()
{
    // Free memory allocated for members
    delete[] m_VisualInfo.songName;
    m_VisualInfo.songName = nullptr;

    delete[] m_VisualInfo.songAuthor;
    m_VisualInfo.songAuthor = nullptr;

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
            delete[] m_Instruments[i].name;
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
            delete[] m_PCMSamples[sample].name;
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

bool DMF::Import(const std::string& filename)
{
    CleanUp();
    m_Status.Clear();

    const bool silent = ModuleUtils::GetCoreOptions().silent;

    if (!silent)
        std::cout << "Starting to import the DMF file...\n";

    if (ModuleUtils::GetTypeFromFilename(filename) != ModuleType::DMF)
    {
        m_Status.SetError(Status::Category::Import, DMF::ImportError::UnspecifiedError, "Input file has the wrong file extension.\nPlease use a DMF file.");
        return true;
    }

    if (!silent)
        std::cout << "DMF Filename: " << filename << "\n";

    zstr::ifstream fin(filename, std::ios_base::binary);
    if (fin.fail())
    {
        m_Status.SetError(Status::Category::Import, DMF::ImportError::UnspecifiedError, "Failed to open DMF file.");
        return true;
    }

    ///////////////// FORMAT FLAGS
    
    char header[17];
    fin.read(header, 16);
    header[16] = '\0';
    
    if (std::string(header) != ".DelekDefleMask.")
    {
        m_Status.SetError(Status::Category::Import, DMF::ImportError::UnspecifiedError, "DMF format header is bad.");
        return true;
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
        
        m_Status.SetError(Status::Category::Import, DMF::ImportError::UnspecifiedError, errorMsg);
        return true;
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

    return false;
}

bool DMF::Export(const std::string& filename)
{
    // Not implemented
    return false;
}

System DMF::GetSystem(uint8_t systemByte)
{
    const size_t size = sizeof(m_Systems) / sizeof(m_Systems[0]);
    for (unsigned i = 1; i < size; i++)
    {
        if (m_Systems[i].id == systemByte)
            return m_Systems[i];
    }
    return m_Systems[SYS_ERROR]; // Error: System byte invalid
}

void DMF::LoadVisualInfo(zstr::ifstream& fin)
{
    m_VisualInfo.songNameLength = fin.get();
    m_VisualInfo.songName = new char[m_VisualInfo.songNameLength + 1];
    fin.read(m_VisualInfo.songName, m_VisualInfo.songNameLength);
    m_VisualInfo.songName[m_VisualInfo.songNameLength] = '\0';

    m_VisualInfo.songAuthorLength = fin.get();
    m_VisualInfo.songAuthor = new char[m_VisualInfo.songAuthorLength + 1];
    fin.read(m_VisualInfo.songAuthor, m_VisualInfo.songAuthorLength);
    m_VisualInfo.songAuthor[m_VisualInfo.songAuthorLength] = '\0';

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

    if (m_DMFFileVersion >= 24) // For Version 0.12.0 and onward. DMF version 24 (0x18). TODO: What is 23?
    {
        // Newer versions read 4 bytes here
        m_ModuleInfo.totalRowsPerPattern = fin.get();
        m_ModuleInfo.totalRowsPerPattern |= fin.get() << 8;
        m_ModuleInfo.totalRowsPerPattern |= fin.get() << 16;
        m_ModuleInfo.totalRowsPerPattern |= fin.get() << 24;
    }
    else // Version 0.12.0. DMF version 22 (0x16).
    {
        // Earlier versions such as 22 (0x16) only read one byte here
        m_ModuleInfo.totalRowsPerPattern = fin.get();
    }

    m_ModuleInfo.totalRowsInPatternMatrix = fin.get();

    // TODO: Prior to Version 0.11.1, arpeggio tick speed was stored here
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
    m_Instruments = new Instrument[m_TotalInstruments];

    for (int i = 0; i < m_TotalInstruments; i++)
    {
        m_Instruments[i] = LoadInstrument(fin, m_System);
    }
}

Instrument DMF::LoadInstrument(zstr::ifstream& fin, System systemType)
{
    Instrument inst;

    uint8_t name_size = fin.get();
    inst.name = new char[name_size + 1];
    fin.read(inst.name, name_size);
    inst.name[name_size] = '\0';

    inst.mode = fin.get(); // 1 = FM; 0 = Standard

    // Initialize to nullptr to prevent issues if they are freed later without ever dynamically allocating memory
    inst.stdVolEnvValue = nullptr;
    inst.stdArpEnvValue = nullptr;
    inst.stdDutyNoiseEnvValue = nullptr;
    inst.stdWavetableEnvValue = nullptr;

    if (inst.mode == 1) // FM instrument
    {
        inst.fmALG = fin.get();
        inst.fmFB = fin.get();
        inst.fmLFO = fin.get();
        inst.fmLFO2 = fin.get();

        constexpr int TOTAL_OPERATORS = 4;
        for (int i = 0; i < TOTAL_OPERATORS; i++)
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
    }
    else if (inst.mode == 0) // Standard instrument
    {
        if (systemType.id != m_Systems[SYS_GAMEBOY].id)  // Not a Game Boy
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

        if (inst.stdArpEnvSize > 0)
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

        if (inst.stdDutyNoiseEnvSize > 0)
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

        if (inst.stdWavetableEnvSize > 0)
            inst.stdWavetableEnvLoopPos = fin.get();

        // Per system data
        if (systemType.id == m_Systems[SYS_C64_SID_8580].id || systemType.id == m_Systems[SYS_C64_SID_6581].id) // Using Commodore 64
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
        else if (systemType.id == m_Systems[SYS_GAMEBOY].id) // Using Game Boy
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
    m_PatternValues = new PatternRow**[m_System.channels];
    m_ChannelEffectsColumnsCount = new uint8_t[m_System.channels];
    
    uint8_t patternMatrixNumber;

    for (unsigned channel = 0; channel < m_System.channels; channel++)
    {
        m_ChannelEffectsColumnsCount[channel] = fin.get();

        m_PatternValues[channel] = new PatternRow*[m_PatternMatrixMaxValues[channel] + 1]();

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

            m_PatternValues[channel][patternMatrixNumber] = new PatternRow[m_ModuleInfo.totalRowsPerPattern];

            for (uint32_t row = 0; row < m_ModuleInfo.totalRowsPerPattern; row++)
            {
                m_PatternValues[channel][patternMatrixNumber][row] = LoadPatternRow(fin, m_ChannelEffectsColumnsCount[channel]);
            }
        }
    }
}

PatternRow DMF::LoadPatternRow(zstr::ifstream& fin, int effectsColumnsCount)
{
    PatternRow pat;
    pat.note.pitch = fin.get();
    pat.note.pitch |= fin.get() << 8; // Unused byte. Storing it anyway.
    pat.note.octave = fin.get();
    pat.note.octave |= fin.get() << 8; // Unused byte. Storing it anyway.
    pat.volume = fin.get();
    pat.volume |= fin.get() << 8;

    // NOTE: C# is considered the 1st note of an octave rather than C- like in the Deflemask program.

    if (pat.note.pitch == 0 && pat.note.octave == 0)
    {
        pat.note.pitch = DMF_NOTE_EMPTY;
    }

    for (int col = 0; col < effectsColumnsCount; col++)
    {
        pat.effectCode[col] = fin.get();
        pat.effectCode[col] |= fin.get() << 8;
        pat.effectValue[col] = fin.get();
        pat.effectValue[col] |= fin.get() << 8;
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

    if (m_DMFFileVersion >= 24) // For Version 0.12.0 and onward. DMF version 24 (0x18). TODO: What is 23?
    {
        // Read PCM sample name
        uint8_t name_size = fin.get();
        sample.name = new char[name_size];
        fin.read(sample.name, name_size);
        sample.name[name_size] = '\0';
    }
    else // Version 0.12.0. DMF version 22 (0x16).
    {
        // PCM samples don't have names in this DMF version
        sample.name = nullptr;
    }

    sample.rate = fin.get();
    sample.pitch = fin.get();
    sample.amp = fin.get();
    sample.bits = fin.get();

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

bool operator==(const Note& lhs, const Note& rhs)
{
    return lhs.octave == rhs.octave && lhs.pitch == rhs.pitch;
}

bool operator!=(const Note& lhs, const Note& rhs)
{
    return lhs.octave != rhs.octave || lhs.pitch != rhs.pitch;
}

// For the following operators:
// Assumes note isn't Note OFF or Empty note
// Notes must use the DMF convention where the note C# is the 1st note of an octave rather than C-

bool operator>(const Note& lhs, const Note& rhs)
{
    return lhs.octave + lhs.pitch / 13.f > rhs.octave + rhs.pitch / 13.f;
}

bool operator<(const Note& lhs, const Note& rhs)
{
    return lhs.octave + lhs.pitch / 13.f < rhs.octave + rhs.pitch / 13.f;
}

bool operator>=(const Note& lhs, const Note& rhs)
{
    return lhs.octave + lhs.pitch / 13.f >= rhs.octave + rhs.pitch / 13.f;
}

bool operator<=(const Note& lhs, const Note& rhs)
{
    return lhs.octave + lhs.pitch / 13.f <= rhs.octave + rhs.pitch / 13.f;
}
