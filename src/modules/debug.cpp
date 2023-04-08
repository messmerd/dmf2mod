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

    static_assert(ChannelState<Debug>::kCommonCount == 12);
    static_assert(ChannelState<Debug>::kOneShotCommonCount == 3);

    /*
    switch (input->GetType())
    {
    case ModuleType::kDMF:
    {
        using Common = ChannelState<DMF>::ChannelOneShotCommonDefinition;
        auto derived = input->Cast<DMF>();
        auto gen_data = derived->GetGeneratedData();

        const auto& note_delay = gen_data->Get<Common::kNoteDelay>();
        if (note_delay)
        {
            for (auto& [index, extremes] : note_delay.value())
            {
                if (std::is_same_v<SoundIndex<DMF>::Square, decltype(index)>)
                {
                    dump_ += "";
                }
            }
        }

        break;
    }
    case ModuleType::kMOD:
    {
        auto derived = input->Cast<MOD>();
        auto gen_data = derived->GetGeneratedData();

        break;
    }
    default:
        break;
    }
    */
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
