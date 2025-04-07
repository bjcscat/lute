#include <array>
#include <functional>
#include <iterator>
#include <vector>

#include "lute/fs.h"
#include "lua.h"
#include "lualib.h"
#include "lute/error.h"
#include "lute/libuv.h"
#include "lute/runtime.h"
#include "lute/scheduler.h"
#include "lute/utils.h"

#include "uv.h"
#include "uv/unix.h"

class AsyncFSOperation : public AsyncOperation<uv_fs_t> // NOLINT
{
public:
    AsyncFSOperation(lua_State* L, Scheduler& sched)
        : AsyncOperation<uv_fs_t>(L, sched)
    {
    }
    ~AsyncFSOperation() override
    {
        uv_fs_req_cleanup(raw());
    }
};

constexpr int BUFFER_SIZE = 1024;

// NOLINTBEGIN
struct ReadContext : public AsyncFSOperation
{
    std::vector<char> storage;
    uv_buf_t iov{};
    int fd = 0;
    std::array<char, BUFFER_SIZE> buf{};

    ReadContext(lua_State* L, Scheduler& sched)
        : AsyncFSOperation{L, sched}
    {
    }

    void cleanup()
    {
        uv_fs_req_cleanup(raw());
    }

    ~ReadContext() override {};
};
// NOLINTEND

namespace
{
void readFileCallback(ReadContext* ctx) // NOLINT
{
    ssize_t bytesRead = ctx->raw()->result;

    if (bytesRead == 0)
    {
        ctx->resumeDelete(
            [ctx](lua_State* L)
            {
                lua_pushlstring(L, ctx->storage.data(), ctx->storage.size());

                return 1;
            }
        );

        return;
    }

    if (UV_ISERR(bytesRead))
    {
        ctx->resumeErrorDelete(LuteException{uv_strerror((int)bytesRead)});
        return;
    }

    for (int i = 0; i < bytesRead; i++)
    {
        ctx->storage.push_back(ctx->buf.at(i));
    }

    ctx->runUV(
        uv_fs_read,
        [=](AsyncOperation<uv_fs_t>*)
        {
            readFileCallback(ctx);
        },
        ctx->fd,
        &(ctx->iov),
        1,
        -1
    );
}
} // namespace

int fs::readfile(lua_State* L)
{
    Runtime* runtime = Runtime::getRuntime(L);
    const char* path = luaL_checkstring(L, 1);

    auto* readContext = new ReadContext(L, runtime->getScheduler());

    readContext->runUV(
        uv_fs_open,
        [readContext](AsyncOperation<uv_fs_t>*)
        {
            readContext->fd = (int)readContext->raw()->result;
            readContext->iov = uv_buf_init(readContext->buf.data(), readContext->buf.size());

            if (UV_ISERR(readContext->fd))
            {
                readContext->resumeErrorDelete(LuteException{uv_strerror(readContext->fd)});
                return;
            }

            readContext->cleanup(); // cleanup the open operation

            readContext->runUV(
                uv_fs_read,
                [readContext](AsyncOperation<uv_fs_t>*)
                {
                    readFileCallback(readContext);
                },
                readContext->fd,
                &readContext->iov,
                1,
                -1
            );
        },
        path,
        O_RDONLY,
        0
    );

    return lua_yield(L, 0);
}

int luteopen_fs(lua_State* L)
{
    lua_createtable(L, 0, std::size(fs::fs));

    luaL_register(L, nullptr, static_cast<const luaL_Reg*>(fs::fs));

    return 1;
}
