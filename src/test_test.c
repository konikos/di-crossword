#include "cwt_testlib.h"

TEST(test, should_pass) {
	cwt_assert(1 == 1, "This test should pass");
}

TEST(test, should_assert_fail) {
	cwt_assert(1 == 2, "This test should fail");
}

TEST(test, should_sigsegv_fail) {
	char *p = NULL;
	*p = '2';
}
