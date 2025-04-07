#pragma once

#include "lute/LuteLibs.h"

LUTE_DECLARE_TYPE(export type Resumption = () -> (...))

LUTE_DECLARE_FUNCTION(runtime, create, () -> ());
LUTE_DECLARE_FUNCTION(runtime, load, (source: string, chunkname: string) -> ());
LUTE_DECLARE_FUNCTION(runtime, resume, (resumption: Resumption) -> ());

LUTE_DEFINE_LIB(runtime,
    LUTE_LIB_ENTRY(create)
    LUTE_LIB_ENTRY(load)
    LUTE_LIB_ENTRY(resume)
)
