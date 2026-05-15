#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <dtcore/dterr.h>
#include <dtcore/dtrpc.h>
#include <dtcore/dtrpc_registry.h>

#include <dtmc_base/dtbufferqueue.h>
#include <dtmc_base/dtiox.h>

#include <dtmc_services/dtservice.h>
#include <dtmc_services/dtservice_registry.h>
#include <dtmc_services/dtservices.h>

#include <dtmc/dthttpd_linux_socket.h>

#define HTTP_BIND_HOST_DEFAULT "0.0.0.0"
#define HTTP_BIND_PORT_DEFAULT 14080
#define HTTP_LISTEN_BACKLOG_DEFAULT 16
#define HTTP_MAX_CONCURRENT_CONNECTIONS_DEFAULT 10
#define HTTP_STATIC_DIR_DEFAULT "./webroot"
#define HTTP_MAX_STATIC_DIRECTORIES 32

#define WS_BIND_HOST_DEFAULT "0.0.0.0"
#define WS_BIND_PORT_DEFAULT 14081

typedef struct main_config_t
{
    dthttpd_linux_socket_config_t server_config;

    bool help_requested;

    const char* webroots[HTTP_MAX_STATIC_DIRECTORIES];
    const char* adc_stream_device;

    const char* ws_bind_host;
    int32_t ws_bind_port;
} main_config_t;

typedef struct main_t
{
    main_config_t config;
} main_t;

extern dterr_t*
main_websocket_setup(main_t* self,
  dtbufferqueue_handle adc_free_bq,
  dtbufferqueue_handle adc_full_bq,
  int32_t buffer_length,
  dtiox_handle* out_websocket_iox_handle,
  dtservice_handle* out_websocket_service_handle);

extern void
main_websocket_teardown(dtiox_handle websocket_iox_handle, dtservice_handle websocket_service_handle);

extern dterr_t*
main_adc_setup(main_t* self,
  dtbufferqueue_handle adc_free_bq,
  dtbufferqueue_handle adc_full_bq,
  int32_t allocated_buffer_length,
  dtiox_handle* out_adc_iox_handle,
  dtservice_handle* out_adc_service_handle);

extern void
main_adc_teardown(dtiox_handle adc_iox_handle, dtservice_handle adc_service_handle);

extern dterr_t*
main_httpd_setup(main_t* self,
  dtrpc_registry_t* rpc_registry,
  dtservice_registry_t* service_registry,
  dtservices_t* services,
  dthttpd_handle* httpserver_handle,
  dtservice_handle* httpd_service_handle);

extern void
main_httpd_teardown(main_t* self, dthttpd_handle httpserver_handle, dtservice_handle httpd_service_handle);

// --------------------------------------------------------------------------------------
// usage

void
main_usage(const char* exe_name);

// --------------------------------------------------------------------------------------
// config init

void
main_config_init(main_config_t* cfg);

// --------------------------------------------------------------------------------------
// parser

dterr_t*
main_parse_args(int argc, char** argv, main_config_t* cfg);

// --------------------------------------------------------------------------------------
// RPC used to satisfy endpoints
#include <didcot/rpc_exit.h>
#include <didcot/rpc_ps.h>