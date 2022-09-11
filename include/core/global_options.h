/*
    global_options.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    TODO: All these options should be made into Console-specific options.
          For verbose, an output stream should be given to Module classes to
          use rather than always using std::cout/std::cerr. This would be
          great for Web App builds because a stringstream could be provided
          and stdout won't need to be redirected.
*/

#pragma

#include "options.h"

namespace d2m {

class GlobalOptions
{
public:
    enum class OptionEnum
    {
        Force,
        Help,
        Verbose,
        Version
    };

    static void Set(const OptionCollection& globalOptions) { m_GlobalOptions = globalOptions; }
    static OptionCollection& Get() { return m_GlobalOptions; }

private:
    static OptionCollection m_GlobalOptions;
};

} // namespace d2m
