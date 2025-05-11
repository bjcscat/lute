#include "lute/Scheduler.h"
#include "uv.h"
#include "lua.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

Scheduler::Scheduler()
    : loop{new uv_loop_t}
{
    uv_loop_init(loop.get());
}

void Scheduler::scheduler_local_resumptions_step()
{
    for (auto& future : local_futures.dump())
    {
        if (!future.is_ready())
        {
            this->local_futures.push(std::move(future));
            continue;
        }

        int status = future.execute();

        if (status == LUA_YIELD)
        {
            continue;
        }

        if (status == LUA_OK)
        {
            continue;
        }

        error_callback(future.get_state());
    }
}

void Scheduler::scheduler_uv_step()
{
    uv_run(get_uv_loop(), UV_RUN_DEFAULT);
}

void Scheduler::scheduler_shared_resumptions_step()
{
    shared_futures.lock();
    for (auto& future : shared_futures.dump())
    {
        if (!future.is_ready())
        {
            this->local_futures.push(std::move(future));
            continue;
        }

        int status = future.execute();

        if (status == LUA_YIELD)
        {
            continue;
        }

        if (status == LUA_OK)
        {
            continue;
        }
    }
    shared_futures.unlock();
}

void Scheduler::run()
{
    bool doing_work = true;

    while (doing_work || active_handles > 0)
    {
        if (local_futures.has_work())
        {
            scheduler_local_resumptions_step();
        }
        else
        {
            doing_work = false;
        }

        scheduler_uv_step();

        if (shared_futures.has_work())
        {
            doing_work = true;
            scheduler_shared_resumptions_step();
        }
        else
        {
            doing_work = local_futures.has_work();
        }

        if (!doing_work && active_handles > 0) {
            std::unique_lock lock {barrier.interrupt_mutex};
            std::cout << "entered await\n";

            barrier.conditional.wait(lock, [this]() {
                std::cerr << "CHECK\n";
                return barrier.has_work;
            });

            std::cout << "signaled\n";

            barrier.has_work = false;


            lock.unlock();
            barrier.conditional.notify_one();

            // std::this_thread::yield();
        }
    }
}
