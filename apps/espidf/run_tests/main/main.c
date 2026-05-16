#include <dtcore/dtunittest.h>

#include <dtmc_base/dtruntime.h>

#include <didcot_tests.h>

// --------------------------------------------------------------------------------------
void
app_main(void)
{
    dtunittest_control_t unittest_control = { 0 };
    unittest_control.should_print_suites = true;
    unittest_control.should_print_tests = false;
    unittest_control.should_print_errors = true;

    // if (unittest_control.pattern == NULL)
    //     unittest_control.pattern = "dtinterval";

    test_didcot_matching(&unittest_control);

    dtunittest_print_final(&unittest_control);

    printf(DTUNITTEST_FINAL_PRINTF_SENTINEL);

    while (true)
    {
        dtruntime_sleep_milliseconds(1000);
    }
}
