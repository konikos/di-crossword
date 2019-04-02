#ifndef CW_TEST_UTIL_H
#define CW_TEST_UTIL_H

#include <stdio.h>
#include <stdarg.h>
#include "util.h"

struct cwt_test_result {
	int failed;
};

struct cwt_test {
	struct cwt_test *next;

	const char *suite;
	const char *name;
	const char *filename;
	int lineno;
	void (*runner)(const struct cwt_test *, struct cwt_test_result *result);
};

void cwt_register_test(struct cwt_test *test);

#define __unused __attribute__((unused))

#define __CWT_TEST_TOK_CONCAT4(t1, t2, t3, t4) t1 ## t2 ## t3 ## t4
#define _CWT_TEST_TOK_CONCAT4(t1, t2, t3, t4) __CWT_TEST_TOK_CONCAT4(t1, t2, t3, t4)

#define _CWT_TEST_RUNNER_NAME(test_suite, name, line) \
	_CWT_TEST_TOK_CONCAT4(cwt_test_runner_, test_suite, name, line)
#define _CWT_TEST_REG_NAME(test_suite, name, line) \
	_CWT_TEST_TOK_CONCAT4(cwt_register_test_, test_suite, name, line)
#define _CWT_TEST_RUNNER_DECL(test_suite, name, line) \
	__unused static void _CWT_TEST_RUNNER_NAME(test_suite, name, line) (\
			__unused const struct cwt_test *_cur_test, \
			__unused struct cwt_test_result *_test_result)

#define _CWT_TEST_DEF_NAME(test_suite, name, line) \
	_CWT_TEST_TOK_CONCAT4(cwt_def_test_, test_suite, name, line)


#define TEST(test_suite, test_name) \
	_CWT_TEST_RUNNER_DECL(test_suite, test_name, __LINE__); \
	static struct cwt_test _CWT_TEST_DEF_NAME(test_suite, test_name, __LINE__) = { \
		.next = NULL, \
		.suite = #test_suite, \
		.name = #test_name, \
		.filename = __FILE__, \
		.lineno = __LINE__, \
		.runner = & _CWT_TEST_RUNNER_NAME(test_suite, test_name, __LINE__), \
	}; \
	static void __attribute__((constructor)) \
	_CWT_TEST_REG_NAME(test_suite, test_name, __LINE__) () { \
		struct cwt_test *t = & _CWT_TEST_DEF_NAME(test_suite, test_name, __LINE__); \
		cwt_register_test(t); \
	} \
	_CWT_TEST_RUNNER_DECL(test_suite, test_name, __LINE__)

#define cwt_fail() do { _test_result->failed = 1; return; } while (0)
#define cwt_succeed() do { _test_result->failed = 0; return; } while (0)

#define _cwt_assert(res, expr, got_fmt, got, expected_fmt, expected, fmt, ...) \
	do { \
		if (!(res)) { \
			printf("error at line %d: value of: %s\n", \
					__LINE__, \
					expr); \
			printf("     Got: " got_fmt "\n", got); \
			printf("Expected: " expected_fmt "\n", expected); \
			printf(fmt, ##__VA_ARGS__); \
			cwt_fail(); \
		} \
	} while (0)


#define cwt_assert(expr, fmt, ...) do { \
	typeof (expr) res = (expr); \
	_cwt_assert(!!res, STRINGIFY(expr), \
			"%s", "true", "%s", "false", \
			fmt, ##__VA_ARGS__); \
} while (0)

#define cwt_assert_eq(got, expected, fmt, ...) do { \
	typeof (got) _got = (got), _expected = (expected); \
	_cwt_assert(_got == _expected, STRINGIFY(got) "==" STRINGIFY(expected), \
			"%s", "true", "%s", "false", \
			fmt, ##__VA_ARGS__); \
} while (0)

#define cwt_assert_streq(got, expected, fmt, ...) do { \
	const char *_got = (got), *_expected = (expected); \
	_cwt_assert(strcmp(_got, _expected) == 0, STRINGIFY(got), \
			"%s", _got, "%s", _expected, \
			fmt, ##__VA_ARGS__); \
} while (0)

#endif
