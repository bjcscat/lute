#pragma once

#include "lualib.h"

#include <array>
#include <map>
#include <string>

#define LUTE_DECLARE_TYPE(...)

#define LUTE_DECLARE_FUNCTION(lib, funcname, ...) \
    namespace lib \
    { \
    int funcname(lua_State*); \
    };


#define LUTE_DECLARE_LIB_INIT(lib) \
    namespace lib \
    { \
    int lua_init(lua_State*); \
    };

#define LUTE_LIB_ENTRY(funcname) luaL_Reg{#funcname, funcname},

#define LUTE_DEFINE_LIB(libname, ...) \
    namespace libname \
    { \
    const static std::array lib{__VA_ARGS__}; \
    };
