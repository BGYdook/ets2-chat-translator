#pragma once

#include <string>
#include <windows.h>

namespace paths
{
    std::wstring ModuleFolder(HINSTANCE module);
    std::wstring DocumentsFolder();
    std::wstring LocateEts2();
    bool ExistsFile(const std::wstring& path);
    bool ExistsDir(const std::wstring& path);
}
