#include <dtcore/dterr.h>
#include <dtcore/dteventlogger.h>
#include <dtcore/dtlog.h>
#include <dtcore/dtstr.h>
#include <dtcore/dttimeout.h>

#include <dtmc_base/dtruntime.h>
#include <dtmc_base/dttasker_registry.h>

#include <dtmc/dtinterval_espidf.h>
#include <dtmc/dtmcp4728_espidf.h>
#include <dtmc_services/dtservice_registry.h>
#include <dtmc_services/dtservices.h>

// project's DAC service, which manages the MCP4728 and the main interval timer loop
#include <didcot/didcot_dac.h>

#define TAG "main"
#define DAC_DELTA_EVENT_COUNT 20

#define LOOP_INTERVAL_MILLISECONDS 10

// --------------------------------------------------------------------------------------
void
app_main(void)
{
    dterr_t* dterr = NULL;
    dttasker_registry_t* tasker_registry = &dttasker_registry_global_instance;
    dtservice_registry_t* service_registry = NULL;
    dtservices_t* services = NULL;
    dtinterval_handle interval_handle = NULL;
    dtmcp4728_handle mcp4728 = NULL;
    didcot_dac_t* dac = NULL;
    dteventlogger_t dac_delta_logger = { 0 };

    DTERR_C(dttasker_registry_init(tasker_registry));
    DTERR_C(dteventlogger_init(&dac_delta_logger, DAC_DELTA_EVENT_COUNT, sizeof(dteventlogger_item1_t)));

    {
        dtinterval_espidf_t* o = NULL;
        DTERR_C(dtinterval_espidf_create(&o));
        interval_handle = (dtinterval_handle)o;
        dtinterval_espidf_config_t c = { .name = TAG,
            .periodic_interval_micros = LOOP_INTERVAL_MILLISECONDS * 1000 };
        DTERR_C(dtinterval_espidf_configure((dtinterval_espidf_t*)interval_handle, &c));
    }

    // create and configure the platform-specific MCP4728 driver
    {
        dtmcp4728_espidf_t* o = NULL;
        DTERR_C(dtmcp4728_espidf_create(&o));
        mcp4728 = (dtmcp4728_handle)o;
        dtmcp4728_espidf_config_t c = { .i2c_port = DTMCP4728_DEFAULT_I2C_PORT,
            .sda_pin = DTMCP4728_DEFAULT_SDA_PIN,
            .scl_pin = DTMCP4728_DEFAULT_SCL_PIN,
            .clock_speed_hz = DTMCP4728_DEFAULT_I2C_CLOCK_HZ,
            .timeout_ms = DTMCP4728_DEFAULT_I2C_TIMEOUT_MS,
            .device_address_7bit = DTMCP4728_DEFAULT_I2C_ADDRESS,
            .enable_pullups = false,
            .install_driver = true };
        DTERR_C(dtmcp4728_espidf_configure(o, &c));
    }

    // create the platform-independent object that manages the DAC control loop
    DTERR_C(didcot_dac_create(&dac));
    didcot_dac_config_t dac_config = {
        //
        .interval_handle = interval_handle,
        .mcp4728 = mcp4728,
        .eventlogger = &dac_delta_logger,
    };
    DTERR_C(didcot_dac_configure(dac, &dac_config));

    // create the service registry and services manager, and add our DAC service to it
    DTERR_C(dtservice_registry_create(&service_registry));
    DTERR_C(dtservices_create(&services, service_registry));

    DTERR_C(dtservice_registry_add(service_registry,
      (dtservice_handle)dac,
      &(dtservice_registry_config_t){ .stack_size = 4096, .priority = DTTASKER_PRIORITY_NORMAL_HIGH, .core = 0 }));

    dtlog_info(TAG, "starting %d services...", service_registry->count);

    DTERR_C(dtservices_start(services));

    {
        char* s = NULL;
        DTERR_C(dtservices_concat_format(services, &s));
        dtlog_info(TAG, "Started services:\n%s", s);
        dtstr_dispose(s);
    }

    // get tasker registry info before entering the loop to store initial state of tasks
    DTERR_C(dtruntime_register_tasks(tasker_registry));

    {
        int32_t poll_iteration = 0;
        bool should_stop = false;

        while (!should_stop)
        {
            dtruntime_sleep_milliseconds(1000);
            DTERR_C(dtservices_poll(services, &should_stop));

            poll_iteration++;
            if (poll_iteration % 10 == 0)
            {
                char* s = NULL;
                DTERR_C(dteventlogger_printf_item1(&dac_delta_logger, "DAC timing", "callback µs", "write µs", &s));
                dtlog_info(TAG, "\n%s", s);
                dtstr_dispose(s);
                s = NULL;

                // update task timing numbers
                DTERR_C(dtruntime_register_tasks(tasker_registry));
                DTERR_C(dttasker_registry_format_as_table(tasker_registry, &s));
                dtlog_info(TAG, "Tasker registry at iteration %" PRId32 ":\n%s", poll_iteration, s);
                dtstr_dispose(s);
            }
        }
    }

    DTERR_C(dtservices_stop(services));
    DTERR_C(dtservices_join(services, 5000, NULL));

    {
        char* s = NULL;
        DTERR_C(dtservices_concat_format(services, &s));
        dtlog_info(TAG, "Services finished:\n%s", s);
        dtstr_dispose(s);
    }

cleanup:
    if (dterr != NULL)
    {
        dtlog_dterr(TAG, dterr);
        dterr_dispose(dterr);
        dterr = NULL;
    }

    dtservices_dispose(services);
    dtservice_registry_dispose(service_registry);

    didcot_dac_dispose(dac);

    dtinterval_dispose(interval_handle);
    dtmcp4728_dispose(mcp4728);
    dteventlogger_dispose(&dac_delta_logger);

    // loop forever so we can see the logs until the user resets the board
    while (true)
    {
        dtruntime_sleep_milliseconds(1000);
    }
}
