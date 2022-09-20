/*
    effects.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines effects which are common to many module file types
*/

#pragma once

#include <cstdint>

namespace d2m {

// All common effects are less than 0
enum class CommonEffects : int8_t
{
    NoEffect            =0,
    Arp                 =-1,
    PortUp              =-2,
    PortDown            =-3,
    Port2Note           =-4,
    Vibrato             =-5,
    Port2NoteVolSlide   =-6,
    VibratoVolSlide     =-7,
    Tremolo             =-8,
    Panning             =-9,
    VolSlide            =-10,
    PosJump             =-11,
    Retrigger           =-12,
    PatBreak            =-13,
    NoteCut             =-14,
    NoteDelay           =-15,
    Speed               =-16,
    Tempo               =-17,
};

// Modules should implement effects specific to them using positive values starting with 1.

} // namespace d2m
