
#include <string.h>

#include <dtcore/dterr.h>
#include <dtcore/dtheaper.h>
#include <dtcore/dtkvp.h>
#include <dtcore/dtrpc.h>

#include <dtmc_base/dthttpd.h>
#include <dtmc_base/dtruntime.h>
#include <dtmc_base/dttasker.h>

#include <didcot/rpc_exit.h>

#define TAG "main_rpc_exit"

typedef struct main_rpc_exit_t
{
    int32_t model_number;
    dthttpd_handle httpserver_handle;
    dttasker_handle stop_task;
} main_rpc_exit_t;

DTRPC_DECLARE_API(main_rpc_exit);
DTRPC_INIT_VTABLE(main_rpc_exit);

// --------------------------------------------------------------------------------------
// Runs in a fire-and-forget task; sleeps briefly so the HTTP response is sent
// before dthttpd_stop tears down the active connection slots.
static dterr_t*
rpc_exit__stop_entry(void* arg, dttasker_handle tasker_handle)
{
    dterr_t* dterr = NULL;
    DTERR_C(dttasker_ready(tasker_handle));
    dtruntime_sleep_milliseconds(200);
    dterr_dispose(dthttpd_stop((dthttpd_handle)arg));
cleanup:
    return NULL;
}

// --------------------------------------------------------------------------------------
extern dterr_t*
main_rpc_exit_create(dtrpc_handle* rpc_handle, int32_t model_number, dthttpd_handle httpserver_handle)
{
    dterr_t* dterr = NULL;
    main_rpc_exit_t* self = NULL;

    DTERR_ASSERT_NOT_NULL(rpc_handle);

    DTERR_C(dtrpc_set_vtable(model_number, &main_rpc_exit_vt));
    DTERR_C(dtheaper_alloc_and_zero((int32_t)sizeof(main_rpc_exit_t), "main_rpc_exit_t", (void**)&self));

    self->model_number = model_number;
    self->httpserver_handle = httpserver_handle;

    *rpc_handle = (dtrpc_handle)self;
    self = NULL;

cleanup:
    if (self != NULL)
        dtheaper_free(self);
    return dterr;
}

// --------------------------------------------------------------------------------------
extern dterr_t*
main_rpc_exit_call(main_rpc_exit_t* self DTRPC_CALL_ARGS)
{
    dterr_t* dterr = NULL;
    dttasker_handle stop_tasker = NULL;

    DTERR_ASSERT_NOT_NULL(self);
    DTERR_ASSERT_NOT_NULL(request_kvp_list);
    DTERR_ASSERT_NOT_NULL(was_refused);
    DTERR_ASSERT_NOT_NULL(response_kvp_list);

    *was_refused = false;

    const char* endpoint = NULL;
    DTERR_C(dtkvp_list_get(request_kvp_list, "endpoint", &endpoint));
    if (endpoint == NULL || strcmp(endpoint, "/exit") != 0)
    {
        *was_refused = true;
        goto cleanup;
    }

    {
        dttasker_config_t c = { 0 };
        c.tasker_entry_point_fn = rpc_exit__stop_entry;
        c.tasker_entry_point_arg = self->httpserver_handle;
        c.name = "rpc_exit_stop";
        c.stack_size = 4096;
        c.priority = DTTASKER_PRIORITY_NORMAL_LOW;
        DTERR_C(dttasker_create(&stop_tasker, &c));
        DTERR_C(dttasker_start(stop_tasker));
        self->stop_task = stop_tasker;
        stop_tasker = NULL;
    }

cleanup:
    dttasker_dispose(stop_tasker); // only runs if create succeeded but start failed
    return dterr;
}

// --------------------------------------------------------------------------------------
extern void
main_rpc_exit_dispose(main_rpc_exit_t* self)
{
    if (self == NULL)
        return;
    if (self->stop_task != NULL)
    {
        bool was_timeout = false;
        dterr_dispose(dttasker_join(self->stop_task, 500, &was_timeout));
        dttasker_dispose(self->stop_task);
    }
    dtheaper_free(self);
}
