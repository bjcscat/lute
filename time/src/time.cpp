#include "lute/time.h"
#include "lua.h"
#include "lualib.h"
#include "lute/LuteException.h"
#include "lute/UserdataTags.h"
#include "lute/Utilities.h"
#include "uv.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <cassert>
#include <limits>
#include <sstream>
#include <string>

namespace
{
constexpr int FORMAT_PRECISION = 9;

constexpr int64_t NANOSECONDS_PER_SECOND = 1000000000;
constexpr int64_t MICROSECONDS_PER_SECOND = 1000000;
constexpr int64_t MILLISECONDS_PER_SECOND = 1000;
constexpr int64_t SECONDS_PER_MINUTE = 60;
constexpr int64_t SECONDS_PER_HOUR = 3600;
constexpr int64_t SECONDS_PER_DAY = 86400;
constexpr int64_t SECONDS_PER_WEEK = 604800;
constexpr int64_t NANOSECONDS_PER_MICROSECOND = 1000;
constexpr int64_t NANOSECONDS_PER_MILLISECOND = NANOSECONDS_PER_SECOND / MILLISECONDS_PER_SECOND;

constexpr auto NEGATIVE_DURATION_ERROR = "duration cannot be negative";
constexpr auto DURATION_TOO_LONG = "duration is too long";


constexpr int log10_constexpr(int64_t n)
{
    constexpr auto TEN = 10;

    return n == 1 ? 0 : 1 + log10_constexpr(n / TEN);
}

constexpr auto TEN = 10;
constexpr int64_t pow10(int64_t n)
{
    return n == 1 ? TEN : TEN * pow10(n - 1);
}

template<int64_t UNITS_PER_SEC>
inline uv_timespec64_t convert_to_timespec(double time_value)
{
    static_assert(UNITS_PER_SEC % TEN == 0, "UNITS_PER_SEC must be a power of 10");
    // making the assumption that most timespecs will be exact
    if (LUTE_LIKELY(std::trunc(time_value) == time_value && time_value <= static_cast<double>(std::numeric_limits<int64_t>::max())))
    {
        const auto units = static_cast<int64_t>(time_value);
        const auto seconds = units / UNITS_PER_SEC;
        const auto remaining_units = units % UNITS_PER_SEC;

        constexpr int power = 9 - log10_constexpr(UNITS_PER_SEC);
        constexpr int64_t multiplier = power > 0 ? pow10(power) : 1;

        const auto nanoseconds = static_cast<int32_t>(remaining_units * multiplier);
        return {seconds, nanoseconds};
    }

    const auto whole_seconds = static_cast<int64_t>(time_value / UNITS_PER_SEC);
    const double remaining_time = time_value - (whole_seconds * static_cast<double>(UNITS_PER_SEC));

    constexpr int power = 9 - log10_constexpr(UNITS_PER_SEC);
    constexpr double multiplier = power > 0 ? pow10(power) : 1.0;

    auto nanoseconds = static_cast<int32_t>(std::round(remaining_time * multiplier));

    if (nanoseconds >= NANOSECONDS_PER_SECOND)
    {
        return {whole_seconds + 1, static_cast<int32_t>(nanoseconds - NANOSECONDS_PER_SECOND)};
    }

    return {whole_seconds, nanoseconds};
}

// Specialized template for nanoseconds
template<>
inline uv_timespec64_t convert_to_timespec<NANOSECONDS_PER_SECOND>(double time_value)
{
    // Direct conversion for nanoseconds
    auto total_ns = static_cast<int64_t>(time_value);
    int64_t seconds = total_ns / NANOSECONDS_PER_SECOND;
    auto nanoseconds = static_cast<int32_t>(total_ns % NANOSECONDS_PER_SECOND);

    return {seconds, nanoseconds};
}

// Specialized template for seconds
template<>
inline uv_timespec64_t convert_to_timespec<1>(double time_value)
{
    // Direct conversion for nanoseconds
    auto seconds = static_cast<int64_t>(time_value);
    auto nanoseconds = static_cast<int32_t>(std::fmod(time_value, 1) * NANOSECONDS_PER_SECOND);

    return {seconds, nanoseconds};
}

// Timespec helpers
float_t diff_timespecs(uv_timespec64_t left, uv_timespec64_t right)
{
    int64_t seconds_diff = left.tv_sec - right.tv_sec;
    int32_t nanoseconds_diff = left.tv_nsec - right.tv_nsec;

    if (nanoseconds_diff < 0)
    {
        seconds_diff -= 1;
        nanoseconds_diff += NANOSECONDS_PER_SECOND;
    };

    // TODO: verify this
    // NOLINTNEXTLINE
    return static_cast<float_t>(seconds_diff + (nanoseconds_diff / NANOSECONDS_PER_SECOND));
}

float_t seconds_since_timespec(uv_timespec64_t timespec)
{
    uv_timespec64_t now;
    uv_clock_gettime(UV_CLOCK_MONOTONIC, &now);

    return diff_timespecs(now, timespec);
}

double get_seconds_from_timespec(uv_timespec64_t timespec)
{
    return static_cast<double>(timespec.tv_sec) + (static_cast<double>(timespec.tv_nsec) / NANOSECONDS_PER_SECOND);
}

// Durations

// returns the address of the timespec from the duration on the stack
uv_timespec64_t get_timespec_from_duration(lua_State* L, int idx)
{
    return *static_cast<uv_timespec64_t*>(luaL_checkudata(L, idx, DURATION_TYPE));
}

void push_duration_mt(lua_State* L);

// creates a userdata, and returns a fresh timespec pointer to it
int create_duration_from_timespec(lua_State* L, uv_timespec64_t timespec)
{
    auto* duration = static_cast<uv_timespec64_t*>(lua_newuserdatatagged(L, sizeof(uv_timespec64_t), userdata_tags::DURATION_TAG));
    *duration = timespec;

    push_duration_mt(L);
    lua_setmetatable(L, -2);

    return 1;
}

// Duration methods
int duration_tonanoseconds(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, static_cast<double>(timespec.tv_sec * NANOSECONDS_PER_SECOND) + timespec.tv_nsec);
    return 1;
}

int duration_tomicroseconds(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, get_seconds_from_timespec(timespec) * MICROSECONDS_PER_SECOND);
    return 1;
}

int duration_tomilliseconds(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, get_seconds_from_timespec(timespec) * MILLISECONDS_PER_SECOND);
    return 1;
}

int duration_toseconds(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, get_seconds_from_timespec(timespec));
    return 1;
}

int duration_tominutes(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, get_seconds_from_timespec(timespec) / SECONDS_PER_MINUTE);
    return 1;
}

int duration_tohours(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, get_seconds_from_timespec(timespec) / SECONDS_PER_HOUR);
    return 1;
}

int duration_todays(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, get_seconds_from_timespec(timespec) / SECONDS_PER_DAY);
    return 1;
}

int duration_toweeks(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, get_seconds_from_timespec(timespec) / SECONDS_PER_WEEK);
    return 1;
}

int duration_subsecnanos(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, timespec.tv_nsec);
    return 1;
}

int duration_subsecmicros(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, static_cast<double>(timespec.tv_nsec) / NANOSECONDS_PER_MICROSECOND);
    return 1;
}

int duration_subsecmillis(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);
    lua_pushnumber(L, static_cast<double>(timespec.tv_nsec) / NANOSECONDS_PER_MILLISECOND);
    return 1;
}

// Metamethods
int duration_tostring(lua_State* L)
{
    uv_timespec64_t timespec = get_timespec_from_duration(L, 1);

    std::ostringstream fmt;

    fmt.precision(FORMAT_PRECISION);

    fmt << timespec.tv_sec << "." << timespec.tv_nsec;

    // fix os-specific format string difference between macos/windows and linux
    lua_pushstring(L, std::move(fmt).str().c_str());
    return 1;
}

int duration_add(lua_State* L)
{
    uv_timespec64_t left = get_timespec_from_duration(L, 1);
    uv_timespec64_t right = get_timespec_from_duration(L, 2);

    uv_timespec64_t result = {left.tv_sec + right.tv_sec, left.tv_nsec + right.tv_nsec};
    if (result.tv_nsec > NANOSECONDS_PER_SECOND)
    {
        result.tv_sec += 1;
        result.tv_nsec -= NANOSECONDS_PER_SECOND;
    }

    return create_duration_from_timespec(L, result);
}

int duration_sub(lua_State* L)
{
    uv_timespec64_t left = get_timespec_from_duration(L, 1);
    uv_timespec64_t right = get_timespec_from_duration(L, 2);

    uv_timespec64_t result = {left.tv_sec - right.tv_sec, left.tv_nsec - right.tv_nsec};
    if (result.tv_nsec < 0)
    {
        result.tv_sec -= 1;
        result.tv_nsec += NANOSECONDS_PER_SECOND;
    }

    return create_duration_from_timespec(L, {result.tv_sec >= 0 ? result.tv_sec : 0, result.tv_nsec >= 0 ? result.tv_nsec : 0});
}

enum class TimespecComparison : uint8_t
{
    Equal,
    Less,
    Greater
};

TimespecComparison compare(uv_timespec64_t left, uv_timespec64_t right)
{
    if (left.tv_sec == right.tv_sec && left.tv_nsec == right.tv_nsec)
    {
        return TimespecComparison::Equal;
    }

    return (left.tv_sec < right.tv_sec || (left.tv_sec == right.tv_sec && left.tv_nsec < right.tv_nsec)) ? TimespecComparison::Less
                                                                                                         : TimespecComparison::Greater;
}

int duration_eq(lua_State* L)
{
    uv_timespec64_t left = get_timespec_from_duration(L, 1);
    uv_timespec64_t right = get_timespec_from_duration(L, 2);
    lua_pushboolean(L, int(compare(left, right) == TimespecComparison::Equal));
    return 1;
}

int duration_lt(lua_State* L)
{
    uv_timespec64_t left = get_timespec_from_duration(L, 1);
    uv_timespec64_t right = get_timespec_from_duration(L, 2);

    lua_pushboolean(L, int(compare(left, right) == TimespecComparison::Less));
    return 1;
}

int duration_le(lua_State* L)
{
    uv_timespec64_t left = get_timespec_from_duration(L, 1);
    uv_timespec64_t right = get_timespec_from_duration(L, 2);

    TimespecComparison comparison = compare(left, right);
    lua_pushboolean(L, int(comparison == TimespecComparison::Equal || comparison == TimespecComparison::Less));
    return 1;
}

// Instants
uv_timespec64_t getTimespecFromInstant(lua_State* L, int idx)
{
    return *static_cast<uv_timespec64_t*>(luaL_checkudata(L, idx, INSTANT_TYPE));
}

// Methods
int instant_elapsed(lua_State* L)
{
    lua_pushnumber(L, seconds_since_timespec(getTimespecFromInstant(L, 1)));
    return 1;
}

// Metamethods
int instant_sub(lua_State* L)
{
    uv_timespec64_t left = getTimespecFromInstant(L, 1);
    uv_timespec64_t right = getTimespecFromInstant(L, 2);

    return create_duration_from_timespec(L, convert_to_timespec<1>(diff_timespecs(left, right)));
}

void init_instant_lib(lua_State* L)
{
    // metatable is in stack spot 1
    if (luaL_newmetatable(L, INSTANT_TYPE) == 0) {
        return;
    }

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, instant_sub, "Instant__sub");
    lua_setfield(L, -2, "__sub");

    // __index table
    lua_createtable(L, 0, 2);

    lua_pushcfunction(L, instant_elapsed, "Instant__elapsed");
    lua_setfield(L, -2, "elapsed");

    lua_setreadonly(L, -1, 1);

    // __index set
    lua_setfield(L, -2, "__index");

    lua_setreadonly(L, -1, 1);

    lua_pop(L, 1);
}

void push_duration_mt(lua_State* L)
{
    if (luaL_newmetatable(L, DURATION_TYPE) == 0) {
        return;
    }

    // Protect metatable from being changed
    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, duration_tostring, "Duration__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, duration_add, "Duration__add");
    lua_setfield(L, -2, "__add");

    lua_pushcfunction(L, duration_sub, "Duration__sub");
    lua_setfield(L, -2, "__sub");

    lua_pushcfunction(L, duration_eq, "Duration__eq");
    lua_setfield(L, -2, "__eq");

    lua_pushcfunction(L, duration_lt, "Duration__lt");
    lua_setfield(L, -2, "__lt");

    lua_pushcfunction(L, duration_le, "Duration__le");
    lua_setfield(L, -2, "__le");

    constexpr int MT_FUNCTION_COUNT = 11;

    // __index table
    lua_createtable(L, 0, MT_FUNCTION_COUNT);

    lua_pushcfunction(L, duration_tonanoseconds, "Duration__tonanoseconds");
    lua_setfield(L, -2, "tonanoseconds");

    lua_pushcfunction(L, duration_tomicroseconds, "Duration__tomicroseconds");
    lua_setfield(L, -2, "tomicroseconds");

    lua_pushcfunction(L, duration_tomilliseconds, "Duration__tomilliseconds");
    lua_setfield(L, -2, "tomilliseconds");

    lua_pushcfunction(L, duration_toseconds, "Duration__toseconds");
    lua_setfield(L, -2, "toseconds");

    lua_pushcfunction(L, duration_tominutes, "Duration__tominutes");
    lua_setfield(L, -2, "tominutes");

    lua_pushcfunction(L, duration_tohours, "Duration__tohours");
    lua_setfield(L, -2, "tohours");

    lua_pushcfunction(L, duration_todays, "Duration__todays");
    lua_setfield(L, -2, "todays");

    lua_pushcfunction(L, duration_toweeks, "Duration__toweeks");
    lua_setfield(L, -2, "toweeks");

    lua_pushcfunction(L, duration_subsecnanos, "Duration__subsecnanos");
    lua_setfield(L, -2, "subsecnanos");

    lua_pushcfunction(L, duration_subsecmicros, "Duration__subsecmicros");
    lua_setfield(L, -2, "subsecmicros");

    lua_pushcfunction(L, duration_subsecmillis, "Duration__subsecmillis");
    lua_setfield(L, -2, "subsecmillis");

    lua_setreadonly(L, -1, 1);

    // set __index
    lua_setfield(L, -2, "__index");

    // metatable is now in stack spot 1
    lua_setreadonly(L, -1, 1);
}
} // namespace

int duration::nanoseconds(lua_State* L)
{
    double nanoseconds = luaL_checknumber(L, 1);
    if (nanoseconds < 0)
    {
        throw LuteException{NEGATIVE_DURATION_ERROR};
    }

    return create_duration_from_timespec(L, convert_to_timespec<NANOSECONDS_PER_SECOND>(nanoseconds));
}

int duration::microseconds(lua_State* L)
{
    double microseconds = luaL_checknumber(L, 1);
    if (microseconds < 0)
    {
        throw LuteException{NEGATIVE_DURATION_ERROR};
    }

    return create_duration_from_timespec(L, convert_to_timespec<MICROSECONDS_PER_SECOND>(microseconds));
}

int duration::milliseconds(lua_State* L)
{
    double milliseconds = luaL_checknumber(L, 1);
    if (milliseconds < 0)
    {
        throw LuteException{NEGATIVE_DURATION_ERROR};
    }

    return create_duration_from_timespec(L, convert_to_timespec<MILLISECONDS_PER_SECOND>(milliseconds));
}

int duration::seconds(lua_State* L)
{
    double seconds = luaL_checknumber(L, 1);
    if (seconds < 0)
    {
        throw LuteException{NEGATIVE_DURATION_ERROR};
    }

    return create_duration_from_timespec(L, convert_to_timespec<1>(seconds));
}

int duration::minutes(lua_State* L)
{
    double minutes = luaL_checknumber(L, 1);
    if (minutes < 0)
    {
        throw LuteException{NEGATIVE_DURATION_ERROR};
    }

    return create_duration_from_timespec(L, convert_to_timespec<1>(minutes * SECONDS_PER_HOUR));
}

int duration::hours(lua_State* L)
{
    double hours = luaL_checknumber(L, 1);
    if (hours < 0)
    {
        throw LuteException{NEGATIVE_DURATION_ERROR};
    }

    // hours can still overflow, so we need to check

    // NOLINTNEXTLINE as the comparison is sound
    if (hours > (std::numeric_limits<int64_t>::max() / SECONDS_PER_HOUR))
    {
        throw LuteException{DURATION_TOO_LONG};
    }

    return create_duration_from_timespec(L, convert_to_timespec<1>(hours * SECONDS_PER_HOUR));
}

int duration::days(lua_State* L)
{
    double days = luaL_checknumber(L, 1);
    if (days < 0)
    {
        throw LuteException{NEGATIVE_DURATION_ERROR};
    }

    // account for overflow

    // NOLINTNEXTLINE as the comparison is sound
    if (days > (std::numeric_limits<int64_t>::max() / SECONDS_PER_DAY))
    {
        throw LuteException{DURATION_TOO_LONG};
    }

    return create_duration_from_timespec(L, convert_to_timespec<1>(days * SECONDS_PER_DAY));
}

int duration::weeks(lua_State* L)
{
    double weeks = luaL_checknumber(L, 1);
    if (weeks < 0)
    {
        throw LuteException{NEGATIVE_DURATION_ERROR};
    }

    // account for overflow

    // NOLINTNEXTLINE as the comparison is sound
    if (weeks > (std::numeric_limits<int64_t>::max() / SECONDS_PER_WEEK))
    {
        throw LuteException{DURATION_TOO_LONG};
    }

    return create_duration_from_timespec(L, convert_to_timespec<1>(weeks * SECONDS_PER_HOUR));
}

int lib_time::now(lua_State* L)
{
    uv_timespec64_t now;

    int status = uv_clock_gettime(UV_CLOCK_MONOTONIC, &now);
    assert(status == 0);

    auto* timespec = static_cast<uv_timespec64_t*>(lua_newuserdatatagged(L, sizeof(uv_timespec64_t), userdata_tags::INSTANT_TAG));

    *timespec = now;

    luaL_getmetatable(L, INSTANT_TYPE);
    lua_setmetatable(L, -2);

    return 1;
}

int lib_time::since(lua_State* L)
{
    lua_pushnumber(L, seconds_since_timespec(getTimespecFromInstant(L, 1)));
    return 1;
}

