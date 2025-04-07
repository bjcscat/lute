#pragma once

#include "lua.h"

#include <string_view>

class TableBuilder
{
public:
    explicit TableBuilder(lua_State* L)
        : ref{create_referenced_table(L)}
        , L{L}
    {
    }

    ~TableBuilder()
    {
        lua_unref(L, ref);
    }

    void append(double number)
    {
        push_table();

        lua_pushnumber(L, number);
        lua_pushinteger(L, lua_objlen(L, -2) + 1);
        lua_settable(L, -3);

        lua_pop(L, -1);
    }

    void append(std::string_view src)
    {
        push_table();

        lua_pushlstring(L, src.data(), src.length());
        lua_pushinteger(L, lua_objlen(L, -2) + 1);
        lua_settable(L, -3);

        lua_pop(L, -1);
    }

    void append(const TableBuilder& builder)
    {
        push_table();

        builder.push_table(L);
        lua_pushinteger(L, lua_objlen(L, -2) + 1);
        lua_settable(L, -3);

        lua_pop(L, -1);
    }

    void set_field(std::string_view field, double n)
    {
        push_table();

        lua_pushnumber(L, n);
        lua_pushlstring(L, field.data(), field.length());
        lua_settable(L, -3);

        lua_pop(L, -1);
    }

    void set_field(std::string_view field, std::string_view src)
    {
        push_table();

        lua_pushlstring(L, src.data(), src.length());
        lua_pushlstring(L, field.data(), field.length());
        lua_settable(L, -3);

        lua_pop(L, -1);
    }

    void set_field(std::string_view field, const TableBuilder& builder)
    {
        push_table();

        builder.push_table(L);
        lua_pushlstring(L, field.data(), field.length());
        lua_settable(L, -3);

        lua_pop(L, -1);
    }

    void push_table()
    {
        lua_getref(L, ref);
    }

    void push_table(lua_State* push_to) const
    {
        lua_getref(push_to, ref);
    }

private:
    static int create_referenced_table(lua_State* L)
    {
        lua_createtable(L, 0, 0);
        return lua_ref(L, -1);
    }

    int ref;
    lua_State* L;
};
