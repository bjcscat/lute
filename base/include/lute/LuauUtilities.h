#pragma once

#include "lua.h"
#include "lute/LuteException.h"

#include <functional>

using LuauFunction = std::function<int(lua_State*)>;

static int invoke_upvalue_function(lua_State* L)
{
    auto* func = static_cast<LuauFunction*>(lua_touserdata(L, lua_upvalueindex(1)));

    return (*func)(L);
}

inline void push_cpp_function(lua_State* L, LuauFunction func, const char* debugname)
{
    new (lua_newuserdata(L, sizeof(LuauFunction))) LuauFunction(std::move(func));
    lua_pushcclosure(L, invoke_upvalue_function, debugname, 1);
}

inline void copy_cross_vms(lua_State* from_state, lua_State* to_state, int n)
{
    for (;n > 0;n--)
    {
        int index = -n;
        switch (lua_type(from_state, index))
        {
        case LUA_TNIL:
            lua_pushnil(to_state);
            break;
        case LUA_TNUMBER:
            lua_pushnumber(to_state, lua_tonumber(from_state, index));
            break;
        case LUA_TSTRING:
        {
            size_t len = 0;
            const char* str = lua_tolstring(from_state, index, &len);
            lua_pushlstring(to_state, str, len);
            break;
        }
        default:
            throw LuteException{"Incompatible value for resume"};
        }
    }
}

template<typename T>
void templated_luau_destructor([[maybe_unused]] lua_State* L, void* target)
{
    static_cast<T*>(target)->~T();
}
