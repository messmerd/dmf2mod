/*
    generated_data.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines ModuleGeneratedData
*/

#include "note.h"
#include "data.h"
#include "state.h"

#include <cstddef>
#include <unordered_map>

namespace d2m {

template<class ModuleClass>
class ModuleGeneratedDataDefault
{
protected:
    ModuleGeneratedDataDefault() = default;
    virtual ~ModuleGeneratedDataDefault() = default;

public:

    // A unique identifier for wavetables, duty cycles, samples, etc.
    using sound_index_t = size_t;

    const ModuleState<ModuleClass>& GetState() const { return state_; }
    ModuleState<ModuleClass>& GetState() { return state_; }

    inline uint32_t GetPos(uint16_t order, uint16_t row) const { return (order << 16) | row; };
    inline std::pair<order_index_t, row_index_t> GetPos(uint32_t pos) const { return { pos >> 16, pos & 0x00FF }; };

private:
    // Generated data storage

    std::unordered_map<sound_index_t, std::pair<Note, Note>> sound_index_note_extremes_;
    std::unordered_map<channel_index_t, std::pair<Note, Note>> channel_note_extremes_;

    enum DataFlags : size_t
    {
        kSoundIndexNoteExtremes = 1,
        kChannelNoteExtremes    = 2,
        kNoteOffUsed            = 8,
        kSoundIndexesUsed       = 32,
        kDuplicateOrders        = 64,
    };


    /*
     * Generated data for Module class A is intended to be used by Module classes B, C, and D
     * which call methods on A's GeneratedData object to get the info they need for the conversion.
     * Each type of generated data should be associated with a unique flag enum.
     * These flag enums can be combined together by calling code to make a request for which
     * generated data the calling code requires using the GeneratedData object's Generate method,
     * and the GeneratedData object will return a flag enum indicating which data it was able to
     * generate. This can be more efficient if the calling code does not need expensive data to be
     * available. And some kinds of data can be efficiently obtained in the same big For loop.
     * Could use if-constexpr to enable the collection of different data using the DataFlag template
     * parameter. After the data is generated, getting it can be done with:
     *     std::optional<DataFlagSpecificDataObject> data = Get<MyDataFlag>();
     * If Generate was not called first (or there was an error calling it), the data associated with
     * the data flag will be lazily calculated.
     * 
     * Again, a Module class's generated data will primarily be used by *other* Module classes.
     * There is going to be generated data implemented by and calculated by class A which is only
     * really useful to one other Module class B, but only A should have to know the details of how the
     * information is calculated. This does feel less modular, but it the right thing to do as far as
     * abstraction goes - which is more important. I can't be completely modular anyway, and it isn't
     * super important - it's mainly to avoid any shotgun surgery anti-patterns.
     */

    ModuleState<ModuleClass> state_;

};


template<class ModuleClass>
class ModuleGeneratedData : public ModuleGeneratedDataDefault<ModuleClass> {};


} // namespace d2m
