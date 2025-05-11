#pragma once

#include "lualib.h"
#include "lute/Ref.h"
#include "lute/LuteException.h"
#include "lute/Runtime.h"
#include "lute/Scheduler.h"

#include <App.h>
#include <future>
#include <lua.h>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace net
{
using uWSApp = std::variant<std::unique_ptr<uWS::App>, std::unique_ptr<uWS::SSLApp>>;

constexpr int DEFAULT_NET_PORT = 3000;

template<bool FORBID_NIL = false>
inline std::optional<std::string> get_field_string_copied(lua_State* L, int narg, const char* key)
{
    const char* field = nullptr;

    int type = lua_getfield(L, narg, key);
    if (type == LUA_TSTRING)
    {
        field = lua_tostring(L, -1);
    }
    else if (type != LUA_TNIL || FORBID_NIL)
    {
        throw LuteException{std::string("Invalid value specified for \"") + key + "\", expected a string."};
    }

    lua_pop(L, 1);

    if (field == nullptr)
    {
        return std::nullopt;
    }

    return field;
}

struct TlsOptions
{
    std::string key_file_name;
    std::string cert_file_name;
    std::optional<std::string> passphrase = nullptr;
    std::optional<std::string> dh_params_file_name = nullptr;
    std::optional<std::string> ca_file_name = nullptr;
    std::optional<std::string> ssl_ciphers = nullptr;

    bool ssl_prefer_low_memory_usage = false;

    // NOLINTNEXTLINE
    operator uWS::SocketContextOptions() const
    {
        uWS::SocketContextOptions opts;

        opts.key_file_name = key_file_name.c_str();
        opts.cert_file_name = key_file_name.c_str();

        if (passphrase.has_value())
        {
            opts.passphrase = passphrase->c_str();
        }

        if (dh_params_file_name.has_value())
        {
            opts.dh_params_file_name = dh_params_file_name->c_str();
        }

        if (ca_file_name.has_value())
        {
            opts.ca_file_name = ca_file_name->c_str();
        }

        if (ssl_ciphers.has_value())
        {
            opts.passphrase = ssl_ciphers->c_str();
        }

        return opts;
    }
};

struct ServeOptions // NOLINT
{
    std::string hostname = "0.0.0.0";
    int port = DEFAULT_NET_PORT;
    std::optional<TlsOptions> tls;
    Ref handler_function;

    ServeOptions(const ServeOptions&) = delete;
    ServeOptions& operator=(const ServeOptions&) = delete;
    ServeOptions(ServeOptions&&) = default;
    ServeOptions& operator=(ServeOptions&&) = default;

    ServeOptions(lua_State* L, int narg)
        : handler_function{L}
    {
        luaL_checktype(L, narg, LUA_TTABLE);

        if (std::optional<std::string> hostname = get_field_string_copied(L, narg, "hostname"))
        {
            hostname = std::move(hostname.value());
        }

        if (lua_getfield(L, narg, "port") == LUA_TNUMBER)
        {
            port = lua_tointeger(L, -1);
        }

        lua_pop(L, 1);

        if (lua_getfield(L, narg, "tls") == LUA_TTABLE)
        {
            lua_ref(L, -1);

            tls = TlsOptions{
                get_field_string_copied<true>(L, -1, "key_file_name").value(),
                get_field_string_copied<true>(L, -1, "cert_file_name").value(),
                get_field_string_copied(L, -1, "passphrase"),
                get_field_string_copied(L, -1, "dh_params_file_name"),
                get_field_string_copied(L, -1, "ca_file_name"),
                get_field_string_copied(L, -1, "ssl_ciphers"),
                false
            };

            if (lua_getfield(L, -1, "prefer_low_memory_usage") == LUA_TBOOLEAN)
            {
                tls->ssl_prefer_low_memory_usage = (lua_toboolean(L, -1) == 1);
            }

            lua_pop(L, 1);
        }

        lua_pop(L, 1);

        if (lua_getfield(L, 1, "handler") == LUA_TFUNCTION)
        {
            handler_function = Ref{L, -1};
        }
        else
        {
            throw LuteException{"Invalid value specified for \"handler\", expected function."};
        }

        lua_pop(L, 1);
    }
};

class ServeState
{
public:
    explicit ServeState(ServeOptions options_arg)
        : options{std::move(options_arg)}
    {
        if (options.tls.has_value())
        {
            app = std::make_unique<uWS::SSLApp>(options.tls.value());
        }
        else
        {
            app = std::make_unique<uWS::App>();
        }
    }

    void run(const SharedRuntimePtr& runtime)
    {
        if (auto* basic_app = std::get_if<std::unique_ptr<uWS::App>>(&app))
        {
            basic_app->get()
                    ->get(
                        "/*",
                        [runtime](auto* res, auto* /*req*/)
                        {
                            runtime->get_scheduler().remote_schedule_future();
                            res->end("Hello world!");
                        }
                    )
                    .listen(
                        options.port,
                        [](us_listen_socket_t* socket)
                        {
                            std::cout << "Running\n";
                        }
                    ).run();

            auto worker_thread_fn = [basic_app, this, runtime]()
            {
                basic_app->get()
                    ->get(
                        "/*",
                        [runtime](auto* res, auto* /*req*/)
                        {
                            runtime->get_scheduler().remote_schedule_future();
                            res->end("Hello world!");
                        }
                    )
                    .listen(
                        options.port,
                        [](auto* socket)
                        {
                            std::cout << "Running at: " << std::hex << socket << '\n';
                        }
                    ).run();

                std::cerr << "run failed\n";
            };

            std::thread worker_thread{worker_thread_fn};

            worker_thread.detach();
        }
    }

private:
    ServeOptions options;
    uWSApp app;
};
} // namespace net
