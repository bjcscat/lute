#pragma once

#include <array>
#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#if !defined(S_ISCHR) && defined(S_IFMT) && defined(S_IFCHR)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#endif

#if !defined(S_ISLNK) && defined(S_IFMT) && defined(S_IFLNK)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif

#if !defined(S_ISFIFO) && defined(S_IFMT) && defined(S_IFIFO)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif


namespace fs
{
constexpr auto UV_TYPENAME_UNKNOWN = "unknown"; // UV_DIRENT_UNKNOWN
constexpr auto UV_TYPENAME_FILE = "file";       // UV_DIRENT_FILE
constexpr auto UV_TYPENAME_DIR = "dir";         // UV_DIRENT_DIR
constexpr auto UV_TYPENAME_LINK = "link";       // UV_DIRENT_LINK
constexpr auto UV_TYPENAME_FIFO = "fifo";       // UV_DIRENT_FIFO
constexpr auto UV_TYPENAME_SOCKET = "socket";   // UV_DIRENT_SOCKET
constexpr auto UV_TYPENAME_CHAR = "char";       // UV_DIRENT_CHAR
constexpr auto UV_TYPENAME_BLOCK = "block";     // UV_DIRENT_BLOCK

constexpr std::array UV_DIRENT_TYPES = {
    UV_TYPENAME_UNKNOWN,
    UV_TYPENAME_FILE,
    UV_TYPENAME_DIR,
    UV_TYPENAME_LINK,
    UV_TYPENAME_FIFO,
    UV_TYPENAME_SOCKET,
    UV_TYPENAME_CHAR,
    UV_TYPENAME_BLOCK,
};
} // namespace fs
