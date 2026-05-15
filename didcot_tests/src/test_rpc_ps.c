
#include <string.h>

#include <dtcore/dterr.h>
#include <dtcore/dtkvp.h>
#include <dtcore/dtrpc.h>
#include <dtcore/dtunittest.h>

#include <dtmc_services/dtservice_registry.h>
#include <dtmc_services/dtservices.h>

#include <didcot/didcot.h>
#include <didcot/rpc_ps.h>

// --------------------------------------------------------------------------------------------
// Refused when endpoint is not /ps.

static dterr_t*
test_rpc_ps_refuses(void)
{
    dterr_t* dterr = NULL;
    dtrpc_handle h = NULL;
    dtkvp_list_t req = { 0 };
    dtkvp_list_t resp = { 0 };
    dtservice_registry_t* service_registry = NULL;
    dtservices_t* services = NULL;
    bool was_refused = false;

    DTERR_C(dtservice_registry_create(&service_registry));
    DTERR_C(dtservices_create(&services, service_registry));
    DTERR_C(main_rpc_ps_create(&h, DIDCOT_RPC_PS_MODEL, services));
    DTERR_C(dtkvp_list_init(&req));
    DTERR_C(dtkvp_list_init(&resp));
    DTERR_C(dtkvp_list_set(&req, "endpoint", "/other"));

    DTERR_C(dtrpc_call(h, &req, &was_refused, &resp));
    DTUNITTEST_ASSERT_TRUE(was_refused);

cleanup:
    dtkvp_list_dispose(&req);
    dtkvp_list_dispose(&resp);
    if (h != NULL)
        dtrpc_dispose(h);
    dtservices_dispose(services);
    dtservice_registry_dispose(service_registry);
    return dterr;
}

// --------------------------------------------------------------------------------------------
// Responds to /ps with a body containing task and service sections.

static dterr_t*
test_rpc_ps_responds(void)
{
    dterr_t* dterr = NULL;
    dtrpc_handle h = NULL;
    dtkvp_list_t req = { 0 };
    dtkvp_list_t resp = { 0 };
    dtservice_registry_t* service_registry = NULL;
    dtservices_t* services = NULL;
    bool was_refused = false;
    const char* body = NULL;

    DTERR_C(dtservice_registry_create(&service_registry));
    DTERR_C(dtservices_create(&services, service_registry));

    DTERR_C(main_rpc_ps_create(&h, DIDCOT_RPC_PS_MODEL, services));
    DTERR_C(dtkvp_list_init(&req));
    DTERR_C(dtkvp_list_init(&resp));
    DTERR_C(dtkvp_list_set(&req, "endpoint", "/ps"));

    DTERR_C(dtrpc_call(h, &req, &was_refused, &resp));
    DTUNITTEST_ASSERT_TRUE(!was_refused);

    DTERR_C(dtkvp_list_get(&resp, "body", &body));
    DTUNITTEST_ASSERT_NOT_NULL(body);
    DTUNITTEST_ASSERT_HAS_SUBSTRING(body, "tasks:");
    DTUNITTEST_ASSERT_HAS_SUBSTRING(body, "services:");

cleanup:
    dtkvp_list_dispose(&req);
    dtkvp_list_dispose(&resp);
    if (h != NULL)
        dtrpc_dispose(h);
    dtservices_dispose(services);
    dtservice_registry_dispose(service_registry);
    return dterr;
}

// --------------------------------------------------------------------------------------------

void
test_rpc_ps(DTUNITTEST_SUITE_ARGS)
{
    DTUNITTEST_RUN_TEST(test_rpc_ps_refuses);
    DTUNITTEST_RUN_TEST(test_rpc_ps_responds);
}
