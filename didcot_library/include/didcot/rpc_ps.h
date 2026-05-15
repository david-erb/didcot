#pragma once

#include <stdint.h>

#include <dtcore/dterr.h>
#include <dtcore/dtrpc.h>

#include <dtmc_services/dtservices.h>

extern dterr_t*
main_rpc_ps_create(dtrpc_handle* rpc_handle, int32_t model_number, dtservices_t* services);
