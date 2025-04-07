#pragma once

#include "lua.h"

#include "lualib.h"
#include "lute/LuauUtilities.h"
#include "lute/LuteException.h"
#include "lute/Scheduler.h"
#include "lute/Utilities.h"

#include <future>
#include <map>
#include <string>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

using StdVFS = std::map<std::string, const char*>;

constexpr auto REQUIRE_CONTEXT_FIELD = "_lute_require_context";

class StateDeleter
{
public:
    void operator()(lua_State* L) const
    {
        lua_close(L);
    }
};

class GlobalRuntime;

struct RuntimeOptions
{
    using Library = std::pair<const char*, std::variant<std::vector<luaL_Reg>, lua_CFunction>>;

    std::vector<Library> libs;
};

class Runtime;

using SharedRuntimePtr = std::shared_ptr<Runtime>;
using WeakRuntimePtr = std::weak_ptr<Runtime>;
using RuntimeFuture = std::shared_future<LuauFunction>;

class Runtime
{
public:
    /**
     * Constructs a shared runtime pointer to a runtime under the given global runtime and options
     */
    static SharedRuntimePtr create_runtime(const GlobalRuntime*, const RuntimeOptions&);

    /**
     * Loads given luau source with a provided chunkname. Pushes the compiled function into the Luau state.
     */
    void load_source(lua_State* L, const std::string& source, const std::string& chunk_name) const;

    /*
     * Get a reference to the scheduler instance for this runtime
     */
    Scheduler& get_scheduler()
    {
        return scheduler;
    }

    /*
     *Retrieves the Luau state pointer for the main thread of the runtime
     */
    [[nodiscard]] lua_State* get_main_state()
    {
        return main_state.get();
    }

    /*
     * Get a const pointer to the global runtime
     */
    const GlobalRuntime* get_global_runtime()
    {
        return global_runtime;
    }

    /*
     * Set a pointer to an instance of a standard library VFS
     */
    void set_std_vfs(const StdVFS*);

    /*
     * Gets the runtime options used in this runtime
     */
    const RuntimeOptions& get_options()
    {
        return options;
    }

    /**
     * Constructs a shared pointer to the runtime from the internal weak ptr
     */
    static SharedRuntimePtr get_runtime(lua_State* L)
    {
        auto* runtime = static_cast<WeakRuntimePtr*>(lua_getthreaddata(lua_mainthread(L)));

        if (LUTE_UNLIKELY(runtime == nullptr || runtime->expired()))
        {
            throw LuteException{"State has no attached runtime instance"};
        }

        return runtime->lock();
    }

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    ~Runtime();
protected:
    // to permit make_shared creation
    explicit Runtime(const GlobalRuntime* runtime, const RuntimeOptions& options);
private:
    using StatePointer = std::unique_ptr<lua_State, StateDeleter>;

    StatePointer main_state;
    Scheduler scheduler;
    RuntimeOptions options;

    const GlobalRuntime* global_runtime;

    bool use_native_codegen = false;
};
