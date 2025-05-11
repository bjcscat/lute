#pragma once

#include "lute/LuteLibs.h"

LUTE_DECLARE_TYPE(export type ServiceOptions = {
    hostname: string?,
    port: number?
    tls: {
        key_file_name: string,
        cert_file_name: string,
        passphrase: string?,
        dh_params_file_name: string?,
        ca_file_name: string?,
        ssl_ciphers: string?
    }?
})

LUTE_DECLARE_FUNCTION(net, serve, (hostname: string?, port: number?, tls: TLSOptions?)->());
// LUTE_DECLARE_FUNCTION(net, serve, ()->());

// LUTE_DECLARE_LIB_INIT(net);

LUTE_DEFINE_LIB(net, LUTE_LIB_ENTRY(serve))
