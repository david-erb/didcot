
#include <string.h>

#include <dtcore/dterr.h>
#include <dtcore/dtheaper.h>
#include <dtcore/dtkvp.h>
#include <dtcore/dtrpc.h>
#include <dtcore/dtstr.h>

#include <dtmc_base/dttasker_registry.h>

#include <dtmc_services/dtservices.h>

#include <didcot/rpc_ps.h>

#define TAG "main_rpc_ps"

typedef struct main_rpc_ps_t
{
    int32_t model_number;
    dtservices_t* services;
} main_rpc_ps_t;

DTRPC_DECLARE_API(main_rpc_ps);
DTRPC_INIT_VTABLE(main_rpc_ps);

// --------------------------------------------------------------------------------------
extern dterr_t*
main_rpc_ps_create(dtrpc_handle* rpc_handle, int32_t model_number, dtservices_t* services)
{
    dterr_t* dterr = NULL;
    main_rpc_ps_t* self = NULL;

    DTERR_ASSERT_NOT_NULL(rpc_handle);
    DTERR_ASSERT_NOT_NULL(services);

    DTERR_C(dtrpc_set_vtable(model_number, &main_rpc_ps_vt));
    DTERR_C(dtheaper_alloc_and_zero((int32_t)sizeof(main_rpc_ps_t), "main_rpc_ps_t", (void**)&self));

    self->model_number = model_number;
    self->services = services;

    *rpc_handle = (dtrpc_handle)self;
    self = NULL;

cleanup:
    if (self != NULL)
        dtheaper_free(self);
    return dterr;
}

// --------------------------------------------------------------------------------------
extern dterr_t*
main_rpc_ps_call(main_rpc_ps_t* self DTRPC_CALL_ARGS)
{
    dterr_t* dterr = NULL;
    char* s = NULL;
    char* services_str = NULL;
    char* g = "\n";

    DTERR_ASSERT_NOT_NULL(self);
    DTERR_ASSERT_NOT_NULL(request_kvp_list);
    DTERR_ASSERT_NOT_NULL(was_refused);
    DTERR_ASSERT_NOT_NULL(response_kvp_list);

    *was_refused = false;

    const char* endpoint = NULL;
    DTERR_C(dtkvp_list_get(request_kvp_list, "endpoint", &endpoint));
    if (endpoint == NULL || strcmp(endpoint, "/ps") != 0)
    {
        *was_refused = true;
        goto cleanup;
    }

    s = dtstr_concat_format(s, g, "tasks:");
    DTERR_C(dttasker_registry_format_as_table(&dttasker_registry_global_instance, &s));
    s = dtstr_concat_format(s, g, "\nservices:");

    DTERR_C(dtservices_flowmonitor(self->services, &services_str));
    s = dtstr_concat_format(s, g, services_str);

    DTERR_C(dtkvp_list_set(response_kvp_list, "body", s));

cleanup:
    dtstr_dispose(services_str);
    dtstr_dispose(s);
    return dterr;
}

// --------------------------------------------------------------------------------------
extern void
main_rpc_ps_dispose(main_rpc_ps_t* self)
{
    if (self != NULL)
        dtheaper_free(self);
}
