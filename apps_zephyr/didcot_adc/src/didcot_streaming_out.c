
#include <dtcore/dterr.h>

#include <dtmc/dtiox_zephyr_uartirq.h>
#include <dtmc_base/dtbufferqueue.h>
#include <dtmc_base/dtiox.h>
#include <dtmc_base/dtuart_helpers.h>

#include <dtmc_services/dtservice.h>
#include <dtmc_services/dtservice_bq2iox.h>

// --------------------------------------------------------------------------------------
dterr_t*
didcot_streaming_out_setup(dtbufferqueue_handle transmitter_free_bq,
  dtbufferqueue_handle transmitter_full_bq,
  int32_t allocated_buffer_length,
  dtiox_handle* out_transmitter_iox_handle,
  dtservice_handle* out_transmitter_service_handle)
{
    dterr_t* dterr = NULL;

    // -------------------------------------------------------------------------
    // the transmitter iox object is a uart
    // -------------------------------------------------------------------------
    {
        dtiox_zephyr_uartirq_t* o = NULL;
        DTERR_C(dtiox_zephyr_uartirq_create(&o));
        *out_transmitter_iox_handle = (dtiox_handle)o;
        dtiox_zephyr_uartirq_config_t c = { 0 };
        c.device_tree_name = "CDC_ACM_0";
        c.uart_config = dtuart_helper_default_config;
        c.tx_capacity = allocated_buffer_length;
        c.rx_capacity = 128;
        DTERR_C(dtiox_zephyr_uartirq_configure(o, &c));
    }

    {
        dtservice_bq2iox_config_t c = { 0 };
        c.iox_handle = *out_transmitter_iox_handle;
        c.free_bq = transmitter_free_bq;
        c.full_bq = transmitter_full_bq;
        c.recovery_cooldown_ms = 1000;
        c.poll_timeout_ms = 100;
        dtservice_bq2iox_t* o = NULL;
        DTERR_C(dtservice_bq2iox_create(&o));
        *out_transmitter_service_handle = (dtservice_handle)o;
        DTERR_C(dtservice_bq2iox_configure(o, &c));
    }

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
void
didcot_streaming_out_teardown(dtiox_handle transmitter_iox_handle, dtservice_handle transmitter_service_handle)
{
    dtservice_dispose(transmitter_service_handle);
    dtiox_dispose(transmitter_iox_handle);
}
