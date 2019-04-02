#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <backtrace.h>
#include "cwt_testlib.h"

// Helper macro for dying when expr is < 0
#define die_iferr(expr, err) \
	die_if((expr) < 0, "%s: %s (errno = %d)", err, strerror(errno), errno);

typedef struct _test_result {
	struct cwt_test *test;

	char *log;
	struct {
		int failed;
		int exit_code;
		int signal;
	} status;

	struct _test_result *next;
} test_result;

static void free_test_result(test_result *result)
{
	free(result->log);
	free(result);
}

static struct cwt_test *registered_tests_head;

void cwt_register_test(struct cwt_test *test)
{
	test->next = registered_tests_head;
	registered_tests_head = test;
}

// Waits until child exits and sets `result.status`.
static void wait_for_child(test_result *result, pid_t child)
{
	int wait_status, err;
	err = waitpid(child, &wait_status, 0);
	die_iferr(err, "could not wait for test child process");
	if (WIFEXITED(wait_status)) {
		result->status.exit_code = WEXITSTATUS(wait_status);
		result->status.failed = result->status.exit_code != 0;
	}
	else if (WIFSIGNALED(wait_status)) {
		result->status.signal = WTERMSIG(wait_status);
		result->status.failed = 1;
	}
	else {
		die("test child exited with unknown status");
	}
}

// Returns a null-terminated string containing all the data that can be read
// from the pipe described by fd.
// Returns NULL if no data were read.
static char *readall_from_pipe(int fd)
{
	char *log = NULL;
	size_t log_size = 0;
	ssize_t read_len = 0;
	char buf[64 * 1024];
	while ((read_len = read(fd, buf, sizeof(buf))) > 0) {
		size_t new_log_size = log_size + (size_t) read_len;
		// Resize log including 1 byte for the string null-terminator.
		log = xrealloc(log, (new_log_size + 1) * sizeof(char));
		memmove(log + log_size, buf, (size_t) read_len);
		log_size = new_log_size;
	}
	die_iferr(read_len, "could not read from pipe");

	if (log) {
		log[log_size] = 0;
	}
	return log;
}

// Used by backtrace functions to report an errors.
static void bt_error_callback(__unused void *data, const char *msg, int err)
{
	fprintf(stderr, "backtrace error (%d): %s\n", err, msg);
}

static void forked_runner_sig_handler(
		__unused int signum, __unused siginfo_t *siginfo,
		__unused void *context)
{
	struct backtrace_state *bt = backtrace_create_state(
			NULL, 0, bt_error_callback, NULL);
	backtrace_print(bt, 0, stderr);
	exit(1);
}

// This is the entry function of the forked test runner process.
static int forked_runner_main(int pipe_fd[0], struct cwt_test *test)
{
	int err;

	// The read end is not used by the child.
	err = close(pipe_fd[0]);
	die_iferr(err, "could not close pipe read end");

	// Connect stdout and stderr to the pipe
	err = dup2(pipe_fd[1], 1);
	die_iferr(err, "could not connect pipe to stdin");
	err = dup2(pipe_fd[1], 2);
	die_iferr(err, "could not connect pipe to stderr");

	err = close(pipe_fd[1]);
	die_iferr(err, "could not close unused fd");

	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_flags = SA_SIGINFO;
	action.sa_sigaction = forked_runner_sig_handler;
	sigaction(SIGHUP, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGQUIT, &action, NULL);
	sigaction(SIGABRT, &action, NULL);
	sigaction(SIGFPE, &action, NULL);
	sigaction(SIGSEGV, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	struct cwt_test_result result = {0};
	test->runner(test, &result);
	return result.failed ? 1 : 0;
}

static test_result *run_test(struct cwt_test *test)
{
	int pipe_fd[2];
	int err = pipe(pipe_fd);
	die_iferr(err, "could not create test fork pipe");

	// Flush stdout and stderr to prevent forks from inheriting
	// any buffered output.
	fflush(stdout);
	fflush(stderr);

	int child = fork();
	die_iferr(child, "could not fork test child");
	if (child == 0) {
		int exit_code = forked_runner_main(pipe_fd, test);
		exit(exit_code);
	}

	err = close(pipe_fd[1]);
	die_iferr(err, "could not close pipe writing end");

	test_result *result = xmalloc(sizeof(test_result));
	*result = (test_result) {
		.test = test,
		.log = NULL,
		.status = {0},
		.next = NULL,
	};

	result->log = readall_from_pipe(pipe_fd[0]);
	wait_for_child(result, child);

	err = close(pipe_fd[0]);
	die_iferr(err, "could not close pipe reading end");

	return result;
}

// Prints a header encompassed by equal number of `pad` characters on the left
// and the right of the header's text.
static __printf_like(3, 4) void print_header(size_t header_len, char pad,
		const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int sprintf_ret = vsnprintf(NULL, 0, fmt, ap);
	die_if(sprintf_ret < 0, "sprintf returned < 0: %d", sprintf_ret);
	size_t text_len = (size_t)sprintf_ret;
	va_end(ap);

	header_len -= 2; // spaces left and right of `text`
	size_t left_pad_len = (header_len - text_len) / 2;
	size_t right_pad_len = header_len - text_len - left_pad_len;

	while (left_pad_len-- > 0) {
		putchar(pad);
	}

	putchar(' ');
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar(' ');

	while (right_pad_len-- > 0) {
		putchar(pad);
	}
	putchar('\n');
}

static size_t count_tests(const struct cwt_test *head)
{
	size_t count = 0;
	while (head) {
		count++;
		head = head->next;
	}
	return count;
}

static int compare_tests(struct cwt_test *t1, struct cwt_test *t2)
{
	int cmp = strcmp(t1->suite, t2->suite);
	if (cmp) {
		return cmp;
	}

	return strcmp(t1->name, t2->name);
}

static struct cwt_test *sort_tests(struct cwt_test *head, size_t length)
{
	if (!length) {
		return NULL;
	}

	if (length == 1) {
		head->next = NULL;
		return head;
	}

	size_t left_length = length / 2;

	struct cwt_test *right = head;
	for (size_t i = 0; i < left_length; i++) {
		right = right->next;
	}
	right = sort_tests(right, length - left_length);

	struct cwt_test *left = sort_tests(head, left_length);

	if (compare_tests(left, right) < 0) {
		head = left;
		left = left->next;
	}
	else {
		head = right;
		right = right->next;
	}
	head->next = NULL;

	struct cwt_test *cur = head;
	while (left && right) {
		if (compare_tests(left, right) < 0) {
			cur->next = left;
			left = left->next;
		}
		else {
			cur->next = right;
			right = right->next;
		}
		cur = cur->next;
		cur->next = NULL;
	}

	if (left) {
		cur->next = left;
	}
	else {
		cur->next = right;
	}

	return head;
}

static void print_results(struct cwt_test *tests_head,
		test_result *failed_results_head)
{
	const size_t header_len = 50;
	print_header(header_len, '=', "FAILURES");

	size_t failed_tests_count = 0;
	test_result *cur = failed_results_head;
	while (cur) {
		failed_tests_count++;
		print_header(header_len, '_', "%s :: %s",
				cur->test->suite, cur->test->name);
		printf("%s\n", cur->log ? cur->log : "");

		if (cur->status.signal) {
			printf("Terminated because of signal %d: %s\n",
					cur->status.signal,
					strsignal(cur->status.signal));
		}
		printf("\n\n");

		test_result *next = cur->next;
		free_test_result(cur);
		cur = next;
	}

	size_t tests_count = count_tests(tests_head);
	printf(">> %zu tests failed, %zu total.\n",
			failed_tests_count, tests_count);
}

static test_result *run_all_tests(struct cwt_test *tests_head)
{
	test_result *failed_results_head = NULL, *results_tail = NULL;
	struct cwt_test *cur = tests_head;
	while (cur) {
		test_result *result = run_test(cur);
		putchar(result->status.failed ? 'F' : '.');

		if (!result->status.failed) {
			free_test_result(result);
			cur = cur->next;
			continue;
		}

		if (!failed_results_head) {
			failed_results_head = results_tail = result;
		}
		else {
			results_tail->next = result;
			results_tail = result;
		}

		cur = cur->next;
	}
	return failed_results_head;
}

int main()
{
	registered_tests_head = sort_tests(registered_tests_head,
			count_tests(registered_tests_head));

	test_result *failed_results_head = run_all_tests(
			registered_tests_head);

	putchar('\n');
	print_results(registered_tests_head, failed_results_head);

	return 0;
}
