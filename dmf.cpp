/*
dmf.cpp
Written by Dalton Messmer <messmer.dalton@gmail.com>.

Provides functions for loading a dmf file according to the 
spec sheet at http://www.deflemask.com/DMF_SPECS.txt.

Requires the zlib compression library from https://zlib.net.
*/

#include "dmf.h"

REGISTER_MODULE(DMF, DMFConversionOptions, ModuleType::DMF, "dmf")

// For inflating .dmf files so that they can be read 
#include "zlib.h"
#include "zconf.h"

#include <iostream>
#include <string>
#include <fstream>

#define DMF_FILE_VERSION 24 // 0x18 - Only DefleMask v0.12.0 files are supported

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
	[SYS_YM2151] = {.id = 0x08, .name = "YM2151", .channels = 13}
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

    m_ImportError = IMPORT_ERROR_FAIL;
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
    std::cout << "DMF files have no conversion options." << std::endl;
}

bool DMF::Load(const std::string& filename)
{
    CleanUp();
    m_ImportError = IMPORT_ERROR_FAIL;

    std::cout << "Starting to import the DMF file..." << std::endl;

    if (ModuleUtils::GetTypeFromFilename(filename) != ModuleType::DMF)
    {
        std::cout << "ERROR: Input file has the wrong file extension." << std::endl << "Please use a DMF file." << std::endl;
        m_ImportError = IMPORT_ERROR_FAIL;
        return true;
    }

    std::cout << "DMF Filename: " << filename << std::endl;

    zstr::ifstream fin(filename, std::ios_base::binary);
    if (fin.fail())
    {
        std::cout << "ERROR: Failed to open DMF file." << std::endl;
        m_ImportError = IMPORT_ERROR_FAIL;
        return true;
    }

    ///////////////// FORMAT FLAGS
    
    char header[17];
    fin.read(header, 16);
    header[16] = '\0';
    
    if (std::string(header) != ".DelekDefleMask.")
    {
        std::cout << "ERROR: DMF format header is bad.\n" << std::endl;
        m_ImportError = IMPORT_ERROR_FAIL;
        return true;
    }

    m_DMFFileVersion = fin.get();
    if (m_DMFFileVersion != DMF_FILE_VERSION)
    {
        std::cout << "ERROR: Deflemask file version must be " << (int)DMF_FILE_VERSION << " (0x" << std::ios_base::hex << (int)DMF_FILE_VERSION << ")."
            << "The given DMF file is version " << (int)DMF_FILE_VERSION << " (0x" << std::ios_base::hex << (int)DMF_FILE_VERSION << ")." << std::endl;
            std::cout << "       You can convert older DMF files to the correct version by opening them in DefleMask v0.12.0 and then saving them." << std::endl;
        m_ImportError = IMPORT_ERROR_FAIL;
        return true;
    }

    ///////////////// SYSTEM SET
    m_System = GetSystem(fin.get());
    std::cout << "System: " << m_System.name << " (channels: " << std::to_string(m_System.channels) << ")" << std::endl;

    ///////////////// VISUAL INFORMATION
    LoadVisualInfo(fin);
    std::cout << "Loaded visual information." << std::endl;

    ///////////////// MODULE INFORMATION
    LoadModuleInfo(fin);
    std::cout << "Loaded module." << std::endl;

    ///////////////// PATTERN MATRIX VALUES
    LoadPatternMatrixValues(fin);
    std::cout << "Loaded pattern matrix values." << std::endl;

    ///////////////// INSTRUMENTS DATA
    LoadInstrumentsData(fin);
    std::cout << "Loaded instruments." << std::endl;

    ///////////////// WAVETABLES DATA
    LoadWavetablesData(fin);
    std::cout << "Loaded " << std::to_string(m_TotalWavetables) << " wavetable(s)." << std::endl;

    ///////////////// PATTERNS DATA
    LoadPatternsData(fin);
    std::cout << "Loaded patterns." << std::endl;

    ///////////////// PCM SAMPLES DATA
    LoadPCMSamplesData(fin);
    std::cout << "Loaded PCM Samples." << std::endl;

    std::cout << "Done loading DMF file!" << std::endl << std::endl;

    m_ImportError = IMPORT_ERROR_SUCCESS;
    return false;
}

bool DMF::Save(const std::string& filename)
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

    std::cout << "Title: " << m_VisualInfo.songName << std::endl;

    m_VisualInfo.songAuthorLength = fin.get();
    m_VisualInfo.songAuthor = new char[m_VisualInfo.songAuthorLength + 1];
    fin.read(m_VisualInfo.songAuthor, m_VisualInfo.songAuthorLength);
    m_VisualInfo.songAuthor[m_VisualInfo.songAuthorLength] = '\0';

    std::cout << "Author: " << m_VisualInfo.songAuthor << std::endl;

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
    m_ModuleInfo.totalRowsPerPattern = fin.get();
    m_ModuleInfo.totalRowsPerPattern |= fin.get() << 8;
    m_ModuleInfo.totalRowsPerPattern |= fin.get() << 16;
    m_ModuleInfo.totalRowsPerPattern |= fin.get() << 24;
    m_ModuleInfo.totalRowsInPatternMatrix = fin.get();

    // TODO: In previous DMF versions, arpeggio tick speed is stored here!!!
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
        /*
        for (unsigned i = 0; i < m_PatternMatrixMaxValues[channel] + 1u; i++)
        {
            m_PatternValues[channel][i] = nullptr;
        }*/
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

    uint8_t name_size = fin.get();
    sample.name = new char[name_size];
    fin.read(sample.name, name_size);
    sample.name[name_size] = '\0';

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

double DMF::GetBPM() const
{
    // Returns the initial BPM of the module
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
    return (15.0 * globalTick) / ((m_ModuleInfo.timeBase + 1) * (m_ModuleInfo.tickTime1 + m_ModuleInfo.tickTime2));
}

int8_t NoteCompare(const Note *n1, const Note *n2)
{
    // Compares notes n1 and n2
    // Assumes note isn't Note OFF or Empty note
    // Notes must use the DMF convention where the note C# is the 1st note of an octave rather than C-

    if (n1->octave + n1->pitch / 13.f > n2->octave + n2->pitch / 13.f)
    {
        return 1; // n1 > n2 (n1 has a higher pitch than n2)
    }
    else if (n1->octave + n1->pitch / 13.f < n2->octave + n2->pitch / 13.f)
    {
        return -1; // n1 < n2 (n1 has a lower pitch than n2)
    }
    else 
    {
        return 0; // Same note
    }
}

int8_t NoteCompare(const Note* n1, const Note n2)
{
    return NoteCompare(n1, &n2);
}

