#pragma once

#include <functional>
#include <memory>

#include "lute/AsyncLibUV.h"
#include "uv.h"

template<typename Derived>
class AsyncFSOperation : public UVAsyncOperationBase<uv_fs_t>
{
private:
    using Callback = std::function<void(std::unique_ptr<Derived>)>;

    friend Derived;

    explicit AsyncFSOperation(lua_State* L, Callback callback_fn)
        : UVAsyncOperationBase{L}
        , callback{std::move(callback_fn)}
    {
    }

    // NOLINTBEGIN
    AsyncFSOperation(const AsyncFSOperation&) = delete;
    AsyncFSOperation& operator=(const AsyncFSOperation&) = delete;

    AsyncFSOperation(AsyncFSOperation&&) = default;
    AsyncFSOperation& operator=(AsyncFSOperation&&) = default;
    // NOLINTEND
public:
    ~AsyncFSOperation<Derived>() override
    {
        uv_fs_req_cleanup(&req);
    }

    template<typename... Args>
    static std::unique_ptr<Derived> create(Args&&... args)
    {
        return std::make_unique<Derived>(std::forward<Args>(args)...);
    }

    static void execute_and_schedule(std::unique_ptr<Derived> derived)
    {
        Runtime::get_runtime(derived->get_state())
            ->get_scheduler()
            .schedule_future(LuauThreadFuture{derived->get_state(), execute(std::move(derived))});
    }

    static RuntimeFuture execute(std::unique_ptr<Derived> derived)
    {
        Derived* pointer = derived.release();

        pointer->run();

        return pointer->get_future();
    }

    uv_fs_t* get_raw()
    {
        return &req;
    }

    void complete(LuauFunction continuation)
    {
        promise->set_value(std::move(continuation));
    }

    void error(LuteException error)
    {
        promise->set_exception(std::make_exception_ptr(std::move(error)));
    }

    void dispatch_callback() override
    {
        if (callback)
        {
            lua_State* lua_state = runtime->get_main_state();

            callback(std::unique_ptr<Derived>{static_cast<Derived*>(this)});
        }
    }

    ssize_t get_result()
    {
        return req.result;
    }

protected:
    Callback callback;
};
