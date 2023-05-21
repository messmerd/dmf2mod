/*
 * debug.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * A debug "module" for debug builds
 */

#pragma once

#ifndef NDEBUG

#include "core/module.h"

#include <string>

namespace d2m {

///////////////////////////////////////////////////////////
// Setup template specializations used by Debug
///////////////////////////////////////////////////////////

class Debug;

template<>
struct ModuleGlobalData<Debug> : public ModuleGlobalDataDefault<DataStorageType::kCOR> {};

///////////////////////////////////////////////////////////
// Debug primary classes
///////////////////////////////////////////////////////////

class DebugConversionOptions : public ConversionOptionsInterface<DebugConversionOptions>
{
public:

    // Factory requires destructor to be public
    ~DebugConversionOptions() override = default;

    enum class OptionEnum
    {
        kDump,
        kAppend,
        kGenDataFlags
    };

    [[nodiscard]] auto Dump() const -> bool { return GetOption(OptionEnum::kDump).GetValue<bool>(); }
    [[nodiscard]] auto Append() const -> bool { return GetOption(OptionEnum::kAppend).GetValue<bool>(); }
    [[nodiscard]] auto GenDataFlags() const -> size_t { return static_cast<size_t>(GetOption(OptionEnum::kGenDataFlags).GetValue<int>()); }

private:

    // Only allow the Factory to construct this class
    friend class Builder<DebugConversionOptions, ConversionOptionsBase>;

    DebugConversionOptions() = default;
};

class Debug final : public ModuleInterface<Debug>
{
public:

    enum ImportError
    {
        kSuccess = 0,
        kUnspecifiedError
    };

    // Factory requires destructor to be public
    ~Debug() override = default;

private:

    // Only allow the Factory to construct this class
    friend class Builder<Debug, ModuleBase>;

    Debug() = default;

    void ImportImpl(const std::string& filename) override;
    void ExportImpl(const std::string& filename) override;
    void ConvertImpl(const ModulePtr& input) override;
    [[nodiscard]] auto GenerateDataImpl(size_t data_flags) const -> size_t override { return 1; }

    std::string dump_;
};

} // namespace d2m

#endif // !NDEBUG
