#include <dtcore/dtunittest.h>

#include <didcot_tests.h>

int
main(int argc, char* argv[])
{
    dtunittest_control_t unittest_control = { 0 };
    unittest_control.should_print_suites = true;
    unittest_control.should_print_tests = false;
    unittest_control.should_print_errors = true;

    if (argc > 1)
        unittest_control.pattern = argv[1];

    test_didcot_matching(&unittest_control);

    dtunittest_print_final(&unittest_control);

    int rc = unittest_control.total_fail_count > 0 ? 1 : 0;
    exit(rc);
}
