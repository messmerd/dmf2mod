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
#include <map>
#include <set>
#include <optional>
#include <cassert>

namespace d2m {

// Forward declare
class ModuleBase;


namespace detail {

struct GenDataDefinitionTag {};

template<typename T>
struct wrapped_gen_data {};

template<typename... Ts>
struct wrapped_gen_data<std::tuple<Ts...>>
{
    // type is either an empty tuple or a tuple with each Ts wrapped in an optional
    using type = std::conditional_t<sizeof...(Ts)==0, std::tuple<>, std::tuple<std::optional<Ts>...>>;
};

template<typename... T>
using wrapped_gen_data_t = typename wrapped_gen_data<T...>::type;

} // namespace detail

///////////////////////////////////////////////////////////
// COMMON GENERATED DATA TYPES
///////////////////////////////////////////////////////////

template<class T> using StateGenData = ModuleState<T>;
template<class T> using SoundIndexNoteExtremesGenData = std::map<typename SoundIndex<T>::type, std::pair<Note, Note>>;
using ChannelNoteExtremesGenData = std::map<ChannelIndex, std::pair<Note, Note>>;
using NoteOffUsedGenData = bool;
template<class T> using SoundIndexesUsedGenData = std::set<typename SoundIndex<T>::type>;
using LoopbackPointsGenData = std::map<OrderIndex, OrderRowPosition>; // Jump destination order --> Order/Row where the PosJump occurred

///////////////////////////////////////////////////////////
// COMMON GENERATED DATA DEFINITION
///////////////////////////////////////////////////////////

template<class TModule>
struct GeneratedDataCommonDefinition : public detail::GenDataDefinitionTag
{
    static constexpr int kCommonCount = 6; // # of variants in GenDataEnumCommon (remember to update this after changing the enum)
    static constexpr int kLowerBound = -kCommonCount;
    using ModuleClass = TModule;

    enum GenDataEnumCommon
    {
        //kDuplicateOrders        =-7,
        kNoteOffUsed            =-6,
        kLoopbackPoints         =-5,
        kChannelNoteExtremes    =-4,
        kSoundIndexNoteExtremes =-3,
        kSoundIndexesUsed       =-2,
        kState                  =-1,
    };

    // Lowest to highest
    using GenDataCommon = std::tuple<
        NoteOffUsedGenData,
        LoopbackPointsGenData,
        ChannelNoteExtremesGenData,
        SoundIndexNoteExtremesGenData<ModuleClass>,
        SoundIndexesUsedGenData<ModuleClass>,
        StateGenData<ModuleClass>
        >;

    // Not necessary, but could be used for potential performance improvements when calling Generate()
    //  by only generating the data which is needed.
    enum GenDataFlagsCommon : size_t
    {
        kFlagAll                    = 0, // 0 as a template parameter to Generate() means calculate all generated data
        kFlagState                  = 1,
        kFlagSoundIndexesUsed       = 2,
        kFlagSoundIndexNoteExtremes = 4,
        kFlagChannelNoteExtremes    = 8,
        kFlagLoopbackPoints         = 16,
        kFlagNoteOffUsed            = 32,
        kFlagDuplicateOrders        = 64
        // ...
    };
};

///////////////////////////////////////////////////////////
// GENERATED DATA STORAGE
///////////////////////////////////////////////////////////

template<class CommonDef, typename... Ts>
class GeneratedDataStorage : public CommonDef
{
public:
    static constexpr int kUpperBound = sizeof...(Ts); // # of module-specific gen data types

    using typename CommonDef::GenDataEnumCommon;
    using typename CommonDef::ModuleClass;

    // The GenDataEnum for any module-specific types should be defined
    //  in the GeneratedData template specialization

    using GenDataModuleSpecific = std::tuple<Ts...>;

    // Single tuple of all data types stored by this gen data storage
    using GenData = detail::tuple_cat_t<typename CommonDef::GenDataCommon, GenDataModuleSpecific>;

    // Single tuple of all wrapped data types stored by this gen data storage. They should all be optionals.
    using GenDataWrapped = detail::wrapped_gen_data_t<GenData>;

    // Returns an immutable reference to generated data at index gen_data_index
    template<int gen_data_index>
    constexpr const auto& Get() const
    {
        return std::get<gen_data_index + CommonDef::kCommonCount>(data_);
    }

    // Returns a mutable reference to generated data at index gen_data_index
    template<int gen_data_index>
    constexpr auto& Get()
    {
        return std::get<gen_data_index + CommonDef::kCommonCount>(data_);
    }

    // For convenience:
    constexpr const std::optional<ModuleState<ModuleClass>>& GetState() const { return Get<GenDataEnumCommon::kState>(); }
    constexpr std::optional<ModuleState<ModuleClass>>& GetState() { return Get<GenDataEnumCommon::kState>(); }

protected:
    GenDataWrapped data_; // Stores all generated data
};

///////////////////////////////////////////////////////////
// GENERATED DATA PRIMARY TEMPLATE
///////////////////////////////////////////////////////////

/*
 * The following is the generated data storage primary class template.
 * It can be specialized to add additional supported generated data if desired.
 * Any specializations must inherit from GeneratedDataStorage and pass the correct
 * common definition struct plus the new module-specific types to the template parameter.
 * In addition, specializations must define GenDataEnumCommon and GenDataEnum.
 * All generated data types must have a "==" operator defined for them.
 */

template<class ModuleClass>
struct GeneratedData : public GeneratedDataStorage<GeneratedDataCommonDefinition<ModuleClass> /* Module-specific types go here in any specializations */>
{
    //using Parent = GeneratedDataStorage<GeneratedDataCommonDefinition<ModuleClass>>;
    using typename GeneratedDataCommonDefinition<ModuleClass>::GenDataEnumCommon;
    enum GenDataEnum {};
};

// This is used by Module classes
template<class ModuleClass>
class ModuleGeneratedData : public GeneratedData<ModuleClass>
{
private:

    ModuleBase const* module_class_;

public:

    ModuleGeneratedData() = delete;
    ModuleGeneratedData(ModuleBase const* moduleClass) : module_class_(moduleClass) {}

    // Bring in dependencies from parent:
    using typename GeneratedData<ModuleClass>::GenDataEnumCommon;
    using typename GeneratedData<ModuleClass>::GenDataEnum;
    //using typename GeneratedData<ModuleClass>::Parent;

    size_t Generate(size_t dataFlags)
    {
        assert(module_class_);
        return module_class_->GenerateDataImpl(dataFlags);
    }

};

} // namespace d2m
