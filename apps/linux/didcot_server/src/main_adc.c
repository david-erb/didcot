
#include <dtcore/dterr.h>

#include <dtmc/dtiox_linux_tty.h>
#include <dtmc_base/dtbufferqueue.h>
#include <dtmc_base/dtiox.h>
#include <dtmc_base/dtuart_helpers.h>

#include <dtmc_services/dtservice.h>
#include <dtmc_services/dtservice_iox2bq.h>

#include "main.h"

// --------------------------------------------------------------------------------------
dterr_t*
main_adc_setup(main_t* self,
  dtbufferqueue_handle adc_free_bq,
  dtbufferqueue_handle adc_full_bq,
  int32_t allocated_buffer_length,
  dtiox_handle* out_adc_iox_handle,
  dtservice_handle* out_adc_service_handle)
{
    dterr_t* dterr = NULL;

    {
        dtiox_linux_tty_t* o = NULL;
        DTERR_C(dtiox_linux_tty_create(&o));
        *out_adc_iox_handle = (dtiox_handle)o;
        dtiox_linux_tty_config_t c = { 0 };
        c.device_path = self->config.adc_stream_device;
        c.uart_config = dtuart_helper_default_config;
        DTERR_C(dtiox_linux_tty_configure(o, &c));
    }

    {
        dtservice_iox2bq_t* o = NULL;
        DTERR_C(dtservice_iox2bq_create(&o));
        *out_adc_service_handle = (dtservice_handle)o;
        dtservice_iox2bq_config_t c = { 0 };
        c.iox_handle = *out_adc_iox_handle;
        c.allocated_buffer_length = allocated_buffer_length;
        c.free_bq = adc_free_bq;
        c.full_bq = adc_full_bq;
        c.recovery_cooldown_ms = 1000;
        c.poll_timeout_ms = 100;
        DTERR_C(dtservice_iox2bq_configure(o, &c));
    }

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
void
main_adc_teardown(dtiox_handle adc_iox_handle, dtservice_handle adc_service_handle)
{
    dtservice_dispose(adc_service_handle);
    dtiox_dispose(adc_iox_handle);
}
