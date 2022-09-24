/*
    effects.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines effects which are common to many module file types
*/

#pragma once

#include <cstdint>

namespace d2m {

using EffectCode = int8_t;
using EffectValue = int16_t;

struct Effect
{
    EffectCode code;
    EffectValue value;
};

inline constexpr bool operator==(const Effect& lhs, const Effect& rhs) { return lhs.code == rhs.code && lhs.value == rhs.value; }

// Defines common effects used by multiple module types
namespace Effects
{
    enum
    {
        // All common effects are less than 0
        kNoEffect            =0,
        kArp                 =-1,
        kPortUp              =-2,
        kPortDown            =-3,
        kPort2Note           =-4,
        kVibrato             =-5,
        kPort2NoteVolSlide   =-6,
        kVibratoVolSlide     =-7,
        kTremolo             =-8,
        kPanning             =-9,
        kSpeedA              =-10,
        kVolSlide            =-11,
        kPosJump             =-12,
        kRetrigger           =-13,
        kPatBreak            =-14,
        kNoteCut             =-15,
        kNoteDelay           =-16,
        kTempo               =-17,
        kSpeedB              =-18,
    };
    // Modules should implement effects specific to them using positive values starting with 1.
}

} // namespace d2m
