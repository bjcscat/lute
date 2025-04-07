#include "lute/RuntimeLib.h"
#include "lua.h"
#include "lualib.h"
#include "lute/LuauUtilities.h"
#include "lute/LuteException.h"
#include "lute/Runtime.h"
#include "lute/GlobalRuntime.h"
#include "lute/Scheduler.h"

#include <cstddef>
#include <exception>
#include <future>
#include <memory>
#include <thread>

constexpr auto RUNTIME_TYPENAME = "Runtime";

namespace
{
class RuntimeLuaConfig
{
};

void get_runtime_mt(lua_State* L)
{
    if (luaL_newmetatable(L, RUNTIME_TYPENAME) == 0)
    {
        return; // if exists, return
    }
}
} // namespace

int runtime::create(lua_State* L)
{
    return 0;
}

int runtime::load(lua_State* L)
{
    return 0;
}

int runtime::resume(lua_State* L)
{
    return 0;
}
