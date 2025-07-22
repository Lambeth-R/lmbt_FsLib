#include "FsLib.h"

#if defined PLATFORM_WIN
#pragma warning (push)
#pragma warning (disable : 4996)
#else
#include <unistd.h>
#include <dirent.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#define FS_USE_POSIX

#if defined FS_USE_POSIX

namespace fs
{
#if defined PLATFORM_WIN
    struct own_stat
    {
        uint32_t    st_dev;
        uint16_t    st_ino;
        uint16_t    st_mode;
        int16_t    st_nlink;
        int16_t    st_uid;
        int16_t    st_gid;
        uint32_t    st_rdev;
        uint64_t    st_size;
        int64_t    st_atime;
        int64_t    st_mtime;
        int64_t    st_ctime;
    };
    using stat = own_stat;
#else
    using stat = struct stat;
#endif

#ifndef S_IFLNK
#define	S_IFLNK	0120000     /* Symbolic link.  */
#endif
}

namespace
{
    template<typename T, typename V = typename T::value_type>
    bool readFileEx(const fs::path &filePath, T &data, [[maybe_unused]] const bool silent)
    {
        const auto &working_path = filePath.is_absolute() ? filePath : fs::expandPath(filePath);
        auto *file_handle = std::fopen(working_path.string().c_str(), "rb");
        if (!file_handle)
        {
            return false;
        }
        std::fseek(file_handle, 0, SEEK_END);
        const std::size_t file_size =
#if defined PLATFORM_WIN
            _ftelli64(file_handle);
#else
            ftello64(file_handle);
#endif
        const auto size_bytes = file_size / sizeof(V);
        data.resize(size_bytes);
        std::fseek(file_handle, 0, SEEK_SET);
        return std::fread(&data[0], sizeof(V), size_bytes, file_handle) == size_bytes;
    }

    template<typename T, typename V = typename T::value_type>
    bool writeFileEx(const fs::path &filePath, const T &data, const bool force)
    {
        const auto &working_path = filePath.is_absolute() ? filePath : fs::expandPath(filePath);
        auto *file_handle = std::fopen(working_path.string().c_str(), force ? "wb" : "ab");
        if (!file_handle)
        {
            return false;
        }
        MakeScopeGuard([&] {if (file_handle) { std::fclose(file_handle); file_handle = nullptr; }});
        const auto size_bytes = data.size() / sizeof(V);
        return std::fwrite(&data[0], sizeof(V), size_bytes, file_handle) == size_bytes;
    }

    template<typename T, typename V = typename T::value_type>
    bool appendFileEx(const fs::path &filePath, const T &data, [[maybe_unused]] const bool silent)
    {
        const auto &working_path = filePath.is_absolute() ? filePath : fs::expandPath(filePath);
        auto *file_handle = std::fopen(working_path.string().c_str(), "ab");
        if (!file_handle)
        {
            return false;
        }
        MakeScopeGuard([&] {if (file_handle) { std::fclose(file_handle); file_handle = nullptr; }});
        const auto size_bytes = data.size() / sizeof(V);
        return std::fwrite(&data[0], sizeof(V), size_bytes, file_handle) == size_bytes;
    }

    int statsEx(const fs::path &filePath, fs::stat &statRes)
    {
        const auto &working_path = filePath.is_absolute() ? filePath : fs::expandPath(filePath);
#if defined PLATFORM_WIN
        return _stat64(working_path.string().c_str(), reinterpret_cast<struct _stat64*>(&statRes));
#else
        return stat(working_path.string().c_str(), &statRes);
#endif
    }
}

namespace fs::posix
{
    DEFINE_MAIN_OUT_OPS(write, force)
    DEFINE_MAIN_OUT_OPS(append, silent)
    DEFINE_MAIN_IN_OPS(read, silent)

    LIB_EXPORT
    const fs::path expandPath(const fs::path &Path)
    {
#if defined PLATFORM_WIN
        wchar_t 
#else
        char
#endif
            *temp_path = nullptr;
        MakeScopeGuard([&] { if (temp_path) { free(temp_path); temp_path = nullptr; } });
#if defined PLATFORM_WIN
        temp_path = _wfullpath(nullptr, Path.wstring().c_str(), 0);
#else
        temp_path = canonicalize_file_name(Path.string().c_str());
#endif
        return fs::path(temp_path);
    }

    LIB_EXPORT
    bool isExist(const fs::path &Path)
    {
        fs::stat unused_stats;
        const auto res = statsEx(Path, unused_stats);
        return res == 0;
    }

    LIB_EXPORT
    bool isDirectory(const fs::path &Path)
    {
        fs::stat stats;
        const auto res = statsEx(Path, stats);
        return res == 0 && stats.st_mode == S_IFDIR;
    }

    LIB_EXPORT
    bool removeFile(const fs::path &filePath)
    {
        fs::stat stats;
        const auto &working_path = filePath.is_absolute() ? filePath : fs::expandPath(filePath);
        if (working_path.empty())
        {
            return false;
        }
        if (statsEx(working_path, stats) != 0)
        {
            return false;
        }
        if (stats.st_mode == S_IFLNK)
        {
            return unlink(filePath.string().c_str()) == 0;
        }
        else if (stats.st_mode == S_IFMT)
        {
            return remove(filePath.string().c_str()) == 0;
        }
        return false;
    }

    LIB_EXPORT
    bool removeDir(const fs::path &Path, bool recursive)
    {
        const auto &working_path = Path.is_absolute() ? Path : fs::expandPath(Path);
        if (!isExist(working_path))
        {
            return true;
        }
        if (!isDirectory(working_path))
        {
            return false;
        }
        if (recursive)
        {
            bool res = true;
            const auto &folder_contnt = fs::enumDir(working_path);
            for (const auto &it : folder_contnt)
            {
                if (fs::isDirectory(it))
                {
                    res &= fs::removeDir(it, recursive);
                }
                else
                {
                    res &= fs::removeFile(it);
                }
                if (!res)
                {
                    return res;
                }
            }

        }

        const int reuslt =
#if defined PLATFORM_WIN
            _wrmdir(working_path.wstring().c_str());
#else
            rmdir(working_path.string().c_str());
#endif
        return reuslt == 0;
    }

    LIB_EXPORT
    bool createDirectory(const fs::path &Path, bool recirsive)
    {
        const auto &working_path = Path.is_absolute() ? Path : fs::expandPath(Path);
        bool result = true;
        if (!isExist(Path.parent_path()))
        {
            if (!recirsive)
            {
                return false;
            }
            else
            {
               result &= createDirectory(Path.parent_path(), recirsive);
            }
        }
#if defined PLATFORM_WIN64 || defined PLATFORM_WIN32
        result &= _wmkdir(Path.wstring().c_str()) == 0;
#else
        result &= mkdir(Path.string().c_str(),  S_IRWXU | S_IRWXG | S_IRWXO) == 0;
#endif
        return result;
    }

// Winapi lack of a POSIX impl to do so...
#if !defined PLATFORM_WIN
    LIB_EXPORT
    const std::list<fs::path> enumDir(const fs::path &Path, const std::string &regFilter)
    {
        const auto &working_path = Path.is_absolute() ? Path : fs::expandPath(Path);
        if (working_path.empty())
        {
            return {};
        }
        if (!isDirectory(working_path))
        {
            return {};
        }
                  DIR *h_dir   = nullptr;
        struct dirent *it_file = nullptr;
        std::list<fs::path> result;
        if (!(h_dir = opendir(working_path.string().c_str())))
        {
            return {};
        }
        MakeScopeGuard([&] { if (h_dir) { closedir(h_dir); h_dir = nullptr; } });
        while ((it_file = readdir(h_dir)) != NULL)
        {
            std::string file_name(it_file->d_name);
            if ((file_name == "." && file_name.size() == 1) || (file_name == ".." && file_name.size() == 2) ||
               (!regFilter.empty() && !std::regex_match(file_name, std::regex(regFilter))) )
            {
                continue;
            }
            result.push_back(Path / it_file->d_name);
        }
        return result;
    }
#else
    LIB_EXPORT
    const std::list<fs::path> enumDir(const fs::path &Path, const std::string &regFilter)
    {
        return {};
    }
#endif
}

#endif

#if defined PLATFORM_WIN
#pragma warning (pop)
#endif