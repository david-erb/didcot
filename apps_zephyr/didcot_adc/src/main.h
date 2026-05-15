#pragma once

#include <stdint.h>

#include <dtcore/dterr.h>

#include <dtmc_base/dtadc.h>
#include <dtmc_base/dtbufferqueue.h>
#include <dtmc_base/dtiox.h>

#include <dtmc_services/dtservice.h>

extern dterr_t*
didcot_sensing_setup(dtbufferqueue_handle adc_free_bq,
  dtbufferqueue_handle adc_full_bq,
  int32_t allocated_buffer_length,
  dtadc_handle* out_adc_handle,
  dtservice_handle* out_adc_service_handle);

extern void
didcot_sensing_teardown(dtadc_handle adc_handle, dtservice_handle adc_service_handle);

extern dterr_t*
didcot_streaming_out_setup(dtbufferqueue_handle adc_free_bq,
  dtbufferqueue_handle adc_full_bq,
  int32_t buffer_length,
  dtiox_handle* out_transmitter_iox_handle,
  dtservice_handle* out_transmitter_service_handle);

extern void
didcot_streaming_out_teardown(dtiox_handle transmitter_iox_handle, dtservice_handle transmitter_service_handle);
