
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dtcore/dterr.h>
#include <dtcore/dtlog.h>
#include <dtcore/dtstr.h>

// concrete objects used by this build
#include <dtmc/dthttpd_linux_socket.h>
#include <dtmc/dtiox_linux_tty.h>
#include <dtmc/dtiox_linux_websocket.h>
#include <dtmc_base/dtbufferqueue.h>
#include <dtmc_base/dtruntime.h>

#include <dtmc_base/dtiox.h>
#include <dtmc_base/dtuart_helpers.h>

// services used by this build
#include <dtmc_services/dtservice.h>
#include <dtmc_services/dtservice_bq2framed_iox.h>
#include <dtmc_services/dtservice_iox2bq.h>
#include <dtmc_services/dtservice_registry.h>
#include <dtmc_services/dtservices.h>

#include "main.h"

#define TAG "main"

// --------------------------------------------------------------------------------------
int
main(int argc, char* argv[])
{
    dterr_t* dterr = NULL;
    dtrpc_registry_t* rpc_registry = NULL;
    dtservice_registry_t* service_registry = NULL;
    dtservices_t* services = NULL;

    dthttpd_handle httpserver_handle = NULL;
    dtservice_handle httpd_service_handle = NULL;

    dtiox_handle adc_iox_handle = NULL;
    dtservice_handle adc_service_handle = NULL;

    dtiox_handle websocket_iox_handle = NULL;
    dtservice_handle websocket_service_handle = NULL;

#define ADC_BQ_POOL_SIZE 4
#define ADC_BQ_BUFFER_SIZE 2048
    dtbuffer_t* adc_buffers[ADC_BQ_POOL_SIZE] = { 0 };
    dtbufferqueue_handle adc_free_bq = NULL;
    dtbufferqueue_handle adc_full_bq = NULL;

    main_t _self = { 0 }, *self = &_self;

    // -------------------------------------------------------------------------
    // Parse command line
    // -------------------------------------------------------------------------
    DTERR_C(main_parse_args(argc, argv, &self->config));

    if (self->config.help_requested)
        goto cleanup;

    // -------------------------------------------------------------------------
    // Make a services regstry and a services manager to run them
    // -------------------------------------------------------------------------
    DTERR_C(dtservice_registry_create(&service_registry));
    DTERR_C(dtservices_create(&services, service_registry));

    // -------------------------------------------------------------------------
    // http concrete object and service setup
    // -------------------------------------------------------------------------

    DTERR_C(dtrpc_registry_create(&rpc_registry));
    DTERR_C(main_httpd_setup(self, rpc_registry, service_registry, services, &httpserver_handle, &httpd_service_handle));

    // -------------------------------------------------------------------------
    // ADC buffer queues (kept in main scope for use by downstream services)
    // -------------------------------------------------------------------------
    DTERR_C(dtbufferqueue_create(&adc_free_bq, ADC_BQ_POOL_SIZE, true));
    for (int i = 0; i < ADC_BQ_POOL_SIZE; i++)
    {
        DTERR_C(dtbuffer_create(&adc_buffers[i], ADC_BQ_BUFFER_SIZE));
        DTERR_C(dtbufferqueue_put(adc_free_bq, adc_buffers[i], DTTIMEOUT_NOWAIT, NULL));
    }
    DTERR_C(dtbufferqueue_create(&adc_full_bq, ADC_BQ_POOL_SIZE, true));

    // -------------------------------------------------------------------------
    // ADC iox object and iox2bq service
    // -------------------------------------------------------------------------
    DTERR_C(main_adc_setup(self, adc_free_bq, adc_full_bq, ADC_BQ_BUFFER_SIZE, &adc_iox_handle, &adc_service_handle));

    // -------------------------------------------------------------------------
    // WebSocket iox object and bq2framed_iox service
    // -------------------------------------------------------------------------
    DTERR_C(main_websocket_setup(
      self, adc_free_bq, adc_full_bq, ADC_BQ_BUFFER_SIZE, &websocket_iox_handle, &websocket_service_handle));

    // -------------------------------------------------------------------------
    // Register services and run (use system default priority, stack size and core affinity)
    // -------------------------------------------------------------------------
    DTERR_C(dtservice_registry_add(service_registry, httpd_service_handle, &(dtservice_registry_config_t){ 0 }));

    DTERR_C(dtservice_registry_add(service_registry, adc_service_handle, &(dtservice_registry_config_t){ 0 }));

    DTERR_C(dtservice_registry_add(service_registry, websocket_service_handle, &(dtservice_registry_config_t){ .stack_size = 32768 }));

    dtlog_info(TAG, "starting %d services...", service_registry->count);

    DTERR_C(dtservices_start(services));

    // -------------------------------------------------------------------------
    // Print what services are running
    // -------------------------------------------------------------------------
    {
        char* s = NULL;
        DTERR_C(dtservices_concat_format(services, &s));
        dtlog_info(TAG, "%s", s);
        dtstr_dispose(s);
    }

    {
        bool should_stop = false;
        while (!should_stop)
        {
            dtruntime_sleep_milliseconds(1000);
            DTERR_C(dtservices_poll(services, &should_stop));
        }
    }

    DTERR_C(dtservices_stop(services));

    DTERR_C(dtservices_join(services, 5000, NULL));

    // -------------------------------------------------------------------------
    // Report final status of services
    // -------------------------------------------------------------------------
    {
        char* s = NULL;
        DTERR_C(dtservices_concat_format(services, &s));
        dtlog_info(TAG, "%s", s);
        dtstr_dispose(s);
    }

cleanup:

    int rc = (dterr != NULL) ? -1 : 0;

    dtlog_dterr(TAG, dterr);
    dterr_dispose(dterr);

    dtservices_dispose(services);
    dtservice_registry_dispose(service_registry);
    dtrpc_registry_dispose(rpc_registry);

    main_websocket_teardown(websocket_iox_handle, websocket_service_handle);
    main_adc_teardown(adc_iox_handle, adc_service_handle);
    main_httpd_teardown(self, httpserver_handle, httpd_service_handle);

    dtbufferqueue_dispose(adc_full_bq);
    for (int i = 0; i < ADC_BQ_POOL_SIZE; i++)
        dtbuffer_dispose(adc_buffers[i]);
    dtbufferqueue_dispose(adc_free_bq);

    return rc;
}
