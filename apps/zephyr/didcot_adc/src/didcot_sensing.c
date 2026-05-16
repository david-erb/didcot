
#include <dtcore/dterr.h>

#include <dtmc/dtadc_zephyr_saadc.h>
#include <dtmc_base/dtadc.h>
#include <dtmc_base/dtbufferqueue.h>
#include <dtmc_base/dttasker.h>

#include <dtmc_services/dtservice.h>
#include <dtmc_services/dtservice_adc2bq.h>

#define ADC_CHANNEL_COUNT 4

// --------------------------------------------------------------------------------------
dterr_t*
didcot_sensing_setup(dtbufferqueue_handle adc_free_bq,
  dtbufferqueue_handle adc_full_bq,
  int32_t allocated_buffer_length,
  dtadc_handle* out_adc_handle,
  dtservice_handle* out_adc_service_handle)
{
    dterr_t* dterr = NULL;

    // -------------------------------------------------------------------------
    // the adc object is the saadc implementation
    // -------------------------------------------------------------------------
    {
        dtadc_zephyr_saadc_t* o = NULL;
        DTERR_C(dtadc_zephyr_saadc_create(&o));
        *out_adc_handle = (dtadc_handle)o;
        dtadc_zephyr_saadc_config_t c = { 0 };
        c.channel_count = ADC_CHANNEL_COUNT;
        dtadc_zephyr_saadc_config_init_defaults(&c);
        c.scan_interval_ms = 10;
        c.task_priority = DTTASKER_PRIORITY_URGENT_MEDIUM;
        c.counter_dev = DEVICE_DT_GET(DT_NODELABEL(timer1));
        DTERR_C(dtadc_zephyr_saadc_configure(o, &c));
    }
    {
        dtservice_adc2bq_config_t c = { 0 };
        c.adc_handle = *out_adc_handle;
        c.framer_topic = "adc_scan";
        c.allocated_buffer_length = allocated_buffer_length;
        c.free_bq = adc_free_bq;
        c.full_bq = adc_full_bq;
        c.scan_count = 10;
        c.channel_count = ADC_CHANNEL_COUNT;
        c.recovery_cooldown_ms = 1000;
        c.poll_timeout_ms = 100;

        dtservice_adc2bq_t* o = NULL;
        DTERR_C(dtservice_adc2bq_create(&o));
        *out_adc_service_handle = (dtservice_handle)o;
        DTERR_C(dtservice_adc2bq_configure(o, &c));
    }

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
void
didcot_sensing_teardown(dtadc_handle adc_handle, dtservice_handle adc_service_handle)
{
    dtservice_dispose(adc_service_handle);
    dtadc_dispose(adc_handle);
}
