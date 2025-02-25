#pragma once
#include "../../Include/Common.h"

#ifdef _MSC_VER
#include <Windows.h>
#else

#endif

#include <string>
#include <vector>
#if _CPP_VERSION >= 201703L
#include <filesystem>
#elif _CPP_VERSION >= 201402L
#define INCLUDE_STD_FILESYSTEM_EXPERIMENTAL 1
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
#else
#error Unsupported.
#endif

namespace fs 
{
#if _CPP_VERSION >= 201703L
    using path = std::filesystem::path;
#else
    using path = std::experimental::filesystem::v1::path;
#endif
#ifdef _MSC_VER

    struct FileMetadata
    {
        typedef union
        {
            uint64_t i;
            FILETIME ft;
        }var;
        var CreationTime;
        var LastAccessTime;
        var LastWriteTime;
        var ChangeTime;
        uint32_t FileAttributes;
    };
#endif
}

LIB_EXPORT
const fs::path Fs_GetMountPointPath(const fs::path &Path);

LIB_EXPORT
const fs::path Fs_ExpandPath(const fs::path &Path);

LIB_EXPORT
bool Fs_WriteFile(const fs::path &filePath, const std::string &data, bool force = false);

LIB_EXPORT
bool Fs_WriteFile(const fs::path &filePath, const std::wstring &data, bool force = false);

LIB_EXPORT
bool Fs_WriteFile(const fs::path &filePath, const std::vector<uint8_t> &data, bool force = false);

LIB_EXPORT
bool Fs_ReadFile(const fs::path &filePath, std::string &data, bool silent = false);

LIB_EXPORT
bool Fs_ReadFile(const fs::path &filePath, std::wstring &data, bool silent = false);

LIB_EXPORT
bool Fs_ReadFile(const fs::path &filePath, std::vector<uint8_t> &data, bool silent = false);

LIB_EXPORT
bool Fs_AppendFile(const fs::path &filePath, const std::string &data, bool silent = false);

LIB_EXPORT
bool Fs_AppendFile(const fs::path &filePath, const std::wstring &data, bool silent = false);

LIB_EXPORT
bool Fs_AppendFile(const fs::path &filePath, const std::vector<uint8_t> &data, bool silent = false);

LIB_EXPORT
bool Fs_DeleteFile(const fs::path &filePath);

LIB_EXPORT
bool Fs_RemoveDir(const fs::path &Path, bool recursive = true);

LIB_EXPORT
bool Fs_IsDirectory(const fs::path &Path);

LIB_EXPORT
bool Fs_IsExist(const fs::path &Path);

LIB_EXPORT
bool Fs_CreateDirectory(const fs::path &Path, bool recirsive = true);

LIB_EXPORT
const std::vector<fs::path> Fs_EnumDir(const fs::path& Path, const std::wstring& regFilter = {}); // add filter

#ifdef _MSC_VER
LIB_EXPORT
bool Fs_GetFileMetadata(const fs::path &Path, fs::FileMetadata &attributes);

LIB_EXPORT
bool Fs_SetFileMetadata(const fs::path &Path, const fs::FileMetadata &attributes);
#endif