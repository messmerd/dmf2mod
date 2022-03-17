/*
    dmf.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares a ModuleInterface-derived class for Deflemask's 
    DMF files.
*/

#pragma once

#include "core/module.h"
#include <zstr/zstr.hpp>

#include <string>
#include <map>

namespace d2m {

// Declare module
MODULE_DECLARE(DMF, DMFConversionOptions)

namespace dmf {

// Deflemask allows four effects columns per channel regardless of the system 
#define DMF_MAX_EFFECTS_COLUMN_COUNT 4

enum class NotePitch
{
    Empty=0,
    C=0,
    CS=1,
    D=2,
    DS=3,
    E=4,
    F=5,
    FS=6,
    G=7,
    GS=8,
    A=9,
    AS=10,
    B=11,
    C_Alt=12,
    Off=100
};

static const int DMFNoInstrument = -1;
static const int DMFNoVolume = -1;
static const int DMFVolumeMax = 15; /* ??? */

// Deflemask effects shared by all systems:
enum class EffectCode
{
    NoEffect=-1, NoEffectVal=-1,
    Arp=0x0, PortUp=0x1, PortDown=0x2, Port2Note=0x3, Vibrato=0x4, Port2NoteVolSlide=0x5, VibratoVolSlide=0x6,
    Tremolo=0x7, Panning=0x8, SetSpeedVal1=0x9, VolSlide=0xA, PosJump=0xB, Retrig=0xC, PatBreak=0xD,
    ArpTickSpeed=0xE0, NoteSlideUp=0xE1, NoteSlideDown=0xE2, SetVibratoMode=0xE3, SetFineVibratoDepth=0xE4,
    SetFinetune=0xE5, SetSamplesBank=0xEB, NoteCut=0xEC, NoteDelay=0xED, SyncSignal=0xEE, SetGlobalFinetune=0xEF,
    SetSpeedVal2=0xF
};

// Deflemask effects exclusive to the Game Boy system:
enum class GameBoyEffectCode
{
    SetWave=0x10,
    SetNoisePolyCounterMode=0x11,
    SetDutyCycle=0x12,
    SetSweepTimeShift=0x13,
    SetSweepDir=0x14
};

// To do: Add enums for effects exclusive to the rest of Deflemask's systems.

struct Note
{
    uint16_t pitch;
    uint16_t octave;

    Note() = default;
    Note(uint16_t p, uint16_t o)
        : pitch(p), octave(o)
    {}

    Note(NotePitch p, uint16_t o)
        : pitch((uint16_t)p), octave(o)
    {}

    inline bool HasPitch() const
    {
        if (pitch == 0 && octave == 0)
            return false; // Empty note
        return pitch >= 0 && pitch <= 12;
        // Contrary to specs, pitch == 0 means C-. I'm not sure if pitch == 12 is also used for C-.
    }

    inline bool IsOff() const
    {
        return pitch == (int)NotePitch::Off;
    }

    inline bool IsEmpty() const
    {
        return pitch == 0 && octave == 0;
    }

    bool operator==(const Note& rhs) const;
    bool operator!=(const Note& rhs) const;
    bool operator>(const Note& rhs) const;
    bool operator<(const Note& rhs) const;
    bool operator>=(const Note& rhs) const;
    bool operator<=(const Note& rhs) const;
};

int GetNoteRange(const Note& low, const Note& high);

// Comparison operators for enums
template <typename T, typename U,
    class = typename std::enable_if<std::is_enum<U>{} &&
    std::is_same<std::underlying_type_t<U>, int>{} &&
    std::is_integral<T>{}>>
bool operator==(T lhs, const U& rhs)
{
    return lhs == static_cast<T>(rhs);
}

template <typename T, typename U,
    class = typename std::enable_if<std::is_enum<U>{} &&
    std::is_same<std::underlying_type_t<U>, int>{} &&
    std::is_integral<T>{}>>
bool operator!=(T lhs, const U& rhs)
{
    return lhs != static_cast<T>(rhs);
}

struct System
{
    enum class Type
    {
        Error=0, YMU759, Genesis, Genesis_CH3, SMS, GameBoy,
        PCEngine, NES, C64_SID_8580, C64_SID_6581, Arcade,
        NeoGeo, NeoGeo_CH2, SMS_OPLL, NES_VRC7
    };

    Type type;
    uint8_t id;
    std::string name;
    uint8_t channels;

    System() = default;
    System(Type type, uint8_t id, std::string name, uint8_t channels)
        : type(type), id(id), name(name), channels(channels)
    {}
};
struct VisualInfo
{
    uint8_t songNameLength;
    std::string songName;
    uint8_t songAuthorLength;
    std::string songAuthor;
    uint8_t highlightAPatterns;
    uint8_t highlightBPatterns;
};

struct ModuleInfo
{
    uint8_t timeBase, tickTime1, tickTime2, framesMode, usingCustomHZ, customHZValue1, customHZValue2, customHZValue3;
    uint32_t totalRowsPerPattern;
    uint8_t totalRowsInPatternMatrix;
};

struct FMOps
{
    // TODO: Use unions depending on DMF version?
    uint8_t am;
    uint8_t ar;     // Attack
    uint8_t dr;     // Delay?
    uint8_t mult;
    uint8_t rr;     // Release
    uint8_t sl;     // Sustain
    uint8_t tl;

    uint8_t dt2;
    uint8_t rs;
    uint8_t dt;
    uint8_t d2r;
    
    union
    {
        uint8_t SSGMode;
        uint8_t egs; // EG-S in SMS OPLL / NES VRC7. 0 if OFF; 8 if ON.
    };
    
    uint8_t dam, dvb, egt, ksl, sus, vib, ws, ksr; // Exclusive to DMF version 18 (0x12) and older
};

struct Instrument
{
    enum InstrumentMode
    {
        InvalidMode=0,
        StandardMode,
        FMMode
    };

    std::string name;
    InstrumentMode mode; // TODO: Use union depending on mode? Would save space

    union
    {
        // Standard Instruments
        struct
        {
            uint8_t volEnvSize, arpEnvSize, dutyNoiseEnvSize, wavetableEnvSize;
            int32_t *volEnvValue, *arpEnvValue, *dutyNoiseEnvValue, *wavetableEnvValue;
            int8_t volEnvLoopPos, arpEnvLoopPos, dutyNoiseEnvLoopPos, wavetableEnvLoopPos;
            uint8_t arpMacroMode;

            // Commodore 64 exclusive
            uint8_t c64TriWaveEn, c64SawWaveEn, c64PulseWaveEn, c64NoiseWaveEn,
                c64Attack, c64Decay, c64Sustain, c64Release, c64PulseWidth, c64RingModEn,
                c64SyncModEn, c64ToFilter, c64VolMacroToFilterCutoffEn, c64UseFilterValuesFromInst;
            uint8_t c64FilterResonance, c64FilterCutoff, c64FilterHighPass, c64FilterLowPass, c64FilterCH2Off;

            // Game Boy exclusive
            uint8_t gbEnvVol, gbEnvDir, gbEnvLen, gbSoundLen;
        } std;

        // FM Instruments
        struct
        {
            uint8_t numOperators;
            
            union
            {
                uint8_t alg;
                uint8_t sus; // SMS OPLL / NES VRC7 exclusive
            };
            
            uint8_t fb;
            uint8_t opllPreset; // SMS OPLL / NES VRC7 exclusive

            union {
                struct {
                    uint8_t lfo, lfo2;
                };
                struct {
                    uint8_t dc, dm; // SMS OPLL / NES VRC7 exclusive
                };
            };
            
            FMOps ops[4];
        } fm;
    };
};

struct PCMSample
{
    uint32_t size;
    std::string name;
    uint8_t rate, pitch, amp, bits;
    uint16_t *data;
};

struct Effect
{
    int16_t code;
    int16_t value;
};

struct ChannelRow
{
    Note note;
    int16_t volume;
    Effect effect[DMF_MAX_EFFECTS_COLUMN_COUNT];
    int16_t instrument;
};

// Deflemask Game Boy channels
enum class GameBoyChannel
{
    SQW1=0, SQW2=1, WAVE=2, NOISE=3
};


} // namespace dmf

class DMFConversionOptions : public ConversionOptionsInterface<DMFConversionOptions>
{
public:
    DMFConversionOptions() = default;
    ~DMFConversionOptions() {}
};

class DMF : public ModuleInterface<DMF, DMFConversionOptions>
{
public:
    enum ImportError
    {
        Success=0,
        UnspecifiedError
    };

    enum class ImportWarning {};
    enum class ExportError {};
    enum class ExportWarning {};
    enum class ConvertError {};
    enum class ConvertWarning {};

    using SystemType = dmf::System::Type;

    static const dmf::System& Systems(SystemType systemType);

public:
    DMF();
    ~DMF();
    void CleanUp();

    std::string GetName() const override { return m_VisualInfo.songName; }

    ////////////

    // Returns the initial BPM of the module
    void GetBPM(unsigned& numerator, unsigned& denominator) const;
    double GetBPM() const;

    const dmf::System& GetSystem() const { return m_System; }
    const dmf::VisualInfo& GetVisualInfo() const { return m_VisualInfo; }
    const dmf::ModuleInfo& GetModuleInfo() const { return m_ModuleInfo; }

    uint8_t** GetPatternMatrixValues() const { return m_PatternMatrixValues; }

    uint8_t GetTotalWavetables() const { return m_TotalWavetables; }

    uint32_t** GetWavetableValues() const { return m_WavetableValues; }
    uint32_t GetWavetableValue(unsigned wavetable, unsigned index) const { return m_WavetableValues[wavetable][index]; }

    dmf::ChannelRow*** GetPatternValues() const { return m_PatternValues; }
    dmf::ChannelRow GetChannelRow(unsigned channel, unsigned patternMatrixRow, unsigned patternRow) const
    {
        return m_PatternValues[channel][m_PatternMatrixValues[channel][patternMatrixRow]][patternRow];
    }

    std::string GetPatternName(unsigned channel, unsigned patternMatrixRow) const
    {
        return m_PatternNames.at((patternMatrixRow * m_System.channels) + channel);
    }

private:
    void ImportRaw(const std::string& filename) override;
    void ExportRaw(const std::string& filename) override;
    void ConvertRaw(const Module* input) override;

    dmf::System GetSystem(uint8_t systemByte) const;
    void LoadVisualInfo(zstr::ifstream& fin);
    void LoadModuleInfo(zstr::ifstream& fin);
    void LoadPatternMatrixValues(zstr::ifstream& fin);
    void LoadInstrumentsData(zstr::ifstream& fin);
    dmf::Instrument LoadInstrument(zstr::ifstream& fin, SystemType systemType);
    void LoadWavetablesData(zstr::ifstream& fin);
    void LoadPatternsData(zstr::ifstream& fin);
    dmf::ChannelRow LoadPatternRow(zstr::ifstream& fin, int effectsColumnsCount);
    void LoadPCMSamplesData(zstr::ifstream& fin);
    dmf::PCMSample LoadPCMSample(zstr::ifstream& fin);

private:
    uint8_t         m_DMFFileVersion;
    dmf::System          m_System;
    dmf::VisualInfo      m_VisualInfo;
    dmf::ModuleInfo      m_ModuleInfo;
    uint8_t**       m_PatternMatrixValues;
    uint8_t*        m_PatternMatrixMaxValues;
    uint8_t         m_TotalInstruments;
    dmf::Instrument*     m_Instruments;
    uint8_t         m_TotalWavetables;
    uint32_t*       m_WavetableSizes;
    uint32_t**      m_WavetableValues;
    dmf::ChannelRow***   m_PatternValues;
    uint8_t*        m_ChannelEffectsColumnsCount;
    uint8_t         m_TotalPCMSamples;
    dmf::PCMSample*      m_PCMSamples;
    std::map<unsigned, std::string> m_PatternNames;
};

} // namespace d2m
