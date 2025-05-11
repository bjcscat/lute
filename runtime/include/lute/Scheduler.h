#pragma once

#include "lute/LuauUtilities.h"
#include "lute/LuteException.h"
#include "uv.h"

#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <lua.h>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility>
#include <vector>

using RuntimePromise = std::promise<LuauFunction>;

/**
 * Class for handling Luau resumptions
 */
class LuauThreadFuture : public std::shared_future<LuauFunction>
{
public:
    explicit LuauThreadFuture(lua_State* L, std::shared_future<LuauFunction> res)
        : L{L}
        , std::shared_future<LuauFunction>{std::move(res)}
    {
        lua_pushthread(L);
        refIdx = lua_ref(L, -1); // NOLINT
        lua_pop(L, 1);
    }

    int execute()
    {
        int nargs = 0;
        try
        {
            LuauFunction func = get();

            nargs = func(L);
        }
        catch (const LuteException& e)
        {
            lua_pushstring(L, e.what());
            return lua_resumeerror(L, nullptr);
        }

        return lua_resume(L, nullptr, nargs);
    }

    bool is_ready()
    {
        return wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    lua_State* get_state()
    {
        return L;
    }

private:
    int refIdx = 0;
    lua_State* L;
};

class ResumptionQueue
{
public:
    std::optional<LuauThreadFuture> take()
    {
        if (resumptions.empty())
        {
            return std::nullopt;
        }

        LuauThreadFuture task{std::move(resumptions.back())};

        resumptions.pop_back();

        return task;
    }

    void push(LuauThreadFuture resumption)
    {
        resumptions.push_back(std::move(resumption));
    }

    [[nodiscard]] bool has_work() const
    {
        return !resumptions.empty();
    }

    std::vector<LuauThreadFuture> dump()
    {
        std::vector<LuauThreadFuture> futures{std::move(resumptions)};

        resumptions.clear();

        return std::move(futures);
    }

private:
    std::vector<LuauThreadFuture> resumptions;
};

class MutexResumptionQueue
{
public:
    void lock()
    {
        mutex.lock();
    }

    /*
     * Pushes a resumption onto the queue. Not thread-safe and requires synchronization with `lock`
     */
    void push(LuauThreadFuture resumption)
    {
        has_work_flag = true;
        resumptions.push_back(std::move(resumption));
    }

    std::optional<LuauThreadFuture> take()
    {
        if (resumptions.empty())
        {
            return std::nullopt;
        }

        LuauThreadFuture task{std::move(resumptions.back())};

        resumptions.pop_back();

        if (resumptions.empty())
        {
            has_work_flag = false;
        }

        return task;
    }

    void unlock()
    {
        mutex.unlock();
    }

    /**
     * Returns if the queue has any work inside it to perform. Thread-safe
     */
    bool has_work()
    {
        return has_work_flag;
    }

    /**
     * Dumps the vector in the queue, not thread safe
     */
    std::vector<LuauThreadFuture> dump()
    {
        has_work_flag = false;
        return std::move(resumptions);
    }

private:
    std::atomic_bool has_work_flag = false; // flags that the resumptions vector is non-empty without requiring a lock to determine that
    std::mutex mutex;
    std::vector<LuauThreadFuture> resumptions;
};

class InterruptBarrier
{
public:
    /**
     * Called in worker threads as well as main threads to signal to the primary thread that work is completed or there is something to do
     */
    void signal()
    {
        std::cerr << "signaling the presense of work\n";
        {
            std::lock_guard lock{interrupt_mutex};
            has_work = true;
        }

        conditional.notify_one();

        std::cerr << "signaled the presense of work\n";

        {
            std::unique_lock lock{interrupt_mutex};
            conditional.wait(
                lock,
                [this]()
                {
                    return !has_work;
                }
            );
        }
    }

    /**
     * Resets the has_work flag
     */
    void reset()
    {
        std::lock_guard lock{interrupt_mutex};
        has_work = false;
    }

    std::unique_lock<std::mutex> get_lock()
    {
        return std::unique_lock{interrupt_mutex};
    }

// private:
    bool has_work = false;
    std::mutex interrupt_mutex;
    std::condition_variable conditional;
};

class Scheduler
{
public:
    Scheduler();

    void run();

    void schedule_future(LuauThreadFuture info)
    {
        local_futures.push(std::move(info));
    }

    void remote_schedule_future()
    {
        barrier.signal();
    }

    uv_loop_t* get_uv_loop()
    {
        return loop.get();
    }

    void add_handle()
    {
        active_handles++;
    }

    void remove_handle()
    {
        active_handles--;
    }

    void set_error_callback(LuauFunction callback)
    {
        error_callback = std::move(callback);
    }

private:
    void scheduler_local_resumptions_step();
    void scheduler_uv_step();
    void scheduler_shared_resumptions_step();

    class UVLoopDeleter
    {
    public:
        void operator()(uv_loop_t* loop)
        {
            int err = uv_loop_close(loop);

            if (err == 0)
            {
                delete loop;
            }
        }
    };

    /**
     * Number of active handles to the scheduler. Used to keep the scheduler running even when no work is present due to an existing task like a web
     * server
     */
    int active_handles = 0;

    /**
     * Error callback invoked upon each error
     */
    LuauFunction error_callback;

    std::unique_ptr<uv_loop_t, UVLoopDeleter> loop;

    /**
     * Thread-local futures queue for handling resumptions emitted from the thread where this scheduler is primarily running
     */
    ResumptionQueue local_futures;

    /**
     * Queue which uses a mutex to synchronize writes from across threads
     */
    MutexResumptionQueue shared_futures;

    /**
     * Blocks primary thread from running whilst there is no work to be done
     */
    InterruptBarrier barrier;
};
