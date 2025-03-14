/*
 * note.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines a data structure for storing notes + helper functions.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <variant>

namespace d2m {

enum class NotePitch : std::uint8_t
{
	kC=0,
	kCS,
	kD,
	kDS,
	kE,
	kF,
	kFS,
	kG,
	kGS,
	kA,
	kAS,
	kB
};

namespace NoteTypes
{
	enum { kEmpty, kNote, kOff }; // The order is important

	struct Empty {};
	constexpr auto operator==(const Empty&, const Empty&) -> bool { return true; };

	struct alignas(1) Note
	{
		NotePitch pitch : 4;
		uint8_t octave : 4;

		constexpr Note() : pitch(NotePitch::kC), octave(0) {}
		constexpr Note(NotePitch pitch, std::uint8_t octave) : pitch(pitch), octave(octave) {}

		constexpr auto operator>(Note rhs) const -> bool
		{
			return (this->octave << 4) + static_cast<std::uint8_t>(this->pitch) > (rhs.octave << 4) + static_cast<std::uint8_t>(rhs.pitch);
		}

		constexpr auto operator>=(Note rhs) const -> bool
		{
			return (this->octave << 4) + static_cast<std::uint8_t>(this->pitch) >= (rhs.octave << 4) + static_cast<std::uint8_t>(rhs.pitch);
		}

		constexpr auto operator<(Note rhs) const -> bool
		{
			return (this->octave << 4) + static_cast<std::uint8_t>(this->pitch) < (rhs.octave << 4) + static_cast<std::uint8_t>(rhs.pitch);
		}

		constexpr auto operator<=(Note rhs) const -> bool
		{
			return (this->octave << 4) + static_cast<std::uint8_t>(this->pitch) <= (rhs.octave << 4) + static_cast<std::uint8_t>(rhs.pitch);
		}

		constexpr auto operator==(Note rhs) const -> bool
		{
			return this->octave == rhs.octave && this->pitch == rhs.pitch;
		}

		constexpr auto operator!=(Note rhs) const -> bool
		{
			return this->octave != rhs.octave || this->pitch != rhs.pitch;
		}
	};

	struct Off {};
	constexpr auto operator==(const Off&, const Off&) -> bool { return true; };
};

using NoteSlot = std::variant<NoteTypes::Empty, NoteTypes::Note, NoteTypes::Off>;
using Note = NoteTypes::Note; // For convenience

constexpr auto NoteIsEmpty(const NoteSlot& note) -> bool { return note.index() == NoteTypes::kEmpty; }
constexpr auto NoteHasPitch(const NoteSlot& note) -> bool { return note.index() == NoteTypes::kNote; }
constexpr auto NoteIsOff(const NoteSlot& note) -> bool { return note.index() == NoteTypes::kOff; }
constexpr auto GetNote(const NoteSlot& note) -> const Note&
{
	assert(NoteHasPitch(note) && "NoteSlot variant must be using the Note alternative");
	return std::get<Note>(note);
}

constexpr auto GetNote(NoteSlot& note) -> Note&
{
	assert(NoteHasPitch(note) && "NoteSlot variant must be using the Note alternative");
	return std::get<Note>(note);
}

constexpr auto GetNoteRange(const Note& low, const Note& high) -> int
{
	// Returns range in semitones. Assumes high >= low.
	// Range is inclusive on both ends.

	return (high.octave - low.octave) * 12 + (static_cast<std::uint8_t>(high.pitch) - static_cast<std::uint8_t>(low.pitch)) + 1;
}

} // namespace d2m
