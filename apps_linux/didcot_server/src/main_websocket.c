
#include <dtcore/dterr.h>

#include <dtmc/dtiox_linux_websocket.h>
#include <dtmc_base/dtbufferqueue.h>
#include <dtmc_base/dtiox.h>

#include <dtmc_services/dtservice.h>
#include <dtmc_services/dtservice_bq2framed_iox.h>

#include "main.h"

// --------------------------------------------------------------------------------------
dterr_t*
main_websocket_setup(main_t* self,
  dtbufferqueue_handle adc_free_bq,
  dtbufferqueue_handle adc_full_bq,
  int32_t buffer_length,
  dtiox_handle* out_websocket_iox_handle,
  dtservice_handle* out_websocket_service_handle)
{
    dterr_t* dterr = NULL;

    {
        dtiox_linux_websocket_t* o = NULL;
        DTERR_C(dtiox_linux_websocket_create(&o));
        *out_websocket_iox_handle = (dtiox_handle)o;
        dtiox_linux_websocket_config_t c = { 0 };
        c.local_bind_host = self->config.ws_bind_host;
        c.local_bind_port = self->config.ws_bind_port;
        DTERR_C(dtiox_linux_websocket_configure(o, &c));
    }

    {
        dtservice_bq2framed_iox_t* o = NULL;
        DTERR_C(dtservice_bq2framed_iox_create(&o));
        *out_websocket_service_handle = (dtservice_handle)o;
        dtservice_bq2framed_iox_config_t c = { 0 };
        c.framer_topic = "adc_scan";
        c.free_bq = adc_free_bq;
        c.full_bq = adc_full_bq;
        c.iox_handle = *out_websocket_iox_handle;
        c.recovery_cooldown_ms = 1000;
        c.poll_timeout_ms = 100;
        DTERR_C(dtservice_bq2framed_iox_configure(o, &c));
    }

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
void
main_websocket_teardown(dtiox_handle websocket_iox_handle, dtservice_handle websocket_service_handle)
{
    dtservice_dispose(websocket_service_handle);
    dtiox_dispose(websocket_iox_handle);
}
