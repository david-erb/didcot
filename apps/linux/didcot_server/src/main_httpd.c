
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dtcore/dterr.h>
#include <dtcore/dtlog.h>
#include <dtcore/dtrpc_registry.h>
#include <dtcore/dtstr.h>

#include <didcot/didcot.h>

// concrete objects used by this build
#include <dtmc/dthttpd_linux_socket.h>
#include <dtmc/dtiox_linux_tty.h>
#include <dtmc_base/dtbufferqueue.h>
#include <dtmc_base/dthttpd.h>
#include <dtmc_base/dtiox.h>
#include <dtmc_base/dtuart_helpers.h>

// services used by this build
#include <dtmc_services/dtservice.h>
#include <dtmc_services/dtservice_httpd.h>
#include <dtmc_services/dtservice_iox2bq.h>
#include <dtmc_services/dtservice_registry.h>
#include <dtmc_services/dtservices.h>

#include "main.h"

#define TAG "main_httpd"

// --------------------------------------------------------------------------------------
static dterr_t*
main__post_callback(void* context,
  const char* request_path,
  const dtbuffer_t* payload,
  dtbuffer_t** out_response,
  const char** out_content_type,
  int32_t* out_status_code)
{
    dterr_t* dterr = NULL;
    dtrpc_registry_t* rpc_registry = (dtrpc_registry_t*)context;
    dtkvp_list_t request_kvp_list = { 0 };
    dtkvp_list_t response_kvp_list = { 0 };
    dtrpc_handle rpc_handle = NULL;
    int32_t index = 0;
    bool was_refused = false;

    (void)payload;

    DTERR_C(dtkvp_list_init(&request_kvp_list));
    DTERR_C(dtkvp_list_set(&request_kvp_list, "endpoint", request_path));
    DTERR_C(dtkvp_list_init(&response_kvp_list));

    while (true)
    {
        DTERR_C(dtrpc_registry_get(rpc_registry, index, &rpc_handle));
        if (rpc_handle == NULL)
        {
            was_refused = true;
            break;
        }
        index++;
        DTERR_C(dtrpc_call(rpc_handle, &request_kvp_list, &was_refused, &response_kvp_list));
        if (!was_refused)
            break;
    }

    *out_status_code = was_refused ? 404 : 200;
    if (!was_refused)
    {
        *out_content_type = "text/plain";
        const char* body = NULL;
        DTERR_C(dtkvp_list_get(&response_kvp_list, "body", &body));
        if (body != NULL)
        {
            int32_t len = (int32_t)strlen(body);
            DTERR_C(dtbuffer_create(out_response, len));
            memcpy((*out_response)->payload, body, (size_t)len);
        }
    }

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
dterr_t*
main_httpd_setup(main_t* self,
  dtrpc_registry_t* rpc_registry,
  dtservice_registry_t* service_registry,
  dtservices_t* services,
  dthttpd_handle* httpserver_handle,
  dtservice_handle* httpd_service_handle)
{
    dterr_t* dterr = NULL;

    // -------------------------------------------------------------------------
    // http concrete object
    // -------------------------------------------------------------------------
    {
        dthttpd_linux_socket_t* o = NULL;
        DTERR_C(dthttpd_linux_socket_create(&o));
        *httpserver_handle = (dthttpd_handle)o;

        dthttpd_linux_socket_config_t c = { 0 };
        c.bind_host = self->config.server_config.bind_host;
        c.bind_port = self->config.server_config.bind_port;
        c.listen_backlog = self->config.server_config.listen_backlog;
        c.child_stack_size = self->config.server_config.child_stack_size;
        c.child_priority = self->config.server_config.child_priority;
        c.static_directories = self->config.server_config.static_directories;
        c.static_directory_count = self->config.server_config.static_directory_count;
        c.max_concurrent_connections = self->config.server_config.max_concurrent_connections;
        DTERR_C(dthttpd_linux_socket_configure(o, &c));
    }
    // -------------------------------------------------------------------------
    // RPC entries
    // -------------------------------------------------------------------------
    {
        dtrpc_handle h = NULL;
        DTERR_C(main_rpc_exit_create(&h, DIDCOT_RPC_EXIT_MODEL, *httpserver_handle));
        DTERR_C(dtrpc_registry_add(rpc_registry, h));
    }
    {
        dtrpc_handle h = NULL;
        DTERR_C(main_rpc_ps_create(&h, DIDCOT_RPC_PS_MODEL, services));
        DTERR_C(dtrpc_registry_add(rpc_registry, h));
    }

    DTERR_C(dthttpd_set_callback(*httpserver_handle, main__post_callback, rpc_registry));

    // -------------------------------------------------------------------------
    // Create and configure dtservice_httpd
    // -------------------------------------------------------------------------
    {
        dtservice_httpd_t* o = NULL;
        DTERR_C(dtservice_httpd_create(&o));
        *httpd_service_handle = (dtservice_handle)o;

        dtservice_httpd_config_t c = { 0 };
        c.httpserver_handle = *httpserver_handle;
        c.rpc_registry = rpc_registry;
        DTERR_C(dtservice_httpd_configure(o, &c));
    }

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------
void
main_httpd_teardown(main_t* self, dthttpd_handle httpserver_handle, dtservice_handle httpd_service_handle)
{
    dtservice_dispose(httpd_service_handle);
    dthttpd_dispose(httpserver_handle);
}