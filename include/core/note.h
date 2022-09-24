/*
    note.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines a data structure for storing notes + helper functions.
*/

#pragma once

#include <variant>
#include <cstdint>
#include <cassert>

namespace d2m {

enum class NotePitch : uint8_t
{
    C=0,
    CS,
    D,
    DS,
    E,
    F,
    FS,
    G,
    GS,
    A,
    AS,
    B
};

namespace NoteTypes
{
    enum { EmptyType, NoteType, OffType }; // The order is important

    struct Empty {};
    inline constexpr bool operator==(const Empty&, const Empty&) { return true; };

    struct alignas(1) Note
    {
        NotePitch pitch : 4;
        uint8_t octave : 4;

        constexpr Note() : pitch(NotePitch::C), octave(0) {}
        constexpr Note(NotePitch pitch, uint8_t octave) : pitch(pitch), octave(octave) {}

        bool operator>(Note rhs) const
        {
            return (this->octave << 4) + static_cast<uint8_t>(this->pitch) > (rhs.octave << 4) + static_cast<uint8_t>(rhs.pitch);
        }

        bool operator>=(Note rhs) const
        {
            return (this->octave << 4) + static_cast<uint8_t>(this->pitch) >= (rhs.octave << 4) + static_cast<uint8_t>(rhs.pitch);
        }

        bool operator<(Note rhs) const
        {
            return (this->octave << 4) + static_cast<uint8_t>(this->pitch) < (rhs.octave << 4) + static_cast<uint8_t>(rhs.pitch);
        }

        bool operator<=(Note rhs) const
        {
            return (this->octave << 4) + static_cast<uint8_t>(this->pitch) <= (rhs.octave << 4) + static_cast<uint8_t>(rhs.pitch);
        }

        bool operator==(Note rhs) const
        {
            return this->octave == rhs.octave && this->pitch == rhs.pitch;
        }

        bool operator!=(Note rhs) const
        {
            return this->octave != rhs.octave || this->pitch != rhs.pitch;
        }
    };

    struct Off {};
    inline constexpr bool operator==(const Off&, const Off&) { return true; };
};

using NoteSlot = std::variant<NoteTypes::Empty, NoteTypes::Note, NoteTypes::Off>;
using Note = NoteTypes::Note; // For convenience

inline constexpr bool NoteIsEmpty(const NoteSlot& note) { return note.index() == NoteTypes::EmptyType; }
inline constexpr bool NoteHasPitch(const NoteSlot& note) { return note.index() == NoteTypes::NoteType; }
inline constexpr bool NoteIsOff(const NoteSlot& note) { return note.index() == NoteTypes::OffType; }
inline constexpr const Note& GetNote(const NoteSlot& note)
{
    assert(NoteHasPitch(note) && "NoteSlot variant must be using the Note alternative");
    return std::get<Note>(note);
}

inline constexpr Note& GetNote(NoteSlot& note)
{
    assert(NoteHasPitch(note) && "NoteSlot variant must be using the Note alternative");
    return std::get<Note>(note);
}

inline int GetNoteRange(const Note& low, const Note& high)
{
    // Returns range in semitones. Assumes high >= low.
    // Range is inclusive on both ends.

    return (high.octave - low.octave) * 12 + (static_cast<uint8_t>(high.pitch) - static_cast<uint8_t>(low.pitch)) + 1;
}

} // namespace d2m
