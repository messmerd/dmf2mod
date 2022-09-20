/*
    generated_data.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines ModuleGeneratedData
*/

#include "note.h"
#include "data.h"
#include "state.h"
#include "module_base.h"

#include <cstddef>
#include <unordered_map>
#include <set>
#include <optional>
#include <cassert>

namespace d2m {

// Forward declare
class ModuleBase;

template<class ModuleClass>
class ModuleGeneratedDataStorageDefault
{
protected:
    ModuleGeneratedDataStorageDefault() = default;
    virtual ~ModuleGeneratedDataStorageDefault() = default;

protected:

    // All data types, wrapped with std::optional (?)
    using state_t = std::optional<ModuleState<ModuleClass>>;
    using sound_index_note_extremes_t = std::optional<std::unordered_map<sound_index_t, std::pair<Note, Note>>>;
    using channel_note_extremes_t = std::optional<std::unordered_map<channel_index_t, std::pair<Note, Note>>>;
    using note_off_used_t = std::optional<bool>;
    using sound_indexes_used_t = std::optional<std::set<sound_index_t>>;

    // data_t is a required type alias. Must be a std::tuple of the above data types in the order that they appear in DataEnum.
    using data_t = std::tuple<state_t, sound_index_note_extremes_t, channel_note_extremes_t, note_off_used_t, sound_indexes_used_t>;

    // Stores all the generated data
    data_t data_;

public:

    // Required enum. Variant values must correspond to the element indexes in data_t, and in the same order.
    enum class DataEnum : size_t
    {
        kState,
        kSoundIndexNoteExtremes,
        kChannelNoteExtremes,
        kNoteOffUsed,
        kSoundIndexesUsed,
        //kDuplicateOrders,
        kCount
    };

    // Not necessary, but could be used for potential performance improvements when calling Generate()
    //  by only generating the data which is needed.
    enum DataFlags : size_t
    {
        kFlagAll                    = 0, // 0 as a template parameter to Generate() means calculate all generated data
        kFlagState                  = 1,
        kFlagSoundIndexNoteExtremes = 2,
        kFlagChannelNoteExtremes    = 4,
        kFlagNoteOffUsed            = 8,
        kFlagSoundIndexesUsed       = 16,
        //kFlagDuplicateOrders        = 32,
    };
};


template<class ModuleClass>
class ModuleGeneratedDataStorage : public ModuleGeneratedDataStorageDefault<ModuleClass>
{
public:
};

// Implements methods for the Module's generated data storage - should not be specialized
template<class ModuleClass>
class ModuleGeneratedDataMethods : public ModuleGeneratedDataStorage<ModuleClass>
{
private:

    ModuleBase const* module_class_;

public:

    ModuleGeneratedDataMethods() = delete;
    ModuleGeneratedDataMethods(ModuleBase const* moduleClass) : module_class_(moduleClass) {}

    // Bring in dependencies from parent:
    using typename ModuleGeneratedDataStorageDefault<ModuleClass>::DataEnum;
    using ModuleGeneratedDataStorageDefault<ModuleClass>::data_;
    static constexpr size_t data_count_ = static_cast<size_t>(DataEnum::kCount);

    const std::optional<ModuleState<ModuleClass>>& GetState() const { return std::get<(size_t)DataEnum::kState>(data_); }
    std::optional<ModuleState<ModuleClass>>& GetState() { return std::get<(size_t)DataEnum::kState>(data_); }

    template<DataEnum I>
    const auto& Get() const { return std::get<(size_t)I>(data_); }
    template<DataEnum I>
    auto& Get() { return std::get<(size_t)I>(data_); }

    size_t Generate(size_t dataFlags)
    {
        assert(module_class_);
        return module_class_->GenerateDataImpl(dataFlags);
    }
};

// Can specialize this, but it must also inherit from ModuleGeneratedDataMethods<ModuleClass>
template<class ModuleClass>
class ModuleGeneratedData : public ModuleGeneratedDataMethods<ModuleClass>
{
public:
    // Inherit constructors
    using ModuleGeneratedDataMethods<ModuleClass>::ModuleGeneratedDataMethods;
};

} // namespace d2m