/*
 * debug.cpp
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * A debug "module" for debug builds
 */

#include "modules/debug.h"

#ifndef NDEBUG

#include "dmf2mod.h"

#include <iostream>
#include <fstream>
#include <cassert>

using namespace d2m;

namespace {

struct SoundIndexVisitor
{
    auto operator()(const SoundIndexType<DMF>& val) -> std::string
    {
        using SI = SoundIndex<DMF>;
        if (auto square = std::get_if<SI::Square>(&val); square)
            { return "SQUARE:" + std::to_string(square->id); }
        else if (auto wave = std::get_if<SI::Wave>(&val); wave)
            { return "WAVE:" + std::to_string(wave->id); }
        else if (auto noise = std::get_if<SI::Noise>(&val); noise)
            { return "NOISE:" + std::to_string(noise->id); }
        else { return "UNKNOWN"; }
    }
};

} // namespace

void Debug::ImportImpl(const std::string& filename)
{
    // Not implemented
    throw NotImplementedException{};
}

void Debug::ConvertImpl(const ModulePtr& input)
{
    dump_.clear();
    if (!input)
    {
        throw MODException(ModuleException::Category::kConvert, ModuleException::ConvertError::kInvalidArgument);
    }

    const auto options = GetOptions()->Cast<DebugConversionOptions>();
    const auto append = options->Append();
    const auto flags = options->GenDataFlags();

    const bool verbose = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kVerbose).GetValue<bool>();
    if (verbose)
    {
        std::cout << "~~~DEBUG~~~\n";
        std::cout << "Append results to log file: " << append << "\n";
        std::cout << "Generated data flags: " << flags << "\n\n";
    }

    [[maybe_unused]] auto result = input->GenerateData(flags);
    dump_ += "GenerateData result: " + std::to_string(result) + "\n";

    static_assert(ChannelState<Debug>::kCommonCount == 11);
    static_assert(ChannelState<Debug>::kOneShotCommonCount == 3);

    switch (input->GetType())
    {
    case ModuleType::kDMF:
    {
        //using Common = ChannelState<DMF>::ChannelOneShotCommonDefinition;
        //auto derived = input->Cast<DMF>();
        // ...
        break;
    }
    case ModuleType::kMOD:
    {
        //auto derived = input->Cast<MOD>();
        //auto gen_data = derived->GetGeneratedData();
        // ...
        break;
    }
    default:
        break;
    }
}

void Debug::ExportImpl(const std::string& filename)
{
    const auto options = GetOptions()->Cast<DebugConversionOptions>();
    auto mode = std::ios::out;
    if (options->Append()) { mode |= std::ios::app; }

    std::ofstream out_file{filename, mode};
    if (!out_file.is_open())
    {
        throw MODException(ModuleException::Category::kExport, ModuleException::ExportError::kFileOpen);
    }

    // ...

    out_file.close();

    const bool verbose = GlobalOptions::Get().GetOption(GlobalOptions::OptionEnum::kVerbose).GetValue<bool>();
    if (verbose) { std::cout << "Wrote log to disk.\n\n"; }
}

#endif // !NDEBUG
