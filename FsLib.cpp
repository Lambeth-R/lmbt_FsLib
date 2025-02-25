#include "FsLib.h"

#include <algorithm>
#if _CPP_VERSION >= 201703L
#include <string_view>
#endif
#include <regex>

#ifdef _MSC_VER
#include <Shlwapi.h>
#include <userenv.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Userenv.lib")

LIB_EXPORT
const fs::path Fs_GetMountPointPath(const fs::path &Path)
{
    const auto &expanded_path = Fs_ExpandPath(Path);
    wchar_t dev_path[50]; // 50 is max
    const size_t letter_off = expanded_path.wstring().find_first_of(L":") + 2;
    const auto& letter = expanded_path.wstring().substr(0, letter_off + 2);
    if (!GetVolumeNameForVolumeMountPointW(letter.c_str(), dev_path, 50))
    {
        return expanded_path;
    }
    return fs::path(dev_path);
}

LIB_EXPORT
const fs::path Fs_ExpandPath(const fs::path &Path)
{
    static const wchar_t       dos_prefix[] = L"\\\\?\\";
    static const size_t        dos_pref_sz  = sizeof(dos_prefix) / sizeof(wchar_t);
           const std::wstring &w_path       = Path.wstring();
    if (w_path.size() > dos_pref_sz && std::char_traits<wchar_t>::compare(w_path.substr(0,4).data(), dos_prefix, dos_pref_sz) == 0)
    {
        return Path;
    }
    if (DWORD len = GetFullPathNameW(Path.wstring().c_str(), 0, 0, 0))
    {
        wchar_t *c;
        std::wstring tmp_str(dos_prefix);
        tmp_str.resize(len + 4,L' ');
        if (len - 1 == GetFullPathNameW(Path.wstring().c_str(), len, tmp_str.data() + 4, &c))
        {
            tmp_str.resize(len + 3);
            return tmp_str;
        }
    }
    return Path;
}

template<typename T>
static bool Fs_WriteFileEx(const fs::path &filePath, const T& data, bool force)
{
    const auto  expanded_path = Fs_ExpandPath(filePath);
          DWORD dw_written    = 0,
                attributes    = force ? CREATE_ALWAYS : CREATE_NEW;
    auto h_File = CreateFileW(expanded_path.c_str(), GENERIC_WRITE, 0, nullptr, attributes, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!h_File || h_File == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    MakeScopeGuard([&] { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }});
    if (!WriteFile(h_File, data.data(), static_cast<DWORD>(data.size()), &dw_written, nullptr) || dw_written == 0)
    {
        return false;
    }
    return true;
}

template<typename T>
static bool Fs_ReadFileEx(const fs::path &filePath, T& data, bool silent)
{
    const auto              expanded_path   = Fs_ExpandPath(filePath);
          fs::FileMetadata  file_attributes;
          DWORD             dw_read         = 0;
          LARGE_INTEGER     li_size         = {};
    ZeroMemory(&file_attributes, sizeof(fs::FileMetadata));

    auto h_File = CreateFileW(expanded_path.c_str(), GENERIC_READ, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!h_File || h_File == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    MakeScopeGuard([&] { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }});
    if (silent && !Fs_GetFileMetadata(filePath, file_attributes))
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
    if (silent && !Fs_SetFileMetadata(filePath, file_attributes))
    {
        data.resize(0);
        return false;
    }
    return true;
}

template<typename T>
static bool Fs_AppendFileEx(const fs::path &filePath, const T& data, bool silent)
{
    const auto              last_error    = GetLastError();
    const auto              expanded_path = Fs_ExpandPath(filePath);
          fs::FileMetadata  file_attributes;
          DWORD             dw_written    = 0;

    ZeroMemory(&file_attributes, sizeof(fs::FileMetadata));
    MakeScopeGuard([&] { SetLastError(last_error); } );

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
    if (silent && !Fs_GetFileMetadata(filePath, file_attributes))
    {
        return false;
    }
    if (!WriteFile(h_File, data.data(), static_cast<DWORD>(data.size()) * static_cast<DWORD>(sizeof(data.data()[0])), &dw_written, nullptr) || dw_written == 0)
    {
        return false;
    }
    if (silent && !Fs_SetFileMetadata(filePath, file_attributes))
    {
        return true;
    }
    return true;
}

LIB_EXPORT
bool Fs_DeleteFile(const fs::path &filePath)
{
    const auto expanded_path = Fs_ExpandPath(filePath);
    if (!DeleteFileW(expanded_path.wstring().c_str()))
    {
        return false;
    }
    return true;
}

LIB_EXPORT
bool Fs_RemoveDir(const fs::path &Path, bool recursive)
{
    const auto expanded_path = Fs_ExpandPath(Path);
          bool ret           = true,
               is_dir        = Fs_IsDirectory(expanded_path);
    if (recursive)
    {
        const auto dir_data = Fs_EnumDir(expanded_path);
        for (auto& it : dir_data)
        {
            const fs::path it_path = expanded_path / it;
            if (Fs_IsDirectory(it_path))
            {
                ret &= Fs_RemoveDir(it_path);
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
bool Fs_IsDirectory(const fs::path &Path)
{
    const auto& expanded_path = Fs_ExpandPath(Path);
    if (GetFileAttributesW(expanded_path.c_str()) & FILE_ATTRIBUTE_DIRECTORY)
    {
        return true;
    }
    return false;
}

LIB_EXPORT
bool Fs_IsExist(const fs::path &Path)
{
    const auto  expanded_path   = Fs_ExpandPath(Path);
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
bool Fs_CreateDirectory(const fs::path &Path, bool recirsive)
{
    const auto expanded_path = Fs_ExpandPath(Path);
    while(recirsive && !Fs_IsExist(expanded_path.parent_path()))
    {
        if (!Fs_CreateDirectory(expanded_path.parent_path()))
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
bool Fs_GetFileMetadata(const fs::path &Path, fs::FileMetadata& attributes)
{
    const auto                      expanded_path = Fs_ExpandPath(Path);
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
bool Fs_SetFileMetadata(const fs::path &Path, const fs::FileMetadata& attributes)
{
    const auto expanded_path = Fs_ExpandPath(Path);
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
const std::vector<fs::path> Fs_EnumDir(const fs::path &Path, const std::wstring& regFilter)
{
    const auto                  file_pattern = Fs_ExpandPath(Path) / L"*";
          bool                  ret          = true;
          WIN32_FIND_DATAW      file_info;
          std::vector<fs::path> ret_vec;
    ZeroMemory(&file_info, sizeof(WIN32_FIND_DATAW));
    auto h_File = FindFirstFileW(file_pattern.c_str(), &file_info);
    MakeScopeGuard([&] { if (h_File && h_File != INVALID_HANDLE_VALUE) { FindClose(h_File); h_File = nullptr; }});
    // Skipping /. && /..
#if _HAS_CXX17
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
        if (!regFilter.empty() && !std::regex_match(file_info.cFileName, std::wregex(regFilter)))
        {
            continue;
        }
        ret_vec.push_back(file_info.cFileName);
    } while (FindNextFileW(h_File, &file_info));
    return ret_vec;
}

#else // Unix like systems

template<typename T>
static bool Fs_WriteFileEx(const fs::path &filePath, const T& data, bool force)
{
    return false;
}

template<typename T>
static bool Fs_ReadFileEx(const fs::path &filePath, const T& data, bool force)
{



    return false;
}

template<typename T>
static bool Fs_AppendFileEx(const fs::path &filePath, const T& data, bool force)
{
    return false;
}

#endif


// Platform independent calls
LIB_EXPORT
bool Fs_WriteFile(const fs::path &filePath, const std::string &data, bool force)
{
    return Fs_WriteFileEx(filePath, data, force);
}

LIB_EXPORT
bool Fs_WriteFile(const fs::path &filePath, const std::wstring &data, bool force)
{
    return Fs_WriteFileEx(filePath, data, force);
}

LIB_EXPORT
bool Fs_WriteFile(const fs::path &filePath, const std::vector<uint8_t> &data, bool force)
{
    return Fs_WriteFileEx(filePath, data, force);
}

LIB_EXPORT
bool Fs_ReadFile(const fs::path &filePath, std::string &data, bool silent)
{
    return Fs_ReadFileEx(filePath, data, silent);
}

LIB_EXPORT
bool Fs_ReadFile(const fs::path &filePath, std::wstring &data, bool silent)
{
    return Fs_ReadFileEx(filePath, data, silent);
}

LIB_EXPORT
bool Fs_ReadFile(const fs::path &filePath, std::vector<uint8_t> &data, bool silent)
{
    return Fs_ReadFileEx(filePath, data, silent);
}

LIB_EXPORT
bool Fs_AppendFile(const fs::path &filePath, const std::string &data, bool silent)
{
    return Fs_AppendFileEx(filePath, data, silent);
}

LIB_EXPORT
bool Fs_AppendFile(const fs::path &filePath, const std::wstring &data, bool silent)
{
    return Fs_AppendFileEx(filePath, data, silent);
}

LIB_EXPORT
bool Fs_AppendFile(const fs::path &filePath, const std::vector<uint8_t> &data, bool silent)
{
    return Fs_AppendFileEx(filePath, data, silent);
}
