#include <string.h>

#include <dtcore/dterr.h>
#include <dtcore/dtstr.h>
#include <dtcore/dtunittest.h>

#include <dtmc_base/dtinterval_scheduled.h>
#include <dtmc_base/dtmcp4728_dummy.h>
#include <dtmc_base/dtruntime.h>
#include <dtmc_base/dttasker.h>
#include <dtmc_services/dtservice_registry.h>
#include <dtmc_services/dtservices.h>

#include <didcot/didcot_dac.h>

// --------------------------------------------------------------------------------------------
// Happy path: create + configure + dispose
//
// Verifies that didcot_dac_configure wires the four timeseries (steady DC, sine,
// beat, browngrav) without error and without touching hardware. The mcp4728 handle is
// stored during configure and the device is only attached later in the entrypoint, so
// the dummy's attach_call_count must remain zero after configure.

static dterr_t*
test_didcot_dac_configure(void)
{
    dterr_t* dterr = NULL;
    dtmcp4728_dummy_t* dac_hw = NULL;
    didcot_dac_t* dac = NULL;

    // Build the hardware test double. Leave it configured-but-not-attached;
    // the entrypoint is responsible for attaching.
    DTERR_C(dtmcp4728_dummy_create(&dac_hw));
    dtmcp4728_dummy_config_t hw_cfg = { .device_address_7bit = 0x60 };
    DTERR_C(dtmcp4728_dummy_configure(dac_hw, &hw_cfg));

    // Create and configure the DAC object.
    DTERR_C(didcot_dac_create(&dac));

    didcot_dac_config_t dac_cfg = { .mcp4728 = (dtmcp4728_handle)dac_hw };
    DTERR_C(didcot_dac_configure(dac, &dac_cfg));

    // configure must not have touched the hardware — attach belongs to the entrypoint.
    DTUNITTEST_ASSERT_INT(dac_hw->attach_call_count, ==, 0);
    DTUNITTEST_ASSERT_INT(dac_hw->fast_write_call_count, ==, 0);
    DTUNITTEST_ASSERT_INT(dac_hw->sequential_write_call_count, ==, 0);

    // The dummy should still be in its pre-attach state.
    DTUNITTEST_ASSERT_TRUE(dac_hw->is_configured);
    DTUNITTEST_ASSERT_TRUE(!dac_hw->is_attached);

cleanup:
    didcot_dac_dispose(dac);
    dtmcp4728_dummy_dispose(dac_hw);
    return dterr;
}

static bool
test_didcot_dac_wait_for_fast_writes(dtmcp4728_dummy_t* dac_hw,
  int32_t minimum_fast_write_count,
  dtruntime_milliseconds_t timeout_milliseconds)
{
    dtruntime_milliseconds_t deadline = dtruntime_now_milliseconds() + timeout_milliseconds;

    while (dac_hw->fast_write_call_count < minimum_fast_write_count)
    {
        if (dtruntime_now_milliseconds() >= deadline)
            return false;

        dtruntime_sleep_milliseconds(5);
    }

    return true;
}

// --------------------------------------------------------------------------------------------
// Entrypoint runs, fires several intervals, stops cleanly on didcot_dac_stop().
//
// Starts didcot_dac_entrypoint in a task, waits for it to signal ready, then
// waits until at least 3 interval callbacks have actually reached the hardware
// before calling didcot_dac_stop(). Verifies the task exits cleanly and that the
// dummy records the expected calls.

static dterr_t*
test_didcot_dac_entrypoint_stop(void)
{
    dterr_t* dterr = NULL;
    dtmcp4728_dummy_t* dac_hw = NULL;
    didcot_dac_t* dac = NULL;
    dttasker_handle task = NULL;
    dtinterval_scheduled_t* interval = NULL;

    DTERR_C(dtmcp4728_dummy_create(&dac_hw));
    dtmcp4728_dummy_config_t hw_cfg = { .device_address_7bit = 0x60 };
    DTERR_C(dtmcp4728_dummy_configure(dac_hw, &hw_cfg));

    DTERR_C(dtinterval_scheduled_create(&interval));
    DTERR_C(dtinterval_scheduled_configure(interval, &(dtinterval_scheduled_config_t){ .interval_milliseconds = 10 }));

    DTERR_C(didcot_dac_create(&dac));
    didcot_dac_config_t dac_cfg = {
        .mcp4728 = (dtmcp4728_handle)dac_hw,
        .interval_handle = (dtinterval_handle)interval,
    };
    DTERR_C(didcot_dac_configure(dac, &dac_cfg));

    dttasker_config_t task_cfg = {
        .name = "dac_test",
        .tasker_entry_point_fn = didcot_dac_entrypoint,
        .tasker_entry_point_arg = dac,
        .stack_size = 8192,
        .priority = DTTASKER_PRIORITY_NORMAL_MEDIUM,
    };
    DTERR_C(dttasker_create(&task, &task_cfg));

    // blocks until didcot_dac_entrypoint calls dttasker_ready()
    DTERR_C(dttasker_start(task));

    DTUNITTEST_ASSERT_TRUE(test_didcot_dac_wait_for_fast_writes(dac_hw, 3, 250));

    didcot_dac_stop(dac);

    bool was_timeout = false;
    DTERR_C(dttasker_join(task, 1000, &was_timeout));
    DTUNITTEST_ASSERT_TRUE(!was_timeout);

    dttasker_info_t info;
    DTERR_C(dttasker_get_info(task, &info));
    DTUNITTEST_ASSERT_INT(info.status, ==, STOPPED);

    DTUNITTEST_ASSERT_INT(dac_hw->attach_call_count, ==, 1);
    DTUNITTEST_ASSERT_INT(dac_hw->detach_call_count, ==, 1);
    DTUNITTEST_ASSERT_INT(dac_hw->fast_write_call_count, >=, 3);

cleanup:
    dttasker_dispose(task);
    didcot_dac_dispose(dac);
    dtinterval_scheduled_dispose(interval);
    dtmcp4728_dummy_dispose(dac_hw);
    return dterr;
}

// --------------------------------------------------------------------------------------------
// Service-manager happy path: dtservices starts, monitors, and stops the DAC service cleanly.

static dterr_t*
test_didcot_dac_dtservices_stop(void)
{
    dterr_t* dterr = NULL;
    dtmcp4728_dummy_t* dac_hw = NULL;
    didcot_dac_t* dac = NULL;
    dtservice_registry_t* service_registry = NULL;
    dtservices_t* services = NULL;
    char* formatted = NULL;
    dtinterval_scheduled_t* interval = NULL;

    DTERR_C(dtmcp4728_dummy_create(&dac_hw));
    dtmcp4728_dummy_config_t hw_cfg = { .device_address_7bit = 0x60 };
    DTERR_C(dtmcp4728_dummy_configure(dac_hw, &hw_cfg));

    DTERR_C(dtinterval_scheduled_create(&interval));
    DTERR_C(dtinterval_scheduled_configure(interval, &(dtinterval_scheduled_config_t){ .interval_milliseconds = 10 }));

    DTERR_C(didcot_dac_create(&dac));
    didcot_dac_config_t dac_cfg = {
        .mcp4728 = (dtmcp4728_handle)dac_hw,
        .interval_handle = (dtinterval_handle)interval,
    };
    DTERR_C(didcot_dac_configure(dac, &dac_cfg));

    DTERR_C(dtservice_registry_create(&service_registry));
    DTERR_C(dtservices_create(&services, service_registry));
    DTERR_C(dtservice_registry_add(service_registry,
      (dtservice_handle)dac,
      &(dtservice_registry_config_t){ .stack_size = 8192, .priority = DTTASKER_PRIORITY_NORMAL_MEDIUM, .core = 0 }));

    DTERR_C(dtservices_start(services));

    DTUNITTEST_ASSERT_TRUE(test_didcot_dac_wait_for_fast_writes(dac_hw, 3, 250));

    bool should_stop = false;
    DTERR_C(dtservices_poll(services, &should_stop));
    DTUNITTEST_ASSERT_TRUE(!should_stop);

    dtservice_status_t status = { 0 };
    DTERR_C(didcot_dac_get_status(dac, &status));
    DTUNITTEST_ASSERT_INT(status.state, ==, DTSERVICE_STATE_ACTIVE);

    DTERR_C(dtservices_concat_format(services, &formatted));
    DTUNITTEST_ASSERT_TRUE(strstr(formatted, "didcot_dac") != NULL);

    DTERR_C(dtservices_stop(services));

    bool was_timeout = false;
    DTERR_C(dtservices_join(services, 1000, &was_timeout));
    DTUNITTEST_ASSERT_TRUE(!was_timeout);

    DTERR_C(didcot_dac_get_status(dac, &status));
    DTUNITTEST_ASSERT_INT(status.state, ==, DTSERVICE_STATE_STOPPED);

    DTUNITTEST_ASSERT_INT(dac_hw->attach_call_count, ==, 1);
    DTUNITTEST_ASSERT_INT(dac_hw->detach_call_count, ==, 1);
    DTUNITTEST_ASSERT_INT(dac_hw->fast_write_call_count, >=, 3);

cleanup:
    dtstr_dispose(formatted);
    dtservices_dispose(services);
    dtservice_registry_dispose(service_registry);
    didcot_dac_dispose(dac);
    dtinterval_scheduled_dispose(interval);
    dtmcp4728_dummy_dispose(dac_hw);
    return dterr;
}

// --------------------------------------------------------------------------------------------

void
test_didcot_dac(DTUNITTEST_SUITE_ARGS)
{
    DTUNITTEST_RUN_TEST(test_didcot_dac_configure);
    DTUNITTEST_RUN_TEST(test_didcot_dac_entrypoint_stop);
    DTUNITTEST_RUN_TEST(test_didcot_dac_dtservices_stop);
}
