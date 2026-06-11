#pragma once

#include "core_types.h"

#include <string>
#include <vector>

struct AppSettings
{
    RuntimeConfig runtime;
    std::vector<ProviderSettings> providers;
};

namespace settings
{
    AppSettings Defaults();
    AppSettings Load(const std::wstring& path);
    bool WriteDefaultFile(const std::wstring& path);
}
