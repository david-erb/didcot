#include <dtcore/dtbuffer.h>
#include <dtcore/dterr.h>
#include <dtcore/dtheaper.h>
#include <dtcore/dtledger.h>
#include <dtcore/dtstr.h>
#include <dtcore/dtunittest.h>

#include <didcot_tests.h>

void
test_didcot_matching(DTUNITTEST_SUITE_ARGS)
{
    dtledger_t* ledgers[10] = { 0 };
    {
        int i = 0;
        ledgers[i++] = dterr_ledger;
        ledgers[i++] = dtstr_ledger;
        ledgers[i++] = dtbuffer_ledger;
        ledgers[i++] = dtheaper_ledger;
    }

    unittest_control->ledgers = ledgers;

    DTUNITTEST_RUN_SUITE(test_didcot_dac);
    DTUNITTEST_RUN_SUITE(test_rpc_ps);
    DTUNITTEST_RUN_SUITE(test_rpc_exit);
}
