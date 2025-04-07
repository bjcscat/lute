#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <sys/types.h>
#include <utility>

#include "lua.h"
#include "lute/fs.h"
#include "lute/runtime.h"
#include "lute/scheduler.h"

int main(int argc, const char* argv[])
{
    Runtime runtime{};

    std::ifstream cliSourceFile;

    cliSourceFile.open(".lute/cli.luau");

    std::string cliSource{std::istreambuf_iterator<char>{cliSourceFile}, std::istreambuf_iterator<char>{}};

    luteopen_fs(runtime.getRawState());
    lua_setfield(runtime.getRawState(), LUA_ENVIRONINDEX, "fs");

    runtime.getScheduler().scheduleResumption(ResumptionInfo{
        runtime.getRawState(),
        [&runtime, cliSource = std::move(cliSource), argc, argv](lua_State* L) // NOLINT : its argv
        {
            runtime.loadSource(cliSource);

            for (int i = 0; i < argc; i++)
            {
                lua_pushstring(L, argv[i]); // NOLINT : its just argv
            }

            return argc;
        }
    });

    try
    {
        runtime.getScheduler().run();
    }
    catch (std::exception& e)
    {
        std::cerr << "error\n";
    }

    return 0;
}
