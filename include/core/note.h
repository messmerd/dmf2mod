/*
    note.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines a data structure for storing notes + helper functions.
*/

#pragma once

#include <variant>
#include <cstdint>

namespace d2m {

enum class NotePitch : uint16_t
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
    struct Note
    {
        NotePitch pitch;
        uint16_t octave;

        //Note(NotePitch pitch, uint16_t octave) : pitch(pitch), octave(octave) {}

        bool operator>(const Note& rhs) const
        {
            return (this->octave << 4) + static_cast<uint16_t>(this->pitch) > (rhs.octave << 4) + static_cast<uint16_t>(rhs.pitch);
        }

        bool operator>=(const Note& rhs) const
        {
            return (this->octave << 4) + static_cast<uint16_t>(this->pitch) >= (rhs.octave << 4) + static_cast<uint16_t>(rhs.pitch);
        }

        bool operator<(const Note& rhs) const
        {
            return (this->octave << 4) + static_cast<uint16_t>(this->pitch) < (rhs.octave << 4) + static_cast<uint16_t>(rhs.pitch);
        }

        bool operator<=(const Note& rhs) const
        {
            return (this->octave << 4) + static_cast<uint16_t>(this->pitch) <= (rhs.octave << 4) + static_cast<uint16_t>(rhs.pitch);
        }

        bool operator==(const Note& rhs) const
        {
            return this->octave == rhs.octave && this->pitch == rhs.pitch;
        }

        bool operator!=(const Note& rhs) const
        {
            return this->octave != rhs.octave || this->pitch != rhs.pitch;
        }
    };
    struct Off {};
};

using NoteSlot = std::variant<NoteTypes::Empty, NoteTypes::Note, NoteTypes::Off>;
using Note = NoteTypes::Note; // For convenience

inline bool NoteIsEmpty(const NoteSlot& note) { return note.index() == NoteTypes::EmptyType; }
inline bool NoteHasPitch(const NoteSlot& note) { return note.index() == NoteTypes::NoteType; }
inline bool NoteIsOff(const NoteSlot& note) { return note.index() == NoteTypes::OffType; }
inline Note& GetNote(NoteSlot& note)
{
    assert(NoteHasPitch(note) && "NoteSlot variant must be using the Note alternative");
    return std::get<Note>(note);
}

inline const Note& GetNote(const NoteSlot& note)
{
    assert(NoteHasPitch(note) && "NoteSlot variant must be using the Note alternative");
    return std::get<Note>(note);
}

inline int GetNoteRange(const Note& low, const Note& high)
{
    // Returns range in semitones. Assumes high >= low.
    // Range is inclusive on both ends.

    return (high.octave - low.octave) * 12 + (static_cast<uint16_t>(high.pitch) - static_cast<uint16_t>(low.pitch)) + 1;
}

} // namespace d2m
