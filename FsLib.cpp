#include "../Include/FsLib.h"
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
#include <userenv.h>
#if _HAS_CXX17
#include <string_view>
#endif
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Userenv.lib")
#pragma warning(disable : 5051)

const fs::path Fs_GetMountPointPath(const fs::path& path)
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
    wchar_t dev_path[50]; // 50 is max
    const auto& letter = expanded_path.wstring().substr(0, 3);
    if (!GetVolumeNameForVolumeMountPointW(letter.c_str(), dev_path, 50))
    {
        return expanded_path;
    }
    return fs::path(dev_path) / expanded_path.wstring().substr(3);
}

const fs::path Fs_ExpandPath(const fs::path& path)
{
    if (!path.is_relative())
    {
        return path;
    }
    HANDLE self_process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, false, GetCurrentProcessId());
    if (!self_process_handle && self_process_handle == INVALID_HANDLE_VALUE)
    {
        return path;
    }
    HANDLE self_token_handle = INVALID_HANDLE_VALUE;
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) {
        if (self_token_handle && self_token_handle != INVALID_HANDLE_VALUE) { CloseHandle(self_token_handle); self_token_handle = nullptr; }
        if (self_process_handle && self_process_handle != INVALID_HANDLE_VALUE) { CloseHandle(self_process_handle); self_process_handle = nullptr; }}};
    if (!self_process_handle || self_process_handle == INVALID_HANDLE_VALUE)
    {
        return path;
    }
    BOOL res = OpenProcessToken(self_process_handle, TOKEN_IMPERSONATE | TOKEN_QUERY, &self_token_handle);
    if (!res || !self_token_handle || self_token_handle == INVALID_HANDLE_VALUE)
    {
        return path;
    }
    std::unique_ptr<wchar_t[]> tmp_buf(new wchar_t[MAX_PATH - 1]);
    res = ExpandEnvironmentStringsForUserW(self_token_handle, path.c_str(), tmp_buf.get(), MAX_PATH - 1);
    if (res)
    {
        return tmp_buf.get();
    }
    return path;
}

bool Fs_WriteFile(const fs::path& filePath, const std::string& data, bool force)
{
    fs::path expanded_path = Fs_GetMountPointPath(filePath);
    DWORD attributes = force ? CREATE_ALWAYS : CREATE_NEW;
    auto h_File = CreateFileW(expanded_path.c_str(), GENERIC_WRITE, 0, nullptr, attributes, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!h_File || h_File == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }}};
    DWORD dw_written = 0;
    if (!WriteFile(h_File, data.data(), static_cast<DWORD>(data.size()), &dw_written, nullptr) || dw_written == 0)
    {
        return false;
    }
    return true;
}

bool Fs_ReadFile(const fs::path& filePath, std::string& data)
{
    fs::path expanded_path = Fs_GetMountPointPath(filePath);
    auto h_File = CreateFileW(expanded_path.c_str(), GENERIC_READ, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!h_File || h_File == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }}};
    LARGE_INTEGER li_Size;
    if (!GetFileSizeEx(h_File, &li_Size))
    {
        return false;
    }
    DWORD dw_read = 0;
    DWORD dw_size = static_cast<DWORD>(li_Size.QuadPart);
    std::unique_ptr<char[]> tmp_buf(new char[li_Size.QuadPart]);
    if (!ReadFile(h_File, tmp_buf.get(), dw_size, &dw_read, nullptr))
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
    fs::path expanded_path = Fs_GetMountPointPath(filePath);
    auto h_File = CreateFileW(expanded_path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!h_File || h_File == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }}};
    LARGE_INTEGER li_Size;
    if (!GetFileSizeEx(h_File, &li_Size))
    {
        return false;
    }
    DWORD dw_written = 0;
    if (!WriteFile(h_File, data.data(), static_cast<DWORD>(data.size()), &dw_written, nullptr) || dw_written == 0)
    {
        return false;
    }
    return true;
}

bool Fs_DeleteFile(const fs::path& filePath)
{
    fs::path expanded_path = Fs_GetMountPointPath(filePath);
    if (!DeleteFileW(expanded_path.c_str()))
    {
        return false;
    }
    return true;
}

bool Fs_RemoveDir(const fs::path& path, bool recursive)
{
    fs::path expanded_path = Fs_GetMountPointPath(path);
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
    const auto& expanded_path = Fs_GetMountPointPath(path);
    if (GetFileAttributesW(expanded_path.c_str()) & FILE_ATTRIBUTE_DIRECTORY)
    {
        return true;
    }
    return false;
}

bool Fs_IsExist(const fs::path& path)
{
    fs::path expanded_path = Fs_GetMountPointPath(path);
    DWORD dw_attrib = GetFileAttributesW(expanded_path.c_str());
    bool exist = (dw_attrib != INVALID_FILE_ATTRIBUTES && (dw_attrib & FILE_ATTRIBUTE_DIRECTORY));
    if (!exist)
    {
        auto h_File = CreateFileW(expanded_path.c_str(), FILE_READ_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!h_File || h_File == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }}};
        exist = true;
    }
    return exist;
}

bool Fs_CreateDirectory(const fs::path& path, bool recirsive)
{
    fs::path expanded_path = Fs_GetMountPointPath(path);
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
    fs::path expanded_path = Fs_GetMountPointPath(path);
    auto h_File = CreateFileW(expanded_path.c_str(), FILE_READ_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!h_File || h_File == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }}};
    if (!GetFileTime(h_File, &attributes.CreationTime, &attributes.LastAccessTime, &attributes.LastWriteTime))
    {
        return false;
    }
    FILE_ATTRIBUTE_TAG_INFO atrrib_tag = {};
    if (!GetFileInformationByHandleEx(h_File, FILE_INFO_BY_HANDLE_CLASS::FileAttributeTagInfo, &atrrib_tag, sizeof(FILE_ATTRIBUTE_TAG_INFO)))
    {
        return false;
    }
    attributes.FileAttributes = atrrib_tag.FileAttributes;
    return true;
}

bool Fs_SetFileMetadata(const fs::path& path, const fs::File_Metadata& attributes)
{
    fs::path expanded_path = Fs_GetMountPointPath(path);
    auto h_File = CreateFileW(expanded_path.c_str(), FILE_WRITE_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!h_File || h_File == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (h_File && h_File != INVALID_HANDLE_VALUE) { CloseHandle(h_File); h_File = nullptr; }}};
    FILE_ATTRIBUTE_TAG_INFO atrrib_tag = {};
    atrrib_tag.FileAttributes = attributes.FileAttributes;
    if (!GetFileInformationByHandleEx(h_File, FILE_INFO_BY_HANDLE_CLASS::FileAttributeTagInfo, &atrrib_tag, sizeof(FILE_ATTRIBUTE_TAG_INFO)))
    {
        return false;
    }
    if (!SetFileTime(h_File, &attributes.CreationTime, &attributes.LastAccessTime, &attributes.LastWriteTime))
    {
        return false;
    }
    return true;
}

const std::vector<fs::path> Fs_EnumDir(const fs::path& path)
{
    fs::path expanded_path = Fs_GetMountPointPath(path);
    if (!Fs_IsDirectory(expanded_path))
    {
        expanded_path = expanded_path.parent_path();
    }
    bool ret = true;
    std::vector<fs::path> ret_vec;
    const fs::path file_pattern = expanded_path / L"*";
    WIN32_FIND_DATAW file_info = {};
    auto h_File = FindFirstFileW(file_pattern.c_str(), &file_info);
    std::shared_ptr<void> _{nullptr, [&]([[maybe_unused]] void* ptr) { if (h_File && h_File != INVALID_HANDLE_VALUE) { FindClose(h_File); h_File = nullptr; }}};
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
        ret_vec.push_back(file_info.cFileName);
    } while (FindNextFileW(h_File, &file_info));
    return ret_vec;
}