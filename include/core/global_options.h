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
        Verbose
    };

    static void Set(const OptionCollection& globalOptions) { m_GlobalOptions = globalOptions; }
    static OptionCollection& Get() { return m_GlobalOptions; }

private:
    static OptionCollection m_GlobalOptions;
};

} // namespace d2m
