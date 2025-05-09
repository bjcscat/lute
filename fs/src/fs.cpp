#include "lute/fs.h"
#include "lualib.h"
#include "lute/AsyncLibUV.h"
#include "lute/LuteException.h"
#include "lute/UserdataTags.h"
#include "lute/Utilities.h"
#include "lua.h"
#include "uv.h"

#include "types.h"
#include "async.h"

#include <array>
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <string>
#include <string_view>

constexpr int DEFAULT_FILE_MODE = 0660;
constexpr int DEFAULT_BUFFER_SIZE = 512;

namespace
{
class AsyncOpenOperation : public AsyncFSOperation<AsyncOpenOperation>
{
public:
    explicit AsyncOpenOperation(lua_State* L, const char* path, int flags, Callback callback_fn)
        : AsyncFSOperation<AsyncOpenOperation>{L, std::move(callback_fn)}
        , path{path}
        , flags{flags}
    {
    }

    TRIVIAL_RUN(uv_fs_open, path, flags, DEFAULT_FILE_MODE);

private:
    int flags;
    const char* path;
};

/**
 * Performs a write operation asynchronously with additional state for tracking the read
 */
class AsyncReadOperation : public AsyncFSOperation<AsyncReadOperation>
{
public:
    explicit AsyncReadOperation(lua_State* L, uv_file file_descriptor, int offset, Callback callback_fn)
        : AsyncFSOperation<AsyncReadOperation>{L, std::move(callback_fn)}
        , file_descriptor{file_descriptor}
        , offset{offset}
    {
        buf = uv_buf_init(buffer->data(), buffer->size());
    }

    TRIVIAL_RUN(uv_fs_read, file_descriptor, &buf, 1, offset);

    void append_buffer(int num)
    {
        offset += num;
        complete_string.append(buffer->data(), num);
    }

    std::string get_string_result()
    {
        return std::move(complete_string);
    }

private:
    int offset;

    uv_file file_descriptor;
    std::string complete_string;

    uv_buf_t buf{};
    std::unique_ptr<std::array<char, DEFAULT_BUFFER_SIZE>> buffer = std::make_unique<std::array<char, DEFAULT_BUFFER_SIZE>>();
};

/**
 * Performs a write operation asynchronously with additional state for tracking the write
 */
class AsyncWriteOperation : public AsyncFSOperation<AsyncWriteOperation>
{
public:
    explicit AsyncWriteOperation(lua_State* L, uv_file file_descriptor, int offset, std::string_view source, Callback callback_fn)
        : AsyncFSOperation<AsyncWriteOperation>{L, std::move(callback_fn)}
        , source{source}
        , file_descriptor{file_descriptor}
        , remaining_bytes{source.length()}
        , offset{offset}
    {
        buf = uv_buf_init(const_cast<char*>(this->source.data()), this->source.length()); // NOLINT
    }

    TRIVIAL_RUN(uv_fs_write, file_descriptor, &buf, 1, 0);

    size_t remaining_bytes;

private:
    uv_file file_descriptor;
    uv_buf_t buf{};
    int offset;
    std::string_view source;
};

/**
 * Handles recursive file read operations
 */
void read_callback(std::unique_ptr<AsyncReadOperation> read_op)
{
    int result = int(read_op->get_result());

    if (is_uv_error(result))
    {
        read_op->error(LuteException{uv_strerror(result)});

        return;
    }

    read_op->append_buffer(result);

    if (result == 0)
    {
        read_op->complete(
            [result = read_op->get_string_result()](lua_State* L)
            {
                lua_pushlstring(L, result.c_str(), result.length());
                return 1;
            }
        );

        return;
    }

    AsyncReadOperation::execute(std::move(read_op));
}

void write_callback(std::unique_ptr<AsyncWriteOperation> write_op)
{
    ssize_t result = write_op->get_result();
    if (is_uv_error(result))
    {
        write_op->error(LuteException{uv_strerror(int(result))});
        return;
    }

    if (write_op->remaining_bytes <= result)
    {
        // done
        write_op->complete(
            [](lua_State*)
            {
                return 0;
            }
        );
        return;
    }

    write_op->remaining_bytes -= result;

    AsyncWriteOperation::execute(std::move(write_op));
}

struct File
{
    static constexpr auto FILE_HANDLE_TYPE_NAME = "file";

    uv_file fd; // NOLINT
    int err = -1;

    explicit File(uv_file descriptor)
        : fd(descriptor)
    {
    } // NOLINT

    /**
     * Checks the value at `narg` for being a File and returns it, otherwise produces a type error
     */
    static File check_file_handle(lua_State* L, int narg)
    {
        auto* handle = static_cast<File*>(lua_touserdatatagged(L, narg, userdata_tags::FILE_HANDLE));

        if (handle == nullptr)
        {
            luaL_typeerror(L, narg, FILE_HANDLE_TYPE_NAME); // doesnt return
        }

        return *handle;
    }

    /**
     * Pushes a File struct to the stack.

     *`lua_pushvalue` should be used for cases where a file is not new to preserve the descriptors correct
     * behavior
     */
    void push(lua_State* L) const
    {
        *static_cast<File*>(lua_newuserdatatagged(L, sizeof(File), userdata_tags::FILE_HANDLE)) = *this;

        push_file_handle_metatable(L);
        lua_setmetatable(L, -2);
    }

private:
    static void push_file_handle_metatable(lua_State* L)
    {
        if (luaL_newmetatable(L, FILE_HANDLE_TYPE_NAME) == 0)
        {
            return;
        }

        // metatable

        lua_pushstring(L, FILE_HANDLE_TYPE_NAME);
        lua_setfield(L, -2, "__type");

        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        // methods

        lua_pushcfunction(L, File::read, "read");
        lua_setfield(L, -2, "read");

        lua_pushcfunction(L, File::write, "write");
        lua_setfield(L, -2, "write");
    }

    static int read(lua_State* L)
    {
        auto [fd, err] = File::check_file_handle(L, 1);

        std::unique_ptr<AsyncReadOperation> read_op = AsyncReadOperation::create(L, fd, 0, read_callback);

        AsyncReadOperation::execute_and_schedule(std::move(read_op));

        return lua_yield(L, 0);
    }

    static int write(lua_State* L)
    {
        auto [fd, err] = File::check_file_handle(L, 1);
        std::string_view source{luaL_checkstring(L, 2)};

        AsyncWriteOperation::execute_and_schedule(AsyncWriteOperation::create(L, fd, 0, source, write_callback));

        return lua_yield(L, 0);
    }
};

inline LuteException invalid_specifer(char specifier)
{
    return LuteException{std::string("Invalid access specifier '") + specifier + '\''};
}

unsigned int get_mode(const std::string_view mode_string)
{
    unsigned int mode = 0;

    if (mode_string.length() < 3)
    {
        switch (mode_string[0])
        {
        case 'r':
            mode |= O_RDONLY;
            break;
        case 'w':
            mode |= O_WRONLY | O_TRUNC;
            break;
        case 'a':
            mode |= O_WRONLY | O_APPEND;
            break;
        default:
            throw invalid_specifer(mode_string[0]);
        };

        if (mode_string.length() == 2)
        {
            switch (mode_string[1])
            {
            case '+':
                // r+, a+, w+
                mode |= O_RDWR;
                break;
            case 'x':
                // exclusive mode
                if (mode_string[0] == 'w')
                {
                    mode |= O_EXCL;
                    break;
                }
            default:
                throw invalid_specifer(mode_string[1]);
            }
        }
    }

    if (mode_string.length() == 3)
    {
        switch (mode_string[3])
        {
        case 'x':
            // exclusive mode
            if (mode_string.substr(0, 1) == "w+")
            {
                mode |= O_EXCL;
                break;
            }
        default:
            throw invalid_specifer(mode_string[1]);
        }
    }

    if (mode_string.length() > 3)
    {
        throw invalid_specifer(mode_string[3]);
    }

    return mode;
}
} // namespace

int fs::open(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const char* mode = luaL_checkstring(L, 2);

    std::unique_ptr<AsyncOpenOperation> open = AsyncOpenOperation::create(
        L,
        path,
        get_mode(mode),
        [](std::unique_ptr<AsyncOpenOperation> open_op)
        {
            int result = int(open_op->get_result());

            if (is_uv_error(result))
            {
                open_op->error(LuteException{uv_strerror(result)});

                return;
            }

            open_op->complete(
                [result](lua_State* L)
                {
                    File handle{result};

                    handle.push(L);

                    return 1;
                }
            );
        }
    );

    AsyncOpenOperation::execute_and_schedule(std::move(open));

    return lua_yield(L, 0);
}

int fs::readfile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);

    std::unique_ptr<AsyncOpenOperation> open_op = AsyncOpenOperation::create(
        L,
        path,
        O_CREAT | O_RDONLY,
        [](std::unique_ptr<AsyncOpenOperation> open_op)
        {
            std::unique_ptr<AsyncReadOperation> operation =
                AsyncReadOperation::create(open_op->get_state(), uv_file(open_op->get_result()), 0, read_callback);

            AsyncReadOperation::execute_and_schedule(std::move(operation));
        }
    );

    AsyncOpenOperation::execute(std::move(open_op));

    return lua_yield(L, 0);
}

int fs::writefile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const char* source = luaL_checkstring(L, 2);

    std::unique_ptr<AsyncOpenOperation> open_op = AsyncOpenOperation::create(
        L,
        path,
        O_CREAT | O_WRONLY,
        [source = std::string_view(source)](std::unique_ptr<AsyncOpenOperation> open_op)
        {
            int result = int(open_op->get_result());

            if (is_uv_error(result))
            {
                open_op->error(LuteException{uv_strerror(result)});
                return;
            }

            std::unique_ptr<AsyncWriteOperation> write_op = AsyncWriteOperation::create(
                open_op->get_state(),
                uv_file(result),
                0,
                source,
                [](std::unique_ptr<AsyncWriteOperation> write)
                {
                    ssize_t result = write->get_result();
                    if (is_uv_error(result))
                    {
                        write->error(LuteException{uv_strerror(int(result))});
                        return;
                    }

                    if (write->remaining_bytes <= result)
                    {
                        // done
                        write->complete(
                            [](lua_State*)
                            {
                                return 0;
                            }
                        );
                        return;
                    }

                    write->remaining_bytes -= result;
                }
            );

            AsyncWriteOperation::execute(std::move(write_op));
        }
    );

    AsyncOpenOperation::execute(std::move(open_op));

    return lua_yield(L, 0);
}

int fs::type(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);

    uv_fs_t req;

    int err = uv_fs_stat(uv_default_loop(), &req, path, nullptr);

    if (is_uv_error(err))
    {
        throw LuteException{uv_strerror(err)};
    }

    if (S_ISDIR(req.statbuf.st_mode))
    {
        lua_pushstring(L, UV_TYPENAME_DIR);
    }
    else if (S_ISREG(req.statbuf.st_mode))
    {
        lua_pushstring(L, UV_TYPENAME_FILE);
    }
    else if (S_ISCHR(req.statbuf.st_mode))
    {
        lua_pushstring(L, UV_TYPENAME_CHAR);
    }
    else if (S_ISLNK(req.statbuf.st_mode))
    {
        lua_pushstring(L, UV_TYPENAME_LINK);
    }
#ifdef S_ISBLK
    else if (S_ISBLK(req.statbuf.st_mode))
    {
        lua_pushstring(L, UV_TYPENAME_BLOCK);
    }
#endif
#ifdef S_ISFIFO
    else if (S_ISFIFO(req.statbuf.st_mode))
    {
        lua_pushstring(L, UV_TYPENAME_FIFO);
    }
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(req.statbuf.st_mode))
    {
        lua_pushstring(L, UV_TYPENAME_SOCKET);
    }
#endif
    else
    {
        lua_pushstring(L, UV_TYPENAME_UNKNOWN);
    }


    uv_fs_req_cleanup(&req);

    return 1;
}


int fs::listdir(lua_State* L)
{
    class AsyncListDirOperation : public AsyncFSOperation<AsyncListDirOperation>
    {
    public:
        explicit AsyncListDirOperation(lua_State* L, const char* path, Callback callback)
            : AsyncFSOperation<AsyncListDirOperation>{L, std::move(callback)}
            , path{path}
        {
        }

        TRIVIAL_RUN(uv_fs_scandir, path, 0);

    private:
        const char* path;
    };

    const char* path = luaL_checkstring(L, 1);

    std::unique_ptr<AsyncListDirOperation> operation = AsyncListDirOperation::create(
        L,
        path,
        [](std::unique_ptr<AsyncListDirOperation> operation)
        {
            uv_dirent_t dirent;
            int err = 0;
            lua_State* L = operation->get_state();

            lua_createtable(L, 0, 0);

            int idx = 0;
            while (!is_uv_error(err = uv_fs_scandir_next(operation->get_raw(), &dirent)))
            {
                lua_pushinteger(L, ++idx);

                lua_createtable(L, 0, 2);

                lua_pushstring(L, dirent.name);
                lua_setfield(L, -2, "name");


                lua_pushstring(L, fs::UV_DIRENT_TYPES.at(dirent.type));
                lua_setfield(L, -2, "type");

                lua_settable(L, -3);
            }

            int ref = lua_ref(L, -1);

            if (err != UV_EOF)
            {
                // fail
                operation->error(LuteException{uv_strerror(err)});
                return;
            }

            operation->complete(
                [ref](lua_State* L)
                {
                    lua_getref(L, ref);
                    lua_unref(L, ref);

                    return 1;
                }
            );
        }
    );

    AsyncListDirOperation::execute_and_schedule(std::move(operation));

    return lua_yield(L, 0);
}
