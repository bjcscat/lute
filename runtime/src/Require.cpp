#include "lute/Require.h"
#include "Luau/Require.h"
#include "lua.h"
#include "lute/LuteException.h"
#include "lute/Runtime.h"
#include <iostream>
#include <string_view>

bool RequireContext::is_require_allowed(lua_State* /*L*/, const char* requirer_chunkname)
{
    std::string_view chunkname = requirer_chunkname;

    bool isStdin = (chunkname == "=stdin");
    bool isFile = (!chunkname.empty() && chunkname[0] == '@');

    constexpr int STD_SIZE = (sizeof("@@std/") - 1);
    bool isStdLibFile = (chunkname.size() >= STD_SIZE && chunkname.substr(0, STD_SIZE) == "@@std/");

    return isStdin || isFile || isStdLibFile;
}

luarequire_NavigateResult RequireContext::reset(lua_State* /*L*/, const char* requirer_chunkname)
{
    std::string_view chunkname = requirer_chunkname;
    if (chunkname == "=stdin")
    {
        current_path = std::move(std::filesystem::current_path() / "stdin");

        return NAVIGATE_SUCCESS;
    }

    if (!chunkname.empty() && chunkname.at(0) == '@')
    {
        current_path = std::move(std::filesystem::current_path() / chunkname.substr(1));

        return NAVIGATE_SUCCESS;
    }

    return NAVIGATE_NOT_FOUND;
}

luarequire_NavigateResult RequireContext::jump_to_alias(lua_State* /*L*/, const char* path)
{
    std::string_view view{path};

    if (view == "$std")
    {
        vfs_type = VFSType::Std;
        current_path = "@std";

        return NAVIGATE_SUCCESS;
    }

    current_path = path;
    add_suffix(current_path);

    return NAVIGATE_SUCCESS;
}

luarequire_NavigateResult RequireContext::to_parent(lua_State* /*L*/)
{
    if (current_path.root_path() == current_path)
    {
        if (at_fake_root)
        {
            return NAVIGATE_NOT_FOUND;
        }

        at_fake_root = true;
        return NAVIGATE_SUCCESS;
    }

    current_path = current_path.parent_path();

    return NAVIGATE_SUCCESS;
}
luarequire_NavigateResult RequireContext::to_child(lua_State* /*L*/, const char* name)
{
    at_fake_root = false;
    current_path /= name;

    if (vfs_type != VFSType::None)
    {
        return NAVIGATE_SUCCESS;
    }

    return add_suffix(current_path);
}

bool RequireContext::is_module_present(lua_State* /*L*/)
{
    if (std_vfs == nullptr || vfs_type == VFSType::Std)
    {
        return std_vfs->count(current_path.filename()) != 0;
    }

    return std::filesystem::is_regular_file(current_path);
}

luarequire_WriteResult RequireContext::get_contents(lua_State* /*L*/, char* buffer, size_t buffer_size, size_t* size_out)
{
    if (vfs_type == VFSType::Std)
    {
        luarequire_WriteResult result = write(std_vfs->at(current_path.filename()), buffer, buffer_size, size_out);

        return result;
    }

    return write_file(current_path, buffer, buffer_size, size_out);
}

luarequire_WriteResult RequireContext::get_chunkname(lua_State* /*L*/, char* buffer, size_t buffer_size, size_t* size_out)
{
    return write("@" + std::string{current_path}, buffer, buffer_size, size_out);
}


luarequire_WriteResult RequireContext::get_cache_key(lua_State* /*L*/, char* buffer, size_t buffer_size, size_t* size_out)
{
    return write(current_path, buffer, buffer_size, size_out);
}

bool RequireContext::is_config_present(lua_State* /*L*/)
{
    return at_fake_root || std::filesystem::exists(current_path / ".luaurc");
}

luarequire_WriteResult RequireContext::get_config(lua_State* /*L*/, char* buffer, size_t buffer_size, size_t* size_out)
{
    if (at_fake_root)
    {
        return write(
            std::string{"{\n"
                        "    \"aliases\": {\n"
                        "        \"std\": \"$std\",\n"
                        "        \"lute\": \"$lute\",\n"
                        "    }\n"
                        "}\n"},
            buffer,
            buffer_size,
            size_out
        );
    }

    return write_file(current_path / ".luaurc", buffer, buffer_size, size_out);
}

int RequireContext::load(lua_State* L, const char* chunkname, const char* contents)
{
    SharedRuntimePtr runtime = Runtime::get_runtime(L);
    lua_State* main_thread = runtime->get_main_state();

    std::string_view chunkname_view{chunkname};

    // lute modules

    if (chunkname_view.rfind("@@lute/", 0) == 0)
    {
        lua_getfield(L, LUA_REGISTRYINDEX, "_MODULES");

        // we know its null terminated
        lua_getfield(L, -1, chunkname_view.substr(7).data()); // NOLINT

        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            lua_pushstring(L, (std::string("no lute library: ") + chunkname_view.substr(1).data()).data());
            lua_error(L);
        }

        return 1;
    }

    // luau modules

    lua_State* module_thread = lua_newthread(main_thread);

    lua_xmove(main_thread, module_thread, 1);

    runtime->load_source(module_thread, contents, chunkname);

    int status = lua_resume(module_thread, L, 0);

    if (status == LUA_OK)
    {
        int results = lua_gettop(module_thread) - 1;

        if (results == 0)
        {
            throw LuteException{"module must return a value"};
        }

        if (results > 1)
        {
            throw LuteException{"module may not return more than one value"};
        }
    }
    else if (status == LUA_YIELD)
    {
        throw LuteException{"module can not yield"};
    }

    lua_xmove(module_thread, L, 1);

    if (status != LUA_OK)
    {
        lua_error(L);
    }

    return 1;
}

void RequireContext::set_vfs(const StdVFS* vfs)
{
    std_vfs = vfs;
}

namespace RequireInternal
{
// Returns whether requires are permitted from the given chunkname.
bool is_require_allowed(lua_State* L, void* ctx, const char* requirer_chunkname)
{
    return static_cast<RequireContext*>(ctx)->is_require_allowed(L, requirer_chunkname);
}

// Resets the internal state to point at the requirer module.
luarequire_NavigateResult reset(lua_State* L, void* ctx, const char* requirer_chunkname)
{
    return static_cast<RequireContext*>(ctx)->reset(L, requirer_chunkname);
}

// Resets the internal state to point at an aliased module, given its exact
// path from a configuration file. This function is only called when an
// alias's path cannot be resolved relative to its configuration file.
luarequire_NavigateResult jump_to_alias(lua_State* L, void* ctx, const char* path)
{
    return static_cast<RequireContext*>(ctx)->jump_to_alias(L, path);
}

// Navigates through the context by making mutations to the internal state.
luarequire_NavigateResult to_parent(lua_State* L, void* ctx)
{
    return static_cast<RequireContext*>(ctx)->to_parent(L);
}

luarequire_NavigateResult to_child(lua_State* L, void* ctx, const char* name)
{
    return static_cast<RequireContext*>(ctx)->to_child(L, name);
}

// Returns whether the context is currently pointing at a module.
bool is_module_present(lua_State* L, void* ctx)
{
    return static_cast<RequireContext*>(ctx)->is_module_present(L);
}

// Provides the contents of the current module. This function is only called
// if is_module_present returns true.
luarequire_WriteResult get_contents(lua_State* L, void* ctx, char* buffer, size_t buffer_size, size_t* size_out)
{
    return static_cast<RequireContext*>(ctx)->get_contents(L, buffer, buffer_size, size_out);
}

// Provides a chunkname for the current module. This will be accessible
// through the debug library. This function is only called if
// is_module_present returns true.
luarequire_WriteResult get_chunkname(lua_State* L, void* ctx, char* buffer, size_t buffer_size, size_t* size_out)
{
    return static_cast<RequireContext*>(ctx)->get_chunkname(L, buffer, buffer_size, size_out);
}

// Provides a cache key representing the current module. This function is
// only called if is_module_present returns true.
luarequire_WriteResult get_cache_key(lua_State* L, void* ctx, char* buffer, size_t buffer_size, size_t* size_out)
{
    return static_cast<RequireContext*>(ctx)->get_cache_key(L, buffer, buffer_size, size_out);
}

// Returns whether a configuration file is present in the current context.
// If not, require-by-string will call to_parent until either a
// configuration file is present or NAVIGATE_FAILURE is returned (at root).
bool is_config_present(lua_State* L, void* ctx)
{
    return static_cast<RequireContext*>(ctx)->is_config_present(L);
}

// Provides the contents of the configuration file in the current context.
// This function is only called if is_config_present returns true.
luarequire_WriteResult get_config(lua_State* L, void* ctx, char* buffer, size_t buffer_size, size_t* size_out)
{
    return static_cast<RequireContext*>(ctx)->get_config(L, buffer, buffer_size, size_out);
}

// Executes the module and places the result on the stack. Returns the
// number of results placed on the stack.
int load(lua_State* L, void* ctx, const char* /*path*/, const char* chunkname, const char* contents)
{
    return static_cast<RequireContext*>(ctx)->load(L, chunkname, contents);
}
} // namespace RequireInternal
