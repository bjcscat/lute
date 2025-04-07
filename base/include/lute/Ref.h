#pragma once

#include "lua.h"

class Ref
{
public:
    explicit Ref(lua_State* L, int idx)
        : L{L}
        , refId(lua_ref(L, idx))
    {
    }

    ~Ref() {
        lua_unref(L, refId);
    }

    void push(lua_State* L) const {
        lua_getref(L, refId);
    }

    [[nodiscard]] lua_State* getState() const {
        return L;
    }

    Ref(const Ref&) = delete;
    Ref& operator=(const Ref&) = delete;

    Ref(Ref&& old) noexcept : refId(old.refId), L(old.L) {
        old.refId = LUA_REFNIL;
    }

    Ref& operator=(Ref&& old) noexcept {
        if (this != &old) {
            L = old.L;
            refId = old.refId;
            old.refId = LUA_REFNIL;
        }

        return *this;
    }
private:
    lua_State* L;
    int refId;
};
