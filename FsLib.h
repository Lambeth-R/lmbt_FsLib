#pragma once
#include "../../Include/Common.h"

#define FS_USE_POSIX

#if defined PLATFORM_WIN
#define FS_USE_WINAPI
#include <Windows.h>
#endif

#if (!defined FS_USE_POSIX) && (!defined FS_USE_WINAPI)
#error Define FS environment
#endif

#include <algorithm>
#include <string>
#include <list>
#include <vector>
#include <regex>
#if defined IS_CPP_17G
#include <filesystem>
#include <string_view>
#else
#error Unsupported.
#endif

#if defined FS_USE_WINAPI
#define FS_APENDIX winapi
#else
#define FS_APENDIX posix
#endif

#define DEFINE_FUNCTION_RESOLVED_BODY( pref_ , post_ , arg_ ) \
    { return FS_APENDIX::pref_##post_(filePath, data, arg_); }

#if defined IS_CPP_20G
#define DEFINE_HEAD_OUT_OPS_20( name_ , _arg ) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::span<const std::byte> &data, const bool _arg = false);

#define DEFINE_MAIN_OUT_OPS_20(name_ , _arg) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::span<const std::byte> &data, const bool _arg) \
        { return name_##FileEx(filePath, data, _arg); }
#else
#define DEFINE_HEAD_OUT_OPS_20( name_ , _arg );
#define DEFINE_MAIN_OUT_OPS_20( name_ , _arg );
#endif

#if defined IS_CPP_17G
#define DEFINE_HEAD_OUT_OPS_17( name_ , _arg ) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::string_view data, const bool _arg = false);  \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::wstring_view data, const bool _arg = false);

#define DEFINE_MAIN_OUT_OPS_17(name_ , _arg)            \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::string_view data, const bool _arg)  \
        { return name_##FileEx(filePath, data, _arg); } \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::wstring_view data, const bool _arg) \
        { return name_##FileEx(filePath, data, _arg); }
#else
#define DEFINE_HEAD_OUT_OPS_17( name_ , _arg );
#define DEFINE_MAIN_OUT_OPS_17( name_ , _arg );
#endif

#define DEFINE_MAIN_OUT_OPS( name_ , _arg ) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::vector<uint8_t> &data, bool _arg)  \
        { return name_##FileEx(filePath, data, _arg); }                                                 \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::string &data, const bool _arg)     \
        { return name_##FileEx(filePath, data, _arg); }                                                 \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::wstring &data, const bool _arg)    \
        { return name_##FileEx(filePath, data, _arg); }                                                 \
    DEFINE_MAIN_OUT_OPS_17(name_ , _arg) \
    DEFINE_MAIN_OUT_OPS_20(name_ , _arg)

#define DEFINE_MAIN_IN_OPS( name_ , _arg ) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::vector<uint8_t> &data, const bool _arg)  \
        { return name_##FileEx(filePath, data, _arg); }                                                 \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::string &data, const bool _arg)           \
        { return name_##FileEx(filePath, data, _arg); }                                                 \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::wstring &data, const bool _arg)          \
        { return name_##FileEx(filePath, data, _arg); }

#define DEFINE_HEAD_OUT_OPS_14( name_ , _arg ) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::vector<uint8_t> &data, const bool _arg = false);   \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::string &data, const bool _arg = false);            \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::wstring &data, const bool _arg = false);           \

#define DEFINE_HEAD_IN_OPS_14( name_ , _arg ) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::vector<uint8_t> &data, const bool _arg = false);   \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::string &data, const bool _arg = false);            \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::wstring &data, const bool _arg = false);           \

#define DEFINE_HEAD_OUT_OPS(name_ , _arg)    \
        DEFINE_HEAD_OUT_OPS_14(name_ , _arg) \
        DEFINE_HEAD_OUT_OPS_17(name_ , _arg) \
        DEFINE_HEAD_OUT_OPS_20(name_ , _arg)

#define DEFINE_COMMON_FS() \
        DEFINE_HEAD_OUT_OPS(write, force)       \
        DEFINE_HEAD_OUT_OPS(append, silent)     \
        DEFINE_HEAD_IN_OPS_14(read, silent)     \
        LIB_EXPORT const fs::path               expandPath(const fs::path &Path);       \
        LIB_EXPORT bool                         removeFile(const fs::path &filePath);   \
        LIB_EXPORT bool                         removeDir(const fs::path &Path, const bool recursive = true);       \
        LIB_EXPORT bool                         isDirectory(const fs::path &Path);      \
        LIB_EXPORT bool                         isExist(const fs::path &Path);          \
        LIB_EXPORT bool                         createDirectory(const fs::path &Path, const bool recirsive = true); \
        LIB_EXPORT const std::list<fs::path>    enumDir(const fs::path &Path, const std::string &regFilter = {});


namespace fs
{
    using path = std::filesystem::path;
#if defined PLATFORM_WIN
    struct file_metadata
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
    // platform specific api
    namespace winapi
    {
        LIB_EXPORT const fs::path   getMountPointPath(const fs::path &Path);
        LIB_EXPORT bool             getFileMetadata(const fs::path &Path, fs::file_metadata &attributes);
        LIB_EXPORT bool             setFileMetadata(const fs::path &Path, const fs::file_metadata &attributes);
        DEFINE_COMMON_FS()
    };
#endif
    namespace posix
    {
        DEFINE_COMMON_FS()
    };
    DEFINE_COMMON_FS()
}