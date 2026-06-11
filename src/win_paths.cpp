#include "win_paths.h"

#include <shlobj.h>
#include <shlwapi.h>

namespace paths
{
bool ExistsFile(const std::wstring& path)
{
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool ExistsDir(const std::wstring& path)
{
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring ModuleFolder(HINSTANCE module)
{
    wchar_t file[MAX_PATH] = {};
    GetModuleFileNameW(module, file, MAX_PATH);
    PathRemoveFileSpecW(file);
    return file;
}

std::wstring DocumentsFolder()
{
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, path)) && path[0]) {
        return path;
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders",
        0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD bytes = sizeof(path);
        if (RegQueryValueExW(key, L"Personal", nullptr, &type, (LPBYTE)path, &bytes) == ERROR_SUCCESS && path[0]) {
            wchar_t expanded[MAX_PATH] = {};
            ExpandEnvironmentStringsW(path, expanded, MAX_PATH);
            RegCloseKey(key);
            return expanded;
        }
        RegCloseKey(key);
    }

    return L"C:\\Users\\Administrator\\Documents";
}

std::wstring LocateEts2()
{
    wchar_t path[MAX_PATH] = {};
    const wchar_t* keys[] = {
        L"SOFTWARE\\SCS Software\\Euro Truck Simulator 2",
        L"SOFTWARE\\WOW6432Node\\SCS Software\\Euro Truck Simulator 2"
    };

    for (const wchar_t* keyName : keys) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyName, 0, KEY_READ, &key) == ERROR_SUCCESS) {
            DWORD type = 0;
            DWORD bytes = sizeof(path);
            if (RegQueryValueExW(key, L"InstallDir", nullptr, &type, (LPBYTE)path, &bytes) == ERROR_SUCCESS && ExistsDir(path)) {
                RegCloseKey(key);
                return path;
            }
            RegCloseKey(key);
        }
    }

    const wchar_t* guesses[] = {
        L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Euro Truck Simulator 2",
        L"D:\\Steam\\steamapps\\common\\Euro Truck Simulator 2",
        L"D:\\SteamLibrary\\steamapps\\common\\Euro Truck Simulator 2",
        L"E:\\SteamLibrary\\steamapps\\common\\Euro Truck Simulator 2"
    };
    for (const wchar_t* guess : guesses) {
        if (ExistsDir(guess)) return guess;
    }
    return L"";
}
}
