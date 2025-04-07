#pragma once

#include "lute/Runtime.h"

#include <map>
#include <memory>
#include <optional>
#include <thread>

class GlobalRuntime
{
public:
    explicit GlobalRuntime(std::map<std::string, const char*> std_vfs)
        : std_vfs{std::move(std_vfs)}
    {
    }

    SharedRuntimePtr new_runtime(const RuntimeOptions& options)
    {
        SharedRuntimePtr runtime = Runtime::create_runtime(this, options);

        init_runtime(runtime.get());

        return runtime;
    }

private:
    void init_runtime(Runtime* runtime) {
        runtime->set_std_vfs(&std_vfs);
    }

    const std::map<std::string, const char*> std_vfs; // NOLINT

};
