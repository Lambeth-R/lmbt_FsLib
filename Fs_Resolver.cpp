#include "FsLib.h"

#if defined IS_CPP_20G
#define DEFINE_RESOLVER_OUT_OPS_20(name_ , _arg) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::span<const std::byte> &data, const bool _arg) \
        DEFINE_FUNCTION_RESOLVED_BODY(name_,File,_arg);
#else
#define DEFINE_RESOLVER_OUT_OPS_20(name_ , _arg);
#endif

#if defined IS_CPP_17G
#define DEFINE_RESOLVER_OUT_OPS_17(name_ , _arg) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::string_view data, const bool _arg)  \
        DEFINE_FUNCTION_RESOLVED_BODY(name_,File,_arg); \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::wstring_view data, const bool _arg ) \
        DEFINE_FUNCTION_RESOLVED_BODY(name_,File,_arg);
#elif
#define DEFINE_RESOLVER_OUT_OPS_17(name_ , _arg);
#endif

#define DEFINE_RESOLVER_IN_OPS_14( name_ , _arg ) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::vector<uint8_t> &data, const bool _arg)  \
        DEFINE_FUNCTION_RESOLVED_BODY(name_, File, _arg);                                               \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::string &data, const bool _arg)           \
        DEFINE_FUNCTION_RESOLVED_BODY(name_, File, _arg);                                               \
    LIB_EXPORT bool name_##File(const fs::path &filePath, std::wstring &data, const bool _arg )         \
        DEFINE_FUNCTION_RESOLVED_BODY(name_, File, _arg);

#define DEFINE_RESOLVER_OUT_OPS_14( name_ , _arg ) \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::vector<uint8_t> &data, const bool _arg)    \
        DEFINE_FUNCTION_RESOLVED_BODY(name_,File,_arg);                                                         \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::string &data, const bool _arg)             \
        DEFINE_FUNCTION_RESOLVED_BODY(name_,File,_arg);                                                         \
    LIB_EXPORT bool name_##File(const fs::path &filePath, const std::wstring &data, const bool _arg)            \
        DEFINE_FUNCTION_RESOLVED_BODY(name_,File,_arg);

#define DEFINE_RESOLVER_OUT_OPS(name_ , _arg)    \
        DEFINE_RESOLVER_OUT_OPS_14(name_ , _arg) \
        DEFINE_RESOLVER_OUT_OPS_17(name_ , _arg) \
        DEFINE_RESOLVER_OUT_OPS_20(name_ , _arg)

#define DEFINE_BODY_FS() \
    DEFINE_RESOLVER_OUT_OPS(write, force)   \
    DEFINE_RESOLVER_OUT_OPS(append, silent) \
    DEFINE_RESOLVER_IN_OPS_14(read, silent) \
    LIB_EXPORT const fs::path               expandPath(const fs::path &Path)    \
        { return FS_APENDIX::expandPath(Path); } \
    LIB_EXPORT bool                         removeFile(const fs::path &filePath)    \
        { return FS_APENDIX::removeFile(filePath); } \
    LIB_EXPORT bool                         removeDir(const fs::path &Path, const bool recursive)    \
        { return FS_APENDIX::removeDir(Path, recursive); } \
    LIB_EXPORT bool                         isDirectory(const fs::path &Path)   \
        { return FS_APENDIX::isDirectory(Path); } \
    LIB_EXPORT bool                         isExist(const fs::path &Path)   \
        { return FS_APENDIX::isExist(Path); } \
    LIB_EXPORT bool                         createDirectory(const fs::path &Path, const bool recirsive) \
        { return FS_APENDIX::createDirectory(Path, recirsive); } \
    LIB_EXPORT const std::list<fs::path>    enumDir(const fs::path &Path, const std::string &regFilter) \
        { return FS_APENDIX::enumDir(Path, regFilter); }

namespace fs
{
    DEFINE_BODY_FS()
}