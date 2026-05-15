#pragma once

#include <stdint.h>

#include <dtcore/dterr.h>
#include <dtcore/dtrpc.h>

#include <dtmc_base/dthttpd.h>

extern dterr_t*
main_rpc_exit_create(dtrpc_handle* rpc_handle, int32_t model_number, dthttpd_handle httpserver_handle);
