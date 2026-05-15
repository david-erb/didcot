
#include <stdbool.h>
#include <stdint.h>

#include <dtcore/dterr.h>
#include <dtcore/dtkvp.h>
#include <dtcore/dtrpc.h>
#include <dtcore/dtunittest.h>

#include <dtmc_base/dthttpd.h>
#include <dtmc_base/dtruntime.h>

#include <didcot/didcot.h>
#include <didcot/rpc_exit.h>

// --------------------------------------------------------------------------------------------
// Minimal dthttpd test double — records how many times stop was called.

#define TEST_HTTPD_MODEL 9901

typedef struct
{
    DTHTTPD_COMMON_MEMBERS
    int32_t stop_call_count;
} httpd_dummy_t;

static dterr_t* httpd_dummy_loop(httpd_dummy_t* self) { (void)self; return NULL; }
static dterr_t* httpd_dummy_stop(httpd_dummy_t* self) { self->stop_call_count++; return NULL; }
static dterr_t* httpd_dummy_join(httpd_dummy_t* self, dttimeout_millis_t t, bool* was_timeout)
    { (void)self; (void)t; *was_timeout = false; return NULL; }
static dterr_t* httpd_dummy_set_callback(httpd_dummy_t* self, dthttpd_post_callback_t cb, void* ctx)
    { (void)self; (void)cb; (void)ctx; return NULL; }
static dterr_t* httpd_dummy_concat_format(httpd_dummy_t* self, char* in, char* sep, char** out)
    { (void)self; (void)in; (void)sep; (void)out; return NULL; }
static void httpd_dummy_dispose(httpd_dummy_t* self) { (void)self; }

static dthttpd_vt_t httpd_dummy_vt = {
    .loop           = (dthttpd_loop_fn)httpd_dummy_loop,
    .stop           = (dthttpd_stop_fn)httpd_dummy_stop,
    .join           = (dthttpd_join_fn)httpd_dummy_join,
    .set_callback   = (dthttpd_set_callback_fn)httpd_dummy_set_callback,
    .concat_format  = (dthttpd_concat_format_fn)httpd_dummy_concat_format,
    .dispose        = (dthttpd_dispose_fn)httpd_dummy_dispose,
};

// --------------------------------------------------------------------------------------------
// Refused when endpoint is not /exit; stop is never called.

static dterr_t*
test_rpc_exit_refuses(void)
{
    dterr_t* dterr = NULL;
    dtrpc_handle h = NULL;
    dtkvp_list_t req = { 0 };
    dtkvp_list_t resp = { 0 };
    httpd_dummy_t httpd = { .model_number = TEST_HTTPD_MODEL };
    bool was_refused = false;

    DTERR_C(dthttpd_set_vtable(TEST_HTTPD_MODEL, &httpd_dummy_vt));
    DTERR_C(main_rpc_exit_create(&h, DIDCOT_RPC_EXIT_MODEL, (dthttpd_handle)&httpd));
    DTERR_C(dtkvp_list_init(&req));
    DTERR_C(dtkvp_list_init(&resp));
    DTERR_C(dtkvp_list_set(&req, "endpoint", "/other"));

    DTERR_C(dtrpc_call(h, &req, &was_refused, &resp));
    DTUNITTEST_ASSERT_TRUE(was_refused);
    DTUNITTEST_ASSERT_INT(httpd.stop_call_count, ==, 0);

cleanup:
    dtkvp_list_dispose(&req);
    dtkvp_list_dispose(&resp);
    if (h != NULL)
        dtrpc_dispose(h);
    return dterr;
}

// --------------------------------------------------------------------------------------------
// Accepted on /exit; fire-and-forget task calls dthttpd_stop after its 200 ms delay.

static dterr_t*
test_rpc_exit_stops(void)
{
    dterr_t* dterr = NULL;
    dtrpc_handle h = NULL;
    dtkvp_list_t req = { 0 };
    dtkvp_list_t resp = { 0 };
    httpd_dummy_t httpd = { .model_number = TEST_HTTPD_MODEL };
    bool was_refused = false;

    DTERR_C(dthttpd_set_vtable(TEST_HTTPD_MODEL, &httpd_dummy_vt));
    DTERR_C(main_rpc_exit_create(&h, DIDCOT_RPC_EXIT_MODEL, (dthttpd_handle)&httpd));
    DTERR_C(dtkvp_list_init(&req));
    DTERR_C(dtkvp_list_init(&resp));
    DTERR_C(dtkvp_list_set(&req, "endpoint", "/exit"));

    DTERR_C(dtrpc_call(h, &req, &was_refused, &resp));
    DTUNITTEST_ASSERT_TRUE(!was_refused);

    // fire-and-forget task sleeps 200 ms before calling dthttpd_stop
    dtruntime_sleep_milliseconds(350);
    DTUNITTEST_ASSERT_INT(httpd.stop_call_count, ==, 1);

cleanup:
    dtkvp_list_dispose(&req);
    dtkvp_list_dispose(&resp);
    if (h != NULL)
        dtrpc_dispose(h);
    return dterr;
}

// --------------------------------------------------------------------------------------------

void
test_rpc_exit(DTUNITTEST_SUITE_ARGS)
{
    DTUNITTEST_RUN_TEST(test_rpc_exit_refuses);
    DTUNITTEST_RUN_TEST(test_rpc_exit_stops);
}
