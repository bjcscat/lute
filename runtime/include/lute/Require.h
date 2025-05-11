#pragma once

#include "lua.h"
#include "lute/Runtime.h"
#include "Luau/Require.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>

class RequireContext
{
public:
    using fs_path = std::filesystem::path;

    bool is_require_allowed(lua_State* L, const char* requirer_chunkname);

    luarequire_NavigateResult reset(lua_State* L, const char* requirer_chunkname);

    luarequire_NavigateResult jump_to_alias(lua_State* L, const char* path);

    luarequire_NavigateResult to_parent(lua_State* L);
    luarequire_NavigateResult to_child(lua_State* L, const char* name);

    bool is_module_present(lua_State* L);
    luarequire_WriteResult get_contents(lua_State* L, char* buffer, size_t buffer_size, size_t* size_out);
    luarequire_WriteResult get_chunkname(lua_State* L, char* buffer, size_t buffer_size, size_t* size_out);

    luarequire_WriteResult get_cache_key(lua_State* L, char* buffer, size_t buffer_size, size_t* size_out);

    bool is_config_present(lua_State* L);

    luarequire_WriteResult get_config(lua_State* L, char* buffer, size_t buffer_size, size_t* size_out);

    int load(lua_State* L, const char* chunkname, const char* contents);

    // Sets the pointer for the standard library virtual file system utilized by this require context
    void set_vfs(const StdVFS* vfs);
private:
    static luarequire_WriteResult write(const std::string& source, char* buffer, size_t buffer_size, size_t* size_out)
    {
        size_t null_terminated_size = source.size() + 1;

        *size_out = null_terminated_size;

        if (null_terminated_size > buffer_size)
        {
            return WRITE_BUFFER_TOO_SMALL;
        }

        std::memcpy(buffer, source.c_str(), source.size());

        return WRITE_SUCCESS;
    }

    static luarequire_WriteResult write_file(const fs_path& path, char* buffer, size_t buffer_size, size_t* size_out)
    {
        std::ifstream file{};
        file.open(path);

        if (!file.is_open())
        {
            return WRITE_FAILURE;
        }

        *size_out = size_t(file.readsome(buffer, std::streamsize(buffer_size)));

        if (file.peek() == EOF)
        {
            return WRITE_SUCCESS;
        }

        return WRITE_BUFFER_TOO_SMALL;
    }

    static luarequire_NavigateResult add_suffix(fs_path& path)
    {
        bool found = false;

        fs_path temp = path;

        if (std::filesystem::is_directory(temp))
        {
            temp /= "init";
        }

        const char* working_extension = nullptr;

        for (const char* candiate : {"luau", "lua"})
        {
            if (std::filesystem::is_regular_file(temp.replace_extension(candiate)))
            {
                if (found)
                {
                    return luarequire_NavigateResult::NAVIGATE_AMBIGUOUS;
                }

                working_extension = candiate;
                found = true;
            }
        }
        if (!found)
        {
            return luarequire_NavigateResult::NAVIGATE_NOT_FOUND;
        }

        temp.replace_extension(working_extension);

        if (!std::filesystem::is_regular_file(temp))
        {
            return luarequire_NavigateResult::NAVIGATE_NOT_FOUND;
        }

        path = std::move(temp);

        return luarequire_NavigateResult::NAVIGATE_SUCCESS;
    }

    enum class VFSType : uint8_t
    {
        Std,
        None,
    };

    bool at_fake_root = false;
    VFSType vfs_type = VFSType::None;
    const StdVFS* std_vfs;
    fs_path current_path;
};

namespace RequireInternal
{
// Returns whether requires are permitted from the given chunkname.
bool is_require_allowed(lua_State* L, void* ctx, const char* requirer_chunkname);

// Resets the internal state to point at the requirer module.
luarequire_NavigateResult reset(lua_State* L, void* ctx, const char* requirer_chunkname);

// Resets the internal state to point at an aliased module, given its exact
// path from a configuration file. This function is only called when an
// alias's path cannot be resolved relative to its configuration file.
luarequire_NavigateResult jump_to_alias(lua_State* L, void* ctx, const char* path);

// Navigates through the context by making mutations to the internal state.
luarequire_NavigateResult to_parent(lua_State* L, void* ctx);
luarequire_NavigateResult to_child(lua_State* L, void* ctx, const char* name);

// Returns whether the context is currently pointing at a module.
bool is_module_present(lua_State* L, void* ctx);

// Provides the contents of the current module. This function is only called
// if is_module_present returns true.
luarequire_WriteResult get_contents(lua_State* L, void* ctx, char* buffer, size_t buffer_size, size_t* size_out);

// Provides a chunkname for the current module. This will be accessible
// through the debug library. This function is only called if
// is_module_present returns true.
luarequire_WriteResult get_chunkname(lua_State* L, void* ctx, char* buffer, size_t buffer_size, size_t* size_out);

// Provides a cache key representing the current module. This function is
// only called if is_module_present returns true.
luarequire_WriteResult get_cache_key(lua_State* L, void* ctx, char* buffer, size_t buffer_size, size_t* size_out);

// Returns whether a configuration file is present in the current context.
// If not, require-by-string will call to_parent until either a
// configuration file is present or NAVIGATE_FAILURE is returned (at root).
bool is_config_present(lua_State* L, void* ctx);

// Provides the contents of the configuration file in the current context.
// This function is only called if is_config_present returns true.
luarequire_WriteResult get_config(lua_State* L, void* ctx, char* buffer, size_t buffer_size, size_t* size_out);

// Executes the module and places the result on the stack. Returns the
// number of results placed on the stack.
int load(lua_State* L, void* ctx, const char* path, const char* chunkname, const char* contents);
} // namespace RequireInternal

const luarequire_Configuration GLOBAL_REQUIRE_CONFIGURATION = {
    RequireInternal::is_require_allowed,
    RequireInternal::reset,
    RequireInternal::jump_to_alias,
    RequireInternal::to_parent,
    RequireInternal::to_child,
    RequireInternal::is_module_present,
    RequireInternal::get_contents,
    RequireInternal::get_chunkname,
    RequireInternal::get_cache_key,
    RequireInternal::is_config_present,
    RequireInternal::get_config,
    RequireInternal::load
};
