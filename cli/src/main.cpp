#include <array>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <lua.h>
#include <string>
#include <vector>

#include "lute/GlobalRuntime.h"
#include "lute/LuauUtilities.h"
#include "lute/LuteException.h"

#include "lute/Runtime.h"
// #include "lute/RuntimeLib.h"
#include "lute/Scheduler.h"
#include "lute/fs.h"

// GENERATED
#include "cli_source.h"
#include "lute_std_source.h"

namespace
{
std::string read_file(const std::filesystem::path& path)
{
    std::ifstream source_file;

    source_file.open(path);

    if (!source_file.is_open())
    {
        throw LuteException{"Could not open source file at path"};
    }

    return std::string{std::istreambuf_iterator<char>{source_file}, std::istreambuf_iterator<char>{}};
}

std::map<std::string, const char*> setup_vfs()
{
    std::map<std::string, const char*> std_vfs{};

    for (auto [name, src] : STD_LIBS)
    {
        std_vfs.insert({name, src});
    }

    return std_vfs;
}

template<typename T, size_t Size>
std::vector<T> convert_array(std::array<T, Size> arr)
{
    return std::vector<T>{arr.begin(), arr.end()};
}

} // namespace

int main(int argc, const char* argv[])
{
    GlobalRuntime global_runtime{setup_vfs()};

    std::vector<RuntimeOptions::Library> libs;

    libs.emplace_back("@lute/fs", convert_array(fs::lib));
    // libs.emplace_back("time", convert_array(lib_time::lib));
    // libs.emplace_back("runtime", convert_array(runtime::lib));

    std::promise<LuauFunction> resumption;

    SharedRuntimePtr runtime = global_runtime.new_runtime(RuntimeOptions{libs});

    runtime->get_scheduler().schedule_future(LuauThreadFuture{runtime->get_main_state(), resumption.get_future().share()});

    resumption.set_value(
        [runtime, argc, argv](lua_State* L) // NOLINT
        {
            runtime->load_source(L, CLI_SOURCE, "@cli");

            lua_createtable(L, argc, 0);
            for (int i = 0; i < argc; i++) {
                lua_pushinteger(L, i);
                lua_pushstring(L, argv[i]); // NOLINT
                lua_settable(L, -3);
            }

            return 1;
        }
    );

    runtime->get_scheduler().set_error_callback([](lua_State* L) {
        std::cerr << "Unhandled Luau error: " << lua_tostring(L, -1) << '\n' << lua_debugtrace(L);

        return 0;
    });

    runtime->get_scheduler().run();

    return 0;
}
