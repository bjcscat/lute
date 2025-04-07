#pragma once

#include "lute/lib.h"

LUTE_DECLARE_FUNCTION(fs, readfile, (path: string)->string);

LUTE_DECLARE_LIB(fs,
    LUTE_LIB_ENTRY(readfile))
