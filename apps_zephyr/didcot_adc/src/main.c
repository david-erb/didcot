
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dtcore/dterr.h>
#include <dtcore/dtlog.h>
#include <dtcore/dtstr.h>

// concrete objects used by this build
#include <dtmc/dtadc_zephyr_saadc.h>
#include <dtmc/dtiox_zephyr_uartirq.h>
#include <dtmc_base/dtbufferqueue.h>
#include <dtmc_base/dtruntime.h>

#include <dtmc_base/dtadc.h>
#include <dtmc_base/dtiox.h>
#include <dtmc_base/dtuart_helpers.h>

// services used by this build
#include <dtmc_services/dtservice.h>
#include <dtmc_services/dtservice_adc2bq.h>
#include <dtmc_services/dtservice_bq2iox.h>
#include <dtmc_services/dtservice_registry.h>
#include <dtmc_services/dtservices.h>

#include "main.h"

#define TAG "main"

// --------------------------------------------------------------------------------------
int
main(int argc, char* argv[])
{
    dterr_t* dterr = NULL;
    dttasker_registry_t* tasker_registry = &dttasker_registry_global_instance;
    dtservice_registry_t* service_registry = NULL;
    dtservices_t* services = NULL;

    dtadc_handle adc_iox_handle = NULL;
    dtservice_handle adc_service_handle = NULL;

    dtiox_handle transmitter_object_handle = NULL;
    dtservice_handle transmitter_service_handle = NULL;

#define ADC_BQ_POOL_SIZE 4
#define ADC_BQ_BUFFER_SIZE 2048
    dtbuffer_t* adc_buffers[ADC_BQ_POOL_SIZE] = { 0 };
    dtbufferqueue_handle adc_free_bq = NULL;
    dtbufferqueue_handle adc_full_bq = NULL;

    DTERR_C(dtcpu_sysinit());

    // -------------------------------------------------------------------------
    // Make a services registry and a services manager to run them
    // -------------------------------------------------------------------------
    DTERR_C(dttasker_registry_init(tasker_registry));
    DTERR_C(dtservice_registry_create(&service_registry));
    DTERR_C(dtservices_create(&services, service_registry));

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
    // ADC iox object and adc2bq service
    // -------------------------------------------------------------------------
    DTERR_C(didcot_sensing_setup(adc_free_bq, adc_full_bq, ADC_BQ_BUFFER_SIZE, &adc_iox_handle, &adc_service_handle));

    // -------------------------------------------------------------------------
    // UART iox object and bq2iox service
    // -------------------------------------------------------------------------
    DTERR_C(didcot_streaming_out_setup(
      adc_free_bq, adc_full_bq, ADC_BQ_BUFFER_SIZE, &transmitter_object_handle, &transmitter_service_handle));

    // -------------------------------------------------------------------------
    // Register services and run.
    //
    // nRF5340 note: the two M33 cores are separate firmware domains, not SMP
    // peers — core affinity has no effect here, leave at 0.
    //
    // ADC service: frames SAADC scans into the buffer queue.  The SAADC
    // background task runs at URGENT_HIGH; the framing service must drain its
    // output promptly or the pool fills and scans are dropped.  NORMAL_HIGH
    // keeps it above the TX path without competing with true interrupt work.
    // Stack: framer + buffer-queue ops + dtlog formatting ~ 2 kB.
    //
    // TX service: drains the full-buffer queue into the CDC-ACM ring buffer.
    // The uartirq driver handles the actual USB writes from its own ISR, so
    // this thread only feeds the ring buffer.  NORMAL_MEDIUM is sufficient;
    // up to 4 × 2048 B of buffering absorbs any scheduling jitter.
    // Stack: iox write path + buffer-queue ops + dtlog formatting ~ 2 kB.
    // -------------------------------------------------------------------------
    DTERR_C(dtservice_registry_add( //
      service_registry,
      adc_service_handle,
      &(dtservice_registry_config_t){ //
        .stack_size = 2048,
        .priority = DTTASKER_PRIORITY_URGENT_LOW }));

    DTERR_C(dtservice_registry_add( //
      service_registry,
      transmitter_service_handle,
      &(dtservice_registry_config_t){ //
        .stack_size = 2048,
        .priority = DTTASKER_PRIORITY_NORMAL_HIGH }));

    dtlog_info(TAG, "starting %d services...", service_registry->count);

    DTERR_C(dtservices_start(services));

    // -------------------------------------------------------------------------
    // Print what services are running
    // -------------------------------------------------------------------------
    {
        char* s = NULL;
        DTERR_C(dtservices_concat_format(services, &s));
        dtlog_info(TAG, "Started services:\n%s", s);
        dtstr_dispose(s);
    }

    {
        int32_t poll_iteration = 0;
        bool should_stop = false;

        // get tasker registry info before entering the loop to store initial state of tasks
        DTERR_C(dtruntime_register_tasks(tasker_registry));
        while (!should_stop)
        {
            dtruntime_sleep_milliseconds(1000);
            DTERR_C(dtservices_poll(services, &should_stop));

            char* s = NULL;
            DTERR_C(dtservices_flowmonitor(services, &s));
            dtlog_info(TAG, "%s", s);
            dtstr_dispose(s);

            poll_iteration++;
            if (poll_iteration % 10 == 0)
            {
                char* s = NULL;
                // update task timing numbers
                DTERR_C(dtruntime_register_tasks(tasker_registry));
                DTERR_C(dttasker_registry_format_as_table(tasker_registry, &s));
                dtlog_info(TAG, "Tasker registry at iteration %d:\n%s", (int)poll_iteration, s);
                dtstr_dispose(s);
            }
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

    didcot_streaming_out_teardown(transmitter_object_handle, transmitter_service_handle);
    didcot_sensing_teardown(adc_iox_handle, adc_service_handle);

    dtbufferqueue_dispose(adc_full_bq);
    for (int i = 0; i < ADC_BQ_POOL_SIZE; i++)
        dtbuffer_dispose(adc_buffers[i]);
    dtbufferqueue_dispose(adc_free_bq);

    dttasker_registry_dispose(tasker_registry);

    exit(rc);
}
