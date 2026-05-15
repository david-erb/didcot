// didcot_dac.c

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <dtcore/dterr.h>
#include <dtcore/dtheaper.h>
#include <dtcore/dtkvp.h>
#include <dtcore/dtlog.h>

#include <dtmc_base/dtcpu.h>
#include <dtmc_base/dtinterval.h>
#include <dtmc_base/dtlock.h>
#include <dtmc_base/dtmcp4728.h>
#include <dtmc_base/dttasker.h>
#include <dtmc_base/dttimeseries.h>
#include <dtmc_base/dttimeseries_beat.h>
#include <dtmc_base/dttimeseries_browngrav.h>
#include <dtmc_base/dttimeseries_sawtooth.h>
#include <dtmc_base/dttimeseries_sine.h>
#include <dtmc_base/dttimeseries_steady.h>

#include <didcot/didcot_dac.h>
#include <dtmc_services/dtservice.h>

#define TAG "didcot_dac"
#define dtlog_debug(...)

DTSERVICE_INIT_VTABLE(didcot_dac);

// private structure for internal use only, not exposed in the header
typedef struct didcot_dac_t
{
    DTSERVICE_COMMON_MEMBERS
    didcot_dac_config_t config;
    dttimeseries_handle timeseries_handles[DIDCOT_MCP4728_CHANNEL_COUNT];
    dtmcp4728_channel_config_t channel_configs[DIDCOT_MCP4728_CHANNEL_COUNT];
    int32_t callback_count;
    dtcpu_t period_timer;
    _Atomic int stop_requested;
    dtservice_stop_poll_fn stop_poll_fn;
    void* stop_poll_context;
    dtservice_status_t status;
    dtlock_handle lock_status;
    bool is_configured;
} didcot_dac_t;

static dterr_t*
didcot_dac__interval_callback(void* context, int* should_pause);
static dterr_t*
didcot_dac__run(didcot_dac_t* self, dttasker_handle tasker_handle);
static dterr_t*
didcot_dac__set_status(didcot_dac_t* self, dtservice_state_t state, dterr_t* error);

// --------------------------------------------------------------------------------------
dterr_t*
didcot_dac_create(didcot_dac_t** self_ptr)
{
    dterr_t* dterr = NULL;
    didcot_dac_t* self = NULL;
    DTERR_ASSERT_NOT_NULL(self_ptr);

    DTERR_C(dtheaper_alloc(sizeof(didcot_dac_t), "didcot_dac_t", (void**)&self));
    memset(self, 0, sizeof(didcot_dac_t));
    self->model_number = DIDCOT_DAC_SERVICE_MODEL;

    DTERR_C(dtservice_set_vtable(self->model_number, &didcot_dac_vt));
    DTERR_C(dtlock_create(&self->lock_status));
    DTERR_C(didcot_dac__set_status(self, DTSERVICE_STATE_IDLE, NULL));

    *self_ptr = self;
    self = NULL;

cleanup:
    didcot_dac_dispose(self);
    return dterr;
}

// --------------------------------------------------------------------------------------
void
didcot_dac_stop(didcot_dac_t* self)
{
    atomic_store(&self->stop_requested, 1);
}

// --------------------------------------------------------------------------------------
// configure the DAC
// the hardware for the mcp4728 should already be created and configured by the caller and passed in via the config struct
// here we set up the waveforms for the 4 channels

dterr_t*
didcot_dac_configure(didcot_dac_t* self, const didcot_dac_config_t* config)
{
    dterr_t* dterr = NULL;
    dtkvp_list_t _kvp_list = { 0 }, *kvp_list = &_kvp_list;

    DTERR_ASSERT_NOT_NULL(self);
    DTERR_ASSERT_NOT_NULL(config);

    if (self->is_configured)
    {
        dterr = dterr_new(DTERR_BADARG, DTERR_LOC, NULL, "instance is already configured");
        goto cleanup;
    }

    self->config = *config;
    self->is_configured = true;
    self->callback_count = 0;
    atomic_store(&self->stop_requested, 0);
    DTERR_C(dtkvp_list_init(kvp_list));
    DTERR_C(didcot_dac__set_status(self, DTSERVICE_STATE_IDLE, NULL));

    // channel 0: steady DC midpoint
    {
        dttimeseries_steady_t* o = NULL;
        DTERR_C(dttimeseries_steady_create(&o));
        DTERR_C(dtkvp_list_set(kvp_list, "value", "1.000"));
        DTERR_C(dttimeseries_steady_configure(o, kvp_list));
        self->timeseries_handles[0] = (dttimeseries_handle)o;
    }

    // channel 1: sine, 5 cycles / 10s, centered on 1.65V
    {
        dttimeseries_sine_t* o = NULL;
        DTERR_C(dttimeseries_sine_create(&o));
        DTERR_C(dtkvp_list_set(kvp_list, "frequency", "0.500"));
        DTERR_C(dtkvp_list_set(kvp_list, "amplitude", "1.650"));
        DTERR_C(dtkvp_list_set(kvp_list, "offset", "1.650"));
        DTERR_C(dttimeseries_sine_configure(o, kvp_list));
        self->timeseries_handles[1] = (dttimeseries_handle)o;
    }

    // channel 2: sawtooth: 0-3.3V, period=2s, rise=4/3s (2x fall), fall=2/3s
    {
        dttimeseries_sawtooth_t* o = NULL;
        DTERR_C(dttimeseries_sawtooth_create(&o));
        DTERR_C(dtkvp_list_set(kvp_list, "min", "0.000"));
        DTERR_C(dtkvp_list_set(kvp_list, "max", "3.300"));
        DTERR_C(dtkvp_list_set(kvp_list, "rise_time", "1.333"));
        DTERR_C(dtkvp_list_set(kvp_list, "fall_time", "0.667"));
        DTERR_C(dttimeseries_sawtooth_configure(o, kvp_list));
        self->timeseries_handles[2] = (dttimeseries_handle)o;
    }

    // channel 3: brownian gravity random walk, wide wander centered on 1.65V
    // noise=80, strength=1 -> alpha=0.01 -> sigma~566 raw units -> ~566mV,
    // giving dramatic excursions across most of the 0-3.3V range
    {
        dttimeseries_browngrav_t* o = NULL;
        DTERR_C(dttimeseries_browngrav_create(&o));
        DTERR_C(dtkvp_list_set(kvp_list, "attraction_point", "1650"));
        DTERR_C(dtkvp_list_set(kvp_list, "attraction_strength", "1"));
        DTERR_C(dtkvp_list_set(kvp_list, "noise_intensity", "80"));
        DTERR_C(dtkvp_list_set(kvp_list, "seed", "42"));
        DTERR_C(dtkvp_list_set(kvp_list, "scale", "0.001"));
        DTERR_C(dtkvp_list_set(kvp_list, "offset", "0.0"));
        DTERR_C(dttimeseries_browngrav_configure(o, kvp_list));
        self->timeseries_handles[3] = (dttimeseries_handle)o;
    }

    // initialize per-channel DAC config (value updated each loop tick)
    self->channel_configs[0] = (dtmcp4728_channel_config_t){ .channel = DTMCP4728_CHANNEL_A,
        .value_12bit = 0,
        .vref = DTMCP4728_VREF_VDD,
        .power_down = DTMCP4728_POWER_DOWN_NORMAL,
        .gain = DTMCP4728_GAIN_X1,
        .udac = false };
    self->channel_configs[1] = (dtmcp4728_channel_config_t){ .channel = DTMCP4728_CHANNEL_B,
        .value_12bit = 0,
        .vref = DTMCP4728_VREF_VDD,
        .power_down = DTMCP4728_POWER_DOWN_NORMAL,
        .gain = DTMCP4728_GAIN_X1,
        .udac = false };
    self->channel_configs[2] = (dtmcp4728_channel_config_t){ .channel = DTMCP4728_CHANNEL_C,
        .value_12bit = 0,
        .vref = DTMCP4728_VREF_VDD,
        .power_down = DTMCP4728_POWER_DOWN_NORMAL,
        .gain = DTMCP4728_GAIN_X1,
        .udac = false };
    self->channel_configs[3] = (dtmcp4728_channel_config_t){ .channel = DTMCP4728_CHANNEL_D,
        .value_12bit = 0,
        .vref = DTMCP4728_VREF_VDD,
        .power_down = DTMCP4728_POWER_DOWN_NORMAL,
        .gain = DTMCP4728_GAIN_X1,
        .udac = false };

cleanup:
    dtkvp_list_dispose(kvp_list);
    return dterr;
}

// --------------------------------------------------------------------------------------
dterr_t*
didcot_dac_set_stop_poll(didcot_dac_t* self, dtservice_stop_poll_fn fn, void* context)
{
    dterr_t* dterr = NULL;
    DTERR_ASSERT_NOT_NULL(self);
    self->stop_poll_fn = fn;
    self->stop_poll_context = context;

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
dterr_t*
didcot_dac_happy_loop(didcot_dac_t* self)
{
    dterr_t* dterr = NULL;
    DTERR_ASSERT_NOT_NULL(self);
    DTERR_C(didcot_dac__run(self, NULL));

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
dterr_t*
didcot_dac_recover(didcot_dac_t* self)
{
    dterr_t* dterr = NULL;
    DTERR_ASSERT_NOT_NULL(self);

    dtlog_info(TAG, "cooling down for 1000 ms before retrying service loop");
    dtruntime_sleep_milliseconds(1000);
    DTERR_C(didcot_dac__set_status(self, DTSERVICE_STATE_IDLE, NULL));

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
dterr_t*
didcot_dac_get_status(didcot_dac_t* self DTSERVICE_GET_STATUS_ARGS)
{
    dterr_t* dterr = NULL;
    bool is_locked = false;

    DTERR_ASSERT_NOT_NULL(self);
    DTERR_ASSERT_NOT_NULL(status);

    DTERR_C(dtlock_acquire(self->lock_status));
    is_locked = true;
    *status = self->status;

cleanup:
    if (is_locked)
        dtlock_release(self->lock_status);
    return dterr;
}

// --------------------------------------------------------------------------------------
dterr_t*
didcot_dac_get_metrics(didcot_dac_t* self DTSERVICE_GET_METRICS_ARGS)
{
    dterr_t* dterr = NULL;
    dtservice_status_t status = { 0 };
    char tmp[32];

    DTERR_ASSERT_NOT_NULL(self);
    DTERR_ASSERT_NOT_NULL(kvp_list);

    DTERR_C(didcot_dac_get_status(self, &status));

    sprintf(tmp, "%" PRId32, self->callback_count);
    DTERR_C(dtkvp_list_set(kvp_list, DTSERVICE_METRIC_RX_BYTE_COUNT, "0"));
    DTERR_C(dtkvp_list_set(kvp_list, DTSERVICE_METRIC_TX_BYTE_COUNT, tmp));
    DTERR_C(dtkvp_list_set(kvp_list,
      DTSERVICE_METRIC_RX_ERROR_MESSAGE,
      status.dterr != NULL && status.dterr->message != NULL ? status.dterr->message : "ok"));
    DTERR_C(dtkvp_list_set(kvp_list,
      DTSERVICE_METRIC_TX_ERROR_MESSAGE,
      status.dterr != NULL && status.dterr->message != NULL ? status.dterr->message : "ok"));

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
dterr_t*
didcot_dac_to_string(didcot_dac_t* self, char* out_string, int32_t out_string_size)
{
    dterr_t* dterr = NULL;

    DTERR_ASSERT_NOT_NULL(self);
    DTERR_ASSERT_NOT_NULL(out_string);

    if (out_string_size <= 0)
    {
        dterr = dterr_new(DTERR_BADARG, DTERR_LOC, NULL, "out_string_size must be > 0");
        goto cleanup;
    }

    snprintf(out_string, (size_t)out_string_size, "didcot_dac service");

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
// this is the entry point for the tasker that runs the DAC control loop

dterr_t*
didcot_dac_entrypoint(void* context, dttasker_handle tasker_handle)
{
    didcot_dac_t* self = (didcot_dac_t*)context;
    dterr_t* dterr = NULL;
    DTERR_C(didcot_dac__run(self, tasker_handle));

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
// callback for the main loop interval timer
// reads timeseries, writes DAC, logs output

static dterr_t*
didcot_dac__interval_callback(void* context, int* should_pause)
{
    dterr_t* dterr = NULL;
    didcot_dac_t* self = (didcot_dac_t*)context;
    dtcpu_t write_timer = { 0 };
    dtcpu_microseconds_t period_us = 0;

    dtcpu_mark(&self->period_timer);
    if (self->callback_count > 0)
        period_us = dtcpu_elapsed_microseconds(&self->period_timer);

    if (atomic_load(&self->stop_requested))
    {
        *should_pause = 1;
        goto cleanup;
    }

    if (self->stop_poll_fn != NULL)
    {
        bool should_stop = false;
        DTERR_C(self->stop_poll_fn(self->stop_poll_context, &should_stop));
        if (should_stop)
        {
            *should_pause = 1;
            goto cleanup;
        }
    }

    self->callback_count++;

    dtruntime_milliseconds_t milliseconds = dtruntime_now_milliseconds();

    for (int i = 0; i < DIDCOT_MCP4728_CHANNEL_COUNT; i++)
    {
        double v;
        DTERR_C(dttimeseries_read(self->timeseries_handles[i], milliseconds * 1000, &v));

        if (v > 3.3)
            v = 3.3;
        else if (v < 0.0)
            v = 0.0;

        self->channel_configs[i].value_12bit = (uint16_t)(v / 3.3 * 4095.0 + 0.5);
    }

    // instrumentation for how long it takes to push the buffer out to the DAC hardware
    dtcpu_mark(&write_timer);
    DTERR_C(dtmcp4728_fast_write(self->config.mcp4728, self->channel_configs));
    dtcpu_mark(&write_timer);

    if (self->config.eventlogger != NULL)
    {
        dtruntime_milliseconds_t post_milliseconds = dtruntime_now_milliseconds();
        dteventlogger_item1_t item = {
            .timestamp = post_milliseconds,
            .value1 = (int32_t)period_us,
            .value2 = (int32_t)dtcpu_elapsed_microseconds(&write_timer),
        };
        DTERR_C(dteventlogger_append(self->config.eventlogger, &item));
    }

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
static dterr_t*
didcot_dac__run(didcot_dac_t* self, dttasker_handle tasker_handle)
{
    dterr_t* dterr = NULL;

    DTERR_ASSERT_NOT_NULL(self);

    if (!self->is_configured)
    {
        dterr = dterr_new(DTERR_STATE, DTERR_LOC, NULL, "service is not configured");
        goto cleanup;
    }

    DTERR_C(didcot_dac__set_status(self, DTSERVICE_STATE_STARTING, NULL));

    // attach the hardware resource
    DTERR_C(dtmcp4728_attach(self->config.mcp4728));

    {
        char tmp[128];
        DTERR_C(dtmcp4728_to_string(self->config.mcp4728, tmp, sizeof(tmp)));
        dtlog_debug(TAG, "attached DAC \"%s\"", tmp);
    }

    DTERR_C(dtinterval_set_callback(self->config.interval_handle, didcot_dac__interval_callback, self));

    if (tasker_handle != NULL)
        DTERR_C(dttasker_ready(tasker_handle));

    DTERR_C(didcot_dac__set_status(self, DTSERVICE_STATE_ACTIVE, NULL));

    // blocks until paused by a stop request or fails with an error
    DTERR_C(dtinterval_start(self->config.interval_handle));

    DTERR_C(didcot_dac__set_status(self, DTSERVICE_STATE_STOPPED, NULL));

cleanup:

    if (self->config.mcp4728 != NULL)
        DTERR_APPEND(dtmcp4728_detach(self->config.mcp4728));

    if (dterr != NULL)
        DTERR_APPEND(didcot_dac__set_status(self, DTSERVICE_STATE_ERROR, dterr));

    return dterr;
}

// --------------------------------------------------------------------------------------
static dterr_t*
didcot_dac__set_status(didcot_dac_t* self, dtservice_state_t state, dterr_t* error)
{
    dterr_t* dterr = NULL;
    bool is_locked = false;

    DTERR_ASSERT_NOT_NULL(self);

    DTERR_C(dtlock_acquire(self->lock_status));
    is_locked = true;

    dterr_dispose(self->status.dterr);
    self->status.dterr = NULL;
    self->status.state = state;

    if (error != NULL)
    {
        dterr_t* leaf = error;
        while (leaf->inner_err != NULL)
            leaf = leaf->inner_err;

        self->status.dterr =
          dterr_new(leaf->error_code, DTERR_LOC, NULL, "%s", leaf->message != NULL ? leaf->message : "service error");
    }

cleanup:
    if (is_locked)
        dtlock_release(self->lock_status);
    return dterr;
}

// --------------------------------------------------------------------------------------
void
didcot_dac_dispose(didcot_dac_t* self)
{
    if (self == NULL)
        return;

    dterr_dispose(self->status.dterr);
    dtlock_dispose(self->lock_status);

    for (int i = 0; i < DIDCOT_MCP4728_CHANNEL_COUNT; i++)
        dttimeseries_dispose(self->timeseries_handles[i]);

    dtheaper_free(self);
}
