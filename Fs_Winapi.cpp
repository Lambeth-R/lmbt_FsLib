#include "FsLib.h"
#include "StringConvertLib.h"

#if defined FS_USE_WINAPI && defined PLATFORM_WIN
#include <Shlwapi.h>
#include <userenv.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Userenv.lib")

namespace
{
    template<typename T>
    bool writeFileEx(const fs::path &filePath, const T &data, const bool force)
    {
        const auto  last_error      = GetLastError();
        const auto  expanded_path   = fs::expandPath(filePath);
        DWORD       dw_written      = 0,
                    attributes      = force ? CREATE_ALWAYS : CREATE_NEW;
        MakeScopeGuard([&] { SetLastError(last_error); });
        auto h_File = CreateFileW(expanded_path.c_str(), GENERIC_WRITE, 0, nullptr, attributes, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!h_File || h_File == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        MakeScopeGuard([&] { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }});
        const size_t size_bytes = data.size() / sizeof(T::value_type);
        if (!WriteFile(h_File, data.data(), static_cast<DWORD>(size_bytes), &dw_written, nullptr) || dw_written != size_bytes)
        {
            return false;
        }
        return true;
    }

    template<typename T>
    bool readFileEx(const fs::path &filePath, T &data, const bool silent)
    {
        const auto              last_error = GetLastError();
        const auto              expanded_path = fs::expandPath(filePath);
        fs::file_metadata file_attributes;
        DWORD             dw_read = 0;
        LARGE_INTEGER     li_size = {};
        ZeroMemory(&file_attributes, sizeof(fs::file_metadata));
        MakeScopeGuard([&] { SetLastError(last_error); });
        auto h_File = CreateFileW(expanded_path.c_str(), GENERIC_READ, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!h_File || h_File == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        MakeScopeGuard([&] { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }});
        if (silent && !fs::winapi::getFileMetadata(filePath, file_attributes))
        {
            return false;
        }
        if (!GetFileSizeEx(h_File, &li_size))
        {
            return false;
        }
        data.resize(li_size.QuadPart / sizeof(T::value_type));
        if (!ReadFile(h_File, data.data(), static_cast<DWORD>(li_size.QuadPart), &dw_read, nullptr))
        {
            data.resize(0);
            return false;
        }
        if (silent && !fs::winapi::setFileMetadata(filePath, file_attributes))
        {
            data.resize(0);
            return false;
        }
        return true;
    }

    template<typename T>
    bool appendFileEx(const fs::path &filePath, const T &data, const bool silent)
    {
        const auto              last_error      = GetLastError();
        const auto              expanded_path   = fs::expandPath(filePath);
        fs::file_metadata file_attributes;
        DWORD             dw_written = 0;

        ZeroMemory(&file_attributes, sizeof(fs::file_metadata));
        MakeScopeGuard([&] { SetLastError(last_error); });

        auto h_File = CreateFileW(expanded_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!h_File || h_File == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        MakeScopeGuard([&] { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }});
        if (SetFilePointer(h_File, 0, nullptr, FILE_END) == INVALID_SET_FILE_POINTER)
        {
            return false;
        }
        if (silent && !fs::winapi::getFileMetadata(filePath, file_attributes))
        {
            return false;
        }
        const size_t size_bytes = data.size() / sizeof(T::value_type);
        if (!WriteFile(h_File, data.data(), static_cast<DWORD>(size_bytes), &dw_written, nullptr) || dw_written == 0)
        {
            return false;
        }
        if (silent && !fs::winapi::setFileMetadata(filePath, file_attributes))
        {
            return true;
        }
        return true;
    }
}

namespace fs::winapi
{
DEFINE_MAIN_OUT_OPS(write, force)
DEFINE_MAIN_OUT_OPS(append, silent)
DEFINE_MAIN_IN_OPS(read, silent)

LIB_EXPORT
const fs::path getMountPointPath(const fs::path &Path)
{
    const auto &expanded_path = expandPath(Path);
    wchar_t dev_path[50]; // 50 is max
    const size_t letter_off = expanded_path.wstring().find_first_of(L":") + 2;
    const auto& letter = expanded_path.wstring().substr(0, letter_off + 2);
    if (!GetVolumeNameForVolumeMountPointW(letter.c_str(), dev_path, 50))
    {
        return expanded_path;
    }
    return fs::path(dev_path);
}

static const fs::path expandDosStylePath(const fs::path &Path)
{
    static const     wchar_t    dos_prefix[]    = L"\\\\?\\";
    static constexpr size_t     dos_pref_sz     = 4;
           const     auto      &w_path          = Path.wstring();
    if (w_path.size() > dos_pref_sz && std::char_traits<wchar_t>::compare(w_path.substr(0, dos_pref_sz).data(), dos_prefix, dos_pref_sz) == 0)
    {
        return Path;
    }
    DWORD len = GetFullPathNameW(w_path.c_str(), 0, 0, 0);
    if (len)
    {
        wchar_t *c;
        std::wstring tmp_str(dos_prefix);
        tmp_str.resize(dos_pref_sz + len, L' ');
        if (len - 1 == GetFullPathNameW(w_path.c_str(), len, &tmp_str[4], &c))
        {
            tmp_str.resize(len + 3);
            return fs::path(tmp_str);
        }
    }
    return Path;
}

LIB_EXPORT
const fs::path expandPath(const fs::path &Path)
{
    if (Path.is_relative())
    {
        return expandDosStylePath(Path);
    }
    return Path;
}

LIB_EXPORT
bool removeFile(const fs::path &filePath)
{
    const auto expanded_path = expandPath(filePath);
    if (!::DeleteFileW(expanded_path.wstring().c_str()))
    {
        return false;
    }
    return true;
}

LIB_EXPORT
bool removeDir(const fs::path &Path, bool recursive)
{
    const auto expanded_path = expandPath(Path);
          bool ret           = true,
               is_dir        = isDirectory(expanded_path);
    if (recursive)
    {
        const auto dir_data = enumDir(expanded_path, "");
        for (auto& it : dir_data)
        {
            const fs::path it_path = expanded_path / it;
            if (isDirectory(it_path))
            {
                ret &= removeDir(it_path, recursive);
            }
            else
            {
                ret &= (DeleteFileW(it_path.wstring().c_str()) >= TRUE);
            }
        }
        return ret && RemoveDirectoryW(expanded_path.wstring().c_str());
    }
    else
    {
        return RemoveDirectoryW(expanded_path.wstring().c_str());
    }
}

LIB_EXPORT
bool isDirectory(const fs::path &Path)
{
    const auto& expanded_path = expandPath(Path);
    if (GetFileAttributesW(expanded_path.c_str()) & FILE_ATTRIBUTE_DIRECTORY)
    {
        return true;
    }
    return false;
}

LIB_EXPORT
bool isExist(const fs::path &Path)
{
    const auto  expanded_path   = expandPath(Path);
    const DWORD dw_attrib       = GetFileAttributesW(expanded_path.c_str());
          bool  exist           = (dw_attrib != INVALID_FILE_ATTRIBUTES && (dw_attrib & FILE_ATTRIBUTE_DIRECTORY));
    if (!exist)
    {
        auto h_File = CreateFileW(expanded_path.c_str(), FILE_READ_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!h_File || h_File == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        CloseHandle(h_File);
        exist = true;
    }
    return exist;
}

LIB_EXPORT
bool createDirectory(const fs::path &Path, bool recirsive)
{
    const auto expanded_path = expandPath(Path);
    while(recirsive && !isExist(expanded_path.parent_path()))
    {
        if (!createDirectory(expanded_path.parent_path(), recirsive))
        {
            return false;
        }
    }
    if (!CreateDirectoryW(expanded_path.c_str(), nullptr))
    {
        return false;
    }
    return true;
}

LIB_EXPORT
bool getFileMetadata(const fs::path &Path, fs::file_metadata &attributes)
{
    const auto                      expanded_path = expandPath(Path);
          FILE_ATTRIBUTE_TAG_INFO   attrib_tag;
    ZeroMemory(&attrib_tag, sizeof(FILE_ATTRIBUTE_TAG_INFO));

    auto h_File = CreateFileW(expanded_path.c_str(), FILE_READ_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!h_File || h_File == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    MakeScopeGuard([&] {if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }});
    if (!GetFileInformationByHandleEx(h_File, FILE_INFO_BY_HANDLE_CLASS::FileAttributeTagInfo, &attrib_tag, sizeof(FILE_ATTRIBUTE_TAG_INFO)))
    {
        return false;
    }
    attributes.FileAttributes = attrib_tag.FileAttributes;
    if (!GetFileTime(h_File, &attributes.CreationTime.ft, &attributes.LastAccessTime.ft, &attributes.LastWriteTime.ft))
    {
        return false;
    }
    return true;
}

LIB_EXPORT
bool setFileMetadata(const fs::path &Path, const fs::file_metadata &attributes)
{
    const auto expanded_path = expandPath(Path);
          auto h_File        = CreateFileW(expanded_path.c_str(), FILE_WRITE_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!h_File || h_File == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    MakeScopeGuard([&] {if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }});
    // "attributes.FileAttributes" can be used only by GetFileInformationByHandleEx
    if (!SetFileTime(h_File, &attributes.CreationTime.ft, &attributes.LastAccessTime.ft, &attributes.LastWriteTime.ft))
    {
        return false;
    }
    return true;
}

LIB_EXPORT
const std::list<fs::path> enumDir(const fs::path &Path, const std::string& regFilter)
{
    const auto                  file_pattern = expandPath(Path) / L"*";
          bool                  ret          = true;
          WIN32_FIND_DATAW      file_info;
          std::list<fs::path>   result;
    const auto                  utf_filter   = ConvertUTF::UTF8_Decode(regFilter);
    ZeroMemory(&file_info, sizeof(WIN32_FIND_DATAW));
    auto h_File = FindFirstFileW(file_pattern.c_str(), &file_info);
    MakeScopeGuard([&] { if (h_File && h_File != INVALID_HANDLE_VALUE) { FindClose(h_File); h_File = nullptr; }});
    // Skipping /. && /..
#if defined IS_CPP_17G
    while (std::wstring_view(file_info.cFileName) == L"." || std::wstring_view(file_info.cFileName) == L"..")
#else
    while (std::wstring(file_info.cFileName) == L"." || std::wstring(file_info.cFileName) == L"..")
#endif
    {
        if (!FindNextFileW(h_File, &file_info))
        {
            ret = false;
            break;
        }
    }
    do
    {
        if (!regFilter.empty() && !std::regex_match(file_info.cFileName, std::wregex(utf_filter)))
        {
            continue;
        }
        result.push_back(file_info.cFileName);
    } while (FindNextFileW(h_File, &file_info));
    return result;
}

}
#endif