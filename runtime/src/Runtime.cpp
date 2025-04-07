#include "lute/Runtime.h"
#include "lua.h"
#include "luacodegen.h"
#include "lualib.h"
#include "Luau/Compiler.h"
#include "lute/LuauUtilities.h"
#include "lute/LuteException.h"
#include "lute/Require.h"

#include "Luau/Require.h"
#include "lute/UserdataTags.h"
#include "lute/Utilities.h"

#include <string>
#include <variant>
#include <vector>

namespace
{
void setup_require(lua_State* L)
{
    lua_setuserdatadtor(L, userdata_tags::REQUIRE_CONTEXT, templated_luau_destructor<RequireContext>);

    auto* require_context = new (lua_newuserdatatagged(L, sizeof(RequireContext), userdata_tags::REQUIRE_CONTEXT)) RequireContext();

    // setup require context
    lua_setfield(L, LUA_REGISTRYINDEX, REQUIRE_CONTEXT_FIELD);

    // setup module table
    lua_createtable(L, 0, 0);
    lua_setfield(L, LUA_REGISTRYINDEX, "_MODULES");

    luaopen_require(
        L,
        [](luarequire_Configuration* init)
        {
            std::memcpy(init, &GLOBAL_REQUIRE_CONFIGURATION, sizeof(luarequire_Configuration));
        },
        require_context
    );
}

void load_libs(lua_State* L, const RuntimeOptions& options)
{

    lua_getfield(L, LUA_REGISTRYINDEX, "_MODULES");

    for (auto [name, lib] : options.libs)
    {
        lua_pushcfunction(L, luarequire_registermodule, "registermodel");
        lua_pushstring(L, name);
        if (const auto* lib_ptr = std::get_if<std::vector<luaL_Reg>>(&lib))
        {
            lua_createtable(L, 0, int(lib_ptr->size()));

            for (const auto& reg : *lib_ptr)
            {
                lua_pushcfunction(L, reg.func, reg.name);
                lua_setfield(L, -2, reg.name);
            }
        }
        else
        {
            std::get<lua_CFunction>(lib)(L);
        }
        lua_call(L, 2, 0);
    }

    lua_pop(L, 1);
}

template<typename... Arg>
SharedRuntimePtr create(Arg&&... arg)
{
    struct EnableMakeShared : public Runtime
    {
        explicit EnableMakeShared(Arg&&... arg)
            : Runtime(std::forward<Arg>(arg)...)
        {
        }
    };
    return std::make_shared<EnableMakeShared>(std::forward<Arg>(arg)...);
}

} // namespace

SharedRuntimePtr Runtime::create_runtime(const GlobalRuntime* global_runtime, const RuntimeOptions& options)
{
    SharedRuntimePtr runtime_ptr = create(global_runtime, options);

    lua_setthreaddata(runtime_ptr->get_main_state(), new WeakRuntimePtr(runtime_ptr));

    return runtime_ptr;
}

// private constructor
Runtime::Runtime(const GlobalRuntime* global_runtime, const RuntimeOptions& in_options)
    : main_state{luaL_newstate()}
    , global_runtime{global_runtime}
    , options{in_options}
{
    lua_State* L = get_main_state();

    // open base luau libs
    luaL_openlibs(L);

    setup_require(L);

    load_libs(L, in_options);
}

Runtime::~Runtime() {
    delete static_cast<WeakRuntimePtr*>(lua_getthreaddata(get_main_state()));
}

namespace
{
RequireContext* get_require_context(lua_State* L)
{
    int member_type = lua_getfield(L, LUA_REGISTRYINDEX, "_lute_require_context");

    if (LUTE_UNLIKELY(member_type != LUA_TUSERDATA))
    {
        throw LuteException{"Missing require context"};
    }

    auto* ctx = static_cast<RequireContext*>(lua_touserdatatagged(L, -1, userdata_tags::REQUIRE_CONTEXT));

    if (LUTE_UNLIKELY(ctx == nullptr))
    {
        throw LuteException{"Invalid require context"};
    }

    lua_pop(L, 1); // pop the userdata

    return ctx;
}
} // namespace

void Runtime::set_std_vfs(const StdVFS* vfs)
{
    get_require_context(get_main_state())->set_vfs(vfs);
}

void Runtime::load_source(lua_State* L, const std::string& source, const std::string& chunk_name) const
{
    std::string bytecode = Luau::compile(source);

    int err = luau_load(L, chunk_name.c_str(), bytecode.c_str(), bytecode.size(), 0);

    if (use_native_codegen)
    {
        luau_codegen_compile(L, -1);
    }

    if (err != 0)
    {
        const char* error = lua_tostring(L, -1);
        lua_pop(L, 1);
        throw LuteException{error};
    }
}
