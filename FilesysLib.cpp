#include "../Include/FilesysLib.h"
#include "../Include/Common.h"
#include "../Include/LogClass.h"
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include <algorithm>
#include <Shlwapi.h>
#if _HAS_CXX17
#include <string_view>
#endif
#pragma comment(lib, "Shlwapi.lib")

const fs::path Fs_GetDosStylePath(const fs::path& path)
{
#if _HAS_CXX17
    if (std::wstring_view(path.c_str()).substr(0, 4) == L"\\\\?\\")
    {
        return path;
    }
#else
    if (std::wstring(path.c_str()).substr(0, 4) == L"\\\\?\\")
    {
        return path;
    }
#endif
    const auto &expanded_path = Fs_ExpandPath(path);
    wchar_t DevPath[50]; // 50 is max
    fs::path abs_path;
    const auto& letter = expanded_path.wstring().substr(0, 3);
    if (!GetVolumeNameForVolumeMountPointW(letter.c_str(), DevPath, 50))
    {
        return expanded_path;
    }
    return fs::path(DevPath) / expanded_path.wstring().substr(3);
}

const fs::path Fs_ExpandPath(const fs::path& path)
{
    if (!path.is_relative())
    {
        return path;
    }
    auto dw_len = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    std::unique_ptr<wchar_t[]> tmp_buf(new wchar_t[dw_len + 1]);
    dw_len = GetFullPathNameW(path.c_str(), dw_len, tmp_buf.get(), nullptr);
    if (dw_len)
    {
        return tmp_buf.get();
    }
    return path;
}

bool Fs_WriteFile(const fs::path& filePath, const std::string& data, bool force)
{
    fs::path expanded_path = Fs_GetDosStylePath(filePath);
    DWORD attributes = force ? CREATE_ALWAYS : CREATE_NEW;
    auto hFile = CreateFileW(expanded_path.c_str(), GENERIC_WRITE, 0, nullptr, attributes, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile)
    {
        return false;
    }
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (hFile && hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = nullptr; }}};
    DWORD dw_written = 0;
    if (!WriteFile(hFile, data.data(), static_cast<DWORD>(data.size()), &dw_written, nullptr) || dw_written == 0)
    {
        return false;
    }
    return true;
}

bool Fs_ReadFile(const fs::path& filePath, std::string& data)
{
    fs::path expanded_path = Fs_GetDosStylePath(filePath);
    auto hFile = CreateFileW(expanded_path.c_str(), GENERIC_READ, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile || hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (hFile && hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = nullptr; }}};
    LARGE_INTEGER liSize;
    if (!GetFileSizeEx(hFile, &liSize))
    {
        return false;
    }
    DWORD dw_read = 0;
    DWORD dw_size = static_cast<DWORD>(liSize.QuadPart);
    std::unique_ptr<char[]> tmp_buf(new char[liSize.QuadPart]);
    if (!ReadFile(hFile, tmp_buf.get(), dw_size, &dw_read, nullptr))
    {
        return false;
    }
    data.clear();
    data.resize(dw_read);
    memcpy((void*)(data.data()), (void*)tmp_buf.get(), dw_read);
    return true;
}

bool Fs_AppendFile(const fs::path& filePath, const std::string& data)
{
    fs::path expanded_path = Fs_GetDosStylePath(filePath);
    auto hFile = CreateFileW(expanded_path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile)
    {
        return false;
    }
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (hFile && hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = nullptr; }}};
    LARGE_INTEGER liSize;
    if (!GetFileSizeEx(hFile, &liSize))
    {
        return false;
    }
    DWORD dw_written = 0;
    if (!WriteFile(hFile, data.data(), static_cast<DWORD>(data.size()), &dw_written, nullptr) || dw_written == 0)
    {
        return false;
    }
    return true;
}

bool Fs_DeleteFile(const fs::path& filePath)
{
    fs::path expanded_path = Fs_GetDosStylePath(filePath);
    if (!DeleteFileW(expanded_path.c_str()))
    {
        return false;
    }
    return true;
}

bool Fs_RemoveDir(const fs::path& path, bool recursive)
{
    fs::path expanded_path = Fs_GetDosStylePath(path);
    if (!Fs_IsDirectory(expanded_path))
    {
        expanded_path = expanded_path.parent_path();
    }
    bool ret = true;
    if (recursive)
    {
        auto dir_data = Fs_EnumDir(expanded_path);
        for (auto& it : dir_data)
        {
            const fs::path it_expanded = expanded_path / it;
            if (Fs_IsDirectory(it_expanded.c_str()))
            {
                Fs_RemoveDir(it_expanded.c_str());
                continue;
            }
            if (!DeleteFileW(it_expanded.c_str()))
            {
                ret = false;
                break;
            }
        }
    }
    if (!RemoveDirectoryW(expanded_path.c_str()))
    {
        return false;
    }
    return ret;
}

bool Fs_IsDirectory(const fs::path& path)
{
    const auto& expanded_path = Fs_GetDosStylePath(path);
    if (GetFileAttributesW(expanded_path.c_str()) & FILE_ATTRIBUTE_DIRECTORY)
    {
        return true;
    }
    return false;
}

bool Fs_IsExist(const fs::path& path)
{
    fs::path expanded_path = Fs_GetDosStylePath(path);
    DWORD dwAttrib = GetFileAttributesW(expanded_path.c_str());
    bool exist = (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    if (!exist)
    {
        auto hFile = CreateFileW(expanded_path.c_str(), FILE_READ_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!hFile || hFile == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (hFile && hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = nullptr; }}};
        exist = true;
    }
    return exist;
}

bool Fs_CreateDirectory(const fs::path& path, bool recirsive)
{
    fs::path expanded_path = Fs_GetDosStylePath(path);
    while(!Fs_IsExist(expanded_path.parent_path()) && recirsive)
    {
        if (!Fs_CreateDirectory(expanded_path.parent_path()))
        {
            return false;
        }
    }
    if (!CreateDirectoryW(expanded_path.c_str(), {}))
    {
        return false;
    }
    return true;
}

bool Fs_GetFileMetadata(const fs::path& path, fs::File_Metadata& attributes)
{
    fs::path expanded_path = Fs_GetDosStylePath(path);
    auto hFile = CreateFileW(expanded_path.c_str(), FILE_READ_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile || hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (hFile && hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = nullptr; }}};
    if (!GetFileTime(hFile, &attributes.CreationTime, &attributes.LastAccessTime, &attributes.LastWriteTime))
    {
        return false;
    }
    FILE_ATTRIBUTE_TAG_INFO atrrib_tag = {};
    if (!GetFileInformationByHandleEx(hFile, FILE_INFO_BY_HANDLE_CLASS::FileAttributeTagInfo, &atrrib_tag, sizeof(FILE_ATTRIBUTE_TAG_INFO)))
    {
        return false;
    }
    attributes.FileAttributes = atrrib_tag.FileAttributes;
    return true;
}

bool Fs_SetFileMetadata(const fs::path& path, const fs::File_Metadata& attributes)
{
    fs::path expanded_path = Fs_GetDosStylePath(path);
    auto hFile = CreateFileW(expanded_path.c_str(), FILE_WRITE_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile || hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (hFile && hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = nullptr; }}};
    FILE_ATTRIBUTE_TAG_INFO atrrib_tag = {};
    atrrib_tag.FileAttributes = attributes.FileAttributes;
    if (!GetFileInformationByHandleEx(hFile, FILE_INFO_BY_HANDLE_CLASS::FileAttributeTagInfo, &atrrib_tag, sizeof(FILE_ATTRIBUTE_TAG_INFO)))
    {
        return false;
    }
    if (!SetFileTime(hFile, &attributes.CreationTime, &attributes.LastAccessTime, &attributes.LastWriteTime))
    {
        return false;
    }
    return true;
}

const std::vector<fs::path> Fs_EnumDir(const fs::path& path)
{
    fs::path expanded_path = Fs_GetDosStylePath(path);
    if (!Fs_IsDirectory(expanded_path))
    {
        expanded_path = expanded_path.parent_path();
    }
    bool ret = true;
    std::vector<fs::path> ret_vec;
    const fs::path file_pattern = expanded_path / L"*";
    WIN32_FIND_DATAW fileInfo = {};
    auto hFile = FindFirstFileW(file_pattern.c_str(), &fileInfo);
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (hFile && hFile != INVALID_HANDLE_VALUE) { FindClose(hFile); hFile = nullptr; }}};
    // Skipping /. && /..
#if _HAS_CXX17
    while (std::wstring_view(fileInfo.cFileName) == L"." || std::wstring_view(fileInfo.cFileName) == L"..")
#else
    while (std::wstring(fileInfo.cFileName) == L"." || std::wstring(fileInfo.cFileName) == L"..")
#endif
    {
        if (!FindNextFileW(hFile, &fileInfo))
        {
            ret = false;
            break;
        }
    }
    do
    {
        ret_vec.push_back(fileInfo.cFileName);
    } while (FindNextFileW(hFile, &fileInfo));
    return ret_vec;
}