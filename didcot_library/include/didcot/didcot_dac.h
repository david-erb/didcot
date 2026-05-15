/*
 * didcot_dac -- Real-time control loop driving four synthesized waveforms to the MCP4728 DAC.
 *
 * Drives the MCP4728 quad 12-bit DAC over I2C, sampling four timeseries on a
 * fixed interval and writing the results to DAC channels A through D.
 *
 * The module runs as a dedicated task under the dttasker facade, which handles cooperative stop,
 * ready signaling, and cross-platform scheduler integration.
 *
 * DAC hardware access is decoupled through the dtmcp4728 vtable interface,
 * keeping the loop logic independent of the underlying I2C implementation.
 *
 * cdox v1.0.2.1
 */
#pragma once

#include <dtcore/dterr.h>
#include <dtcore/dteventlogger.h>

#include <dtmc_base/dtinterval.h>
#include <dtmc_base/dtmcp4728.h>
#include <dtmc_base/dttasker.h>
#include <dtmc_services/dtservice.h>

#include <didcot/didcot.h>

#define DIDCOT_MCP4728_CHANNEL_COUNT 4

typedef struct didcot_dac_t didcot_dac_t;

typedef struct didcot_dac_config_t
{
    dtinterval_handle interval_handle;
    dtmcp4728_handle mcp4728;
    dteventlogger_t* eventlogger;

} didcot_dac_config_t;

extern dterr_t*
didcot_dac_create(didcot_dac_t** self_ptr);

extern dterr_t*
didcot_dac_configure(didcot_dac_t* self, const didcot_dac_config_t* config);

extern void
didcot_dac_dispose(didcot_dac_t* self);

extern void
didcot_dac_stop(didcot_dac_t* self);

extern dterr_t*
didcot_dac_entrypoint(void* context, dttasker_handle tasker_handle);

extern dterr_t*
didcot_dac_happy_loop(didcot_dac_t* self);

extern dterr_t*
didcot_dac_recover(didcot_dac_t* self);

extern dterr_t*
didcot_dac_get_status(didcot_dac_t* self DTSERVICE_GET_STATUS_ARGS);

extern dterr_t*
didcot_dac_get_metrics(didcot_dac_t* self DTSERVICE_GET_METRICS_ARGS);

extern dterr_t*
didcot_dac_to_string(didcot_dac_t* self DTSERVICE_TO_STRING_ARGS);

extern dterr_t*
didcot_dac_set_stop_poll(didcot_dac_t* self DTSERVICE_SET_STOP_POLL_ARGS);
