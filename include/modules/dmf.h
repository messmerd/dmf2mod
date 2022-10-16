/*
    dmf.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Declares all classes used for Deflemask's DMF files.
*/

#pragma once

#include "core/module.h"
#include "utils/stream_reader.h"
#include <zstr/zstr.hpp>

#include <string>
#include <map>

namespace d2m {

///////////////////////////////////////////////////////////
// Setup template specializations used by DMF
///////////////////////////////////////////////////////////

class DMF;

template<>
struct ModuleGlobalData<DMF> : public ModuleGlobalDataDefault<DataStorageType::COR> {};

template<>
struct Row<DMF>
{
    NoteSlot note;
    int16_t volume;
    Effect effect[4]; // Deflemask allows four effects columns per channel regardless of the system
    int16_t instrument;
};

template<>
struct ChannelMetadata<DMF>
{
    uint8_t effectColumnsCount;
};

template<>
struct PatternMetadata<DMF>
{
    std::string name;
};

template<>
struct SoundIndex<DMF>
{
    enum Types { kNone, kSquare, kWave, kNoise /*Other options here*/ };

    using None = std::monostate;
    struct Square { uint8_t id; operator uint8_t() const { return id; } };
    struct Wave { uint8_t id; operator uint8_t() const { return id; } };
    struct Noise { uint8_t id; operator uint8_t() const { return id; } }; // Placeholder

    using type = std::variant<None, Square, Wave, Noise>;
};

inline constexpr bool operator==(const SoundIndex<DMF>::Square& lhs, const SoundIndex<DMF>::Square& rhs) { return lhs.id == rhs.id; }
inline constexpr bool operator==(const SoundIndex<DMF>::Wave& lhs, const SoundIndex<DMF>::Wave& rhs) { return lhs.id == rhs.id; }
inline constexpr bool operator==(const SoundIndex<DMF>::Noise& lhs, const SoundIndex<DMF>::Noise& rhs) { return lhs.id == rhs.id; }

///////////////////////////////////////////////////////////
// dmf namespace
///////////////////////////////////////////////////////////

namespace dmf {

inline constexpr int kDMFNoInstrument = -1;
inline constexpr int kDMFNoVolume = -1;
inline constexpr int kDMFVolumeMax = 15; /* ??? */
inline constexpr int kDMFNoEffectVal = -1;

// Effect codes used by the DMF format:
namespace EffectCode
{
    enum
    {
        kNoEffect=-1,
        kArp                    =0x0,
        kPortUp                 =0x1,
        kPortDown               =0x2,
        kPort2Note              =0x3,
        kVibrato                =0x4,
        kPort2NoteVolSlide      =0x5,
        kVibratoVolSlide        =0x6,
        kTremolo                =0x7,
        kPanning                =0x8,
        kSetSpeedVal1           =0x9,
        kVolSlide               =0xA,
        kPosJump                =0xB,
        kRetrig                 =0xC,
        kPatBreak               =0xD,
        kArpTickSpeed           =0xE0,
        kNoteSlideUp            =0xE1,
        kNoteSlideDown          =0xE2,
        kSetVibratoMode         =0xE3,
        kSetFineVibratoDepth    =0xE4,
        kSetFinetune            =0xE5,
        kSetSamplesBank         =0xEB,
        kNoteCut                =0xEC,
        kNoteDelay              =0xED,
        kSyncSignal             =0xEE,
        kSetGlobalFinetune      =0xEF,
        kSetSpeedVal2           =0xF,

        // Game Boy exclusive
        kGameBoySetWave                 =0x10,
        kGameBoySetNoisePolyCounterMode =0x11,
        kGameBoySetDutyCycle            =0x12,
        kGameBoySetSweepTimeShift       =0x13,
        kGameBoySetSweepDir             =0x14

        // TODO: Add enums for effects exclusive to the rest of Deflemask's systems.
    };
}

// Custom dmf2mod internal effect codes (see effects.h)
namespace Effects
{
    enum
    {
        kArpTickSpeed=1,
        kNoteSlideUp,
        kNoteSlideDown,
        kSetVibratoMode,
        kSetFineVibratoDepth,
        kSetFinetune,
        kSetSamplesBank,
        kSyncSignal,
        kSetGlobalFinetune,
        kGameBoySetWave,
        kGameBoySetNoisePolyCounterMode,
        kGameBoySetDutyCycle,
        kGameBoySetSweepTimeShift,
        kGameBoySetSweepDir
    };
}


struct System
{
    enum class Type
    {
        Error=0, YMU759, Genesis, Genesis_CH3, SMS, GameBoy,
        PCEngine, NES, C64_SID_8580, C64_SID_6581, Arcade,
        NeoGeo, NeoGeo_CH2, SMS_OPLL, NES_VRC7, NES_FDS
    };

    Type type;
    uint8_t id;
    std::string name;
    uint8_t channels;

    System() = default;
    System(Type type, uint8_t id, std::string name, uint8_t channels)
        : type(type), id(id), name(name), channels(channels) {}
};

struct VisualInfo
{
    uint8_t highlightAPatterns;
    uint8_t highlightBPatterns;
};

struct ModuleInfo
{
    uint8_t timeBase, tickTime1, tickTime2, framesMode, usingCustomHZ, customHZValue1, customHZValue2, customHZValue3;
    uint32_t totalRowsPerPattern; // TODO: Should remove this eventually (duplicate w/ ModuleData)
    uint8_t totalRowsInPatternMatrix; // (orders) TODO: Should remove this eventually (duplicate w/ ModuleData)
};

struct FMOps
{
    // TODO: Use unions depending on DMF version?
    uint8_t am;
    uint8_t ar;     // Attack
    uint8_t dr;     // Decay?
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

// Deflemask Game Boy channels
namespace GameBoyChannel
{
    enum
    {
        SQW1=0, SQW2=1, WAVE=2, NOISE=3
    };
}

} // namespace dmf

///////////////////////////////////////////////////////////
// DMF primary classes
///////////////////////////////////////////////////////////

class DMFConversionOptions : public ConversionOptionsInterface<DMFConversionOptions>
{
private:

    // Only allow the Factory to construct this class
    friend class Builder<DMFConversionOptions, ConversionOptionsBase>;

    DMFConversionOptions() = default;

public:

    // Factory requires destructor to be public
    ~DMFConversionOptions() = default;
};

class DMF : public ModuleInterface<DMF>
{
public:

    using options_t = DMFConversionOptions;

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

private:

    // Only allow the Factory to construct this class
    friend class Builder<DMF, ModuleBase>;

    DMF();
    void CleanUp();

public:

    // Factory requires destructor to be public
    ~DMF();

    // Returns the initial BPM of the module
    void GetBPM(unsigned& numerator, unsigned& denominator) const;
    double GetBPM() const;

    const dmf::System& GetSystem() const { return m_System; }

    uint8_t GetTotalWavetables() const { return m_TotalWavetables; }
    uint32_t** GetWavetableValues() const { return m_WavetableValues; }
    uint32_t GetWavetableValue(unsigned wavetable, unsigned index) const { return m_WavetableValues[wavetable][index]; }

private:

    void ImportImpl(const std::string& filename) override;
    void ExportImpl(const std::string& filename) override;
    void ConvertImpl(const ModulePtr& input) override;
    size_t GenerateDataImpl(size_t data_flags) const override;

    using Reader = StreamReader<zstr::ifstream, Endianness::Little>;

    dmf::System GetSystem(uint8_t systemByte) const;
    void LoadVisualInfo(Reader& fin);
    void LoadModuleInfo(Reader& fin);
    void LoadPatternMatrixValues(Reader& fin);
    void LoadInstrumentsData(Reader& fin);
    dmf::Instrument LoadInstrument(Reader& fin, SystemType systemType);
    void LoadWavetablesData(Reader& fin);
    void LoadPatternsData(Reader& fin);
    Row<DMF> LoadPatternRow(Reader& fin, uint8_t effectsColumnsCount);
    void LoadPCMSamplesData(Reader& fin);
    dmf::PCMSample LoadPCMSample(Reader& fin);

private:
    uint8_t         m_DMFFileVersion;
    dmf::System          m_System;
    dmf::VisualInfo      m_VisualInfo;
    dmf::ModuleInfo      m_ModuleInfo;
    uint8_t         m_TotalInstruments;
    dmf::Instrument*     m_Instruments;
    uint8_t         m_TotalWavetables;
    uint32_t*       m_WavetableSizes;
    uint32_t**      m_WavetableValues;
    uint8_t         m_TotalPCMSamples;
    dmf::PCMSample*      m_PCMSamples;
};

} // namespace d2m
