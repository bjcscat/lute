#pragma once

#include "lute/LuteLibs.h"

LUTE_DECLARE_TYPE(declare class Duration end)

LUTE_DECLARE_FUNCTION(duration, nanoseconds, (nanoseconds: number)->Duration);
LUTE_DECLARE_FUNCTION(duration, microseconds, (microseconds: number)->Duration);
LUTE_DECLARE_FUNCTION(duration, milliseconds, (milliseconds: number)->Duration);
LUTE_DECLARE_FUNCTION(duration, seconds, (seconds: number)->Duration);
LUTE_DECLARE_FUNCTION(duration, minutes, (minutes: number)->Duration);
LUTE_DECLARE_FUNCTION(duration, hours, (hours: number)->Duration);
LUTE_DECLARE_FUNCTION(duration, days, (days: number)->Duration);
LUTE_DECLARE_FUNCTION(duration, weeks, (weeks: number)->Duration);

LUTE_DECLARE_LIB_INIT(duration);

LUTE_DEFINE_LIB(duration,
    LUTE_LIB_ENTRY(nanoseconds)
    LUTE_LIB_ENTRY(microseconds)
    LUTE_LIB_ENTRY(milliseconds)
    LUTE_LIB_ENTRY(seconds)
    LUTE_LIB_ENTRY(minutes)
    LUTE_LIB_ENTRY(hours)
    LUTE_LIB_ENTRY(days)
    LUTE_LIB_ENTRY(weeks)
)

constexpr auto INSTANT_TYPE = "instant";
constexpr auto DURATION_TYPE = "duration";

LUTE_DECLARE_FUNCTION(lib_time, now, (nanoseconds: number)->Duration);
LUTE_DECLARE_FUNCTION(lib_time, since, (microseconds: number)->Duration);

LUTE_DEFINE_LIB(lib_time,
    LUTE_LIB_ENTRY(now)
    LUTE_LIB_ENTRY(since)
)
