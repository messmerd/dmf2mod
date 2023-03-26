/*
 * global_options.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * TODO: All these options should be made into Console-specific options.
 *       For verbose, an output stream should be given to Module classes to
 *       use rather than always using std::cout/std::cerr. This would be
 *       great for Web App builds because a stringstream could be provided
 *       and stdout won't need to be redirected.
 */

#pragma once

#include "core/options.h"

namespace d2m {

class GlobalOptions
{
public:
    enum class OptionEnum
    {
        kForce,
        kHelp,
        kVerbose,
        kVersion
    };

    static void Set(const OptionCollection& global_options) { global_options_ = global_options; }
    static auto Get() -> OptionCollection& { return global_options_; }

private:
    static OptionCollection global_options_;
};

} // namespace d2m
