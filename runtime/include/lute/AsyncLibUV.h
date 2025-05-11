#pragma once

#include "lua.h"
#include "lute/LuauUtilities.h"
#include "lute/Runtime.h"
#include "lute/Scheduler.h"
#include "uv.h"
#include <future>
#include <memory>

// NOLINTNEXTLINE
#define TRIVIAL_RUN(fn, ...) \
    void run() override \
    { \
        int err = fn(loop, &req, __VA_ARGS__, async_callback); \
\
        if (is_uv_error(err)) \
        { \
            throw LuteException{uv_strerror(err)}; \
        } \
    }

template<typename BaseUV>
class UVAsyncOperationBase
{
public:
    virtual ~UVAsyncOperationBase() noexcept = default;

    UVAsyncOperationBase(const UVAsyncOperationBase&) = delete;
    UVAsyncOperationBase& operator=(const UVAsyncOperationBase&) = delete;

    UVAsyncOperationBase(UVAsyncOperationBase&&) = default;
    UVAsyncOperationBase& operator=(UVAsyncOperationBase&&) = default;

    static void async_callback(uv_fs_t* fs_req)
    {
        auto* operation = static_cast<UVAsyncOperationBase*>(fs_req->data);

        if (operation != nullptr)
        {
            operation->dispatch_callback();
        }
    }

    lua_State* get_state()
    {
        return runtime->get_main_state();
    }

    std::shared_future<LuauFunction> get_future()
    {
        return future;
    }

protected:
    explicit UVAsyncOperationBase(lua_State* L)
        : runtime{Runtime::get_runtime(L)}
        , L{L}
        , promise{new RuntimePromise()}
        , loop(Runtime::get_runtime(L)->get_scheduler().get_uv_loop())
        , future{promise->get_future().share()}

    {
        req.data = this;
    }

    virtual void run() = 0;
    virtual void dispatch_callback() = 0;

    // the loop
    uv_loop_t* loop;
    // the state
    lua_State* L;

    // the shared runtime pointer
    SharedRuntimePtr runtime;
    // the promise send to the runtime
    std::unique_ptr<RuntimePromise> promise;
    // the future
    RuntimeFuture future;
    // the base libuv request type
    BaseUV req{};
};
