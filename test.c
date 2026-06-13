#pragma once

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_RESET "\033[0m"

#define COL_OUT(c) (color_stdout() ? (c) : "")
#define COL_ERR(c) (color_stderr() ? (c) : "")

static int color_stdout(void) {
  static int c = -1;
  if (c == -1)
    c = isatty(STDOUT_FILENO);
  return c;
}

static int color_stderr(void) {
  static int c = -1;
  if (c == -1)
    c = isatty(STDERR_FILENO);
  return c;
}

static jmp_buf _test_jmp_buf;
static int _test_jmp_buf_valid;

#undef assert
#define assert(expr)                                                                                                   \
  do {                                                                                                                 \
    if (!(expr)) {                                                                                                     \
      fprintf(stderr, "%sassertion failed: %s (%s:%d)%s\n", COL_ERR(ANSI_RED), #expr, __FILE__, __LINE__,              \
              COL_ERR(ANSI_RESET));                                                                                    \
      if (_test_jmp_buf_valid)                                                                                         \
        longjmp(_test_jmp_buf, 1);                                                                                     \
      abort();                                                                                                         \
    }                                                                                                                  \
  } while (0)

#define assert_int(a, op, b)                                                                                           \
  do {                                                                                                                 \
    int _ta = (a);                                                                                                     \
    int _tb = (b);                                                                                                     \
    if (!(_ta op _tb)) {                                                                                               \
      fprintf(stderr, "%sassertion failed: %s %s %s (%d %s %d) [%s:%d]%s\n", COL_ERR(ANSI_RED), #a, #op, #b, _ta, #op, \
              _tb, __FILE__, __LINE__, COL_ERR(ANSI_RESET));                                                           \
      if (_test_jmp_buf_valid)                                                                                         \
        longjmp(_test_jmp_buf, 1);                                                                                     \
      abort();                                                                                                         \
    }                                                                                                                  \
  } while (0)

#define assert_string_equal(a, b)                                                                                      \
  do {                                                                                                                 \
    const char *_tsa = (a);                                                                                            \
    const char *_tsb = (b);                                                                                            \
    if (strcmp(_tsa, _tsb) != 0) {                                                                                     \
      fprintf(stderr, "%sassertion failed: string %s == %s (\"%s\" == \"%s\") [%s:%d]%s\n", COL_ERR(ANSI_RED), #a, #b, \
              _tsa, _tsb, __FILE__, __LINE__, COL_ERR(ANSI_RESET));                                                    \
      if (_test_jmp_buf_valid)                                                                                         \
        longjmp(_test_jmp_buf, 1);                                                                                     \
      abort();                                                                                                         \
    }                                                                                                                  \
  } while (0)

char *test_create_dir(const char *caller, const char *path) {
  struct stat st = {0};
  char *full_path = calloc(500, 1);

  sprintf(full_path, "%s/%s", "/tmp", caller);
  if (stat(full_path, &st) == -1) {
    assert_int(mkdir(full_path, 0700), ==, 0);
  }

  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);
  if (stat(full_path, &st) == -1) {
    assert_int(mkdir(full_path, 0700), ==, 0);
  }
  assert_int(stat(full_path, &st), ==, 0);

  return full_path;
}

char *test_create_file(const char *caller, const char *path) {
  struct stat st = {0};
  char *full_path = calloc(500, 1);

  sprintf(full_path, "%s/%s", "/tmp", caller);
  if (stat(full_path, &st) == -1) {
    assert_int(mkdir(full_path, 0700), ==, 0);
  }

  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);
  FILE *fptr;
  fptr = fopen(full_path, "w");
  assert(fptr != NULL);
  assert_int(fclose(fptr), ==, 0);
  assert_int(access(full_path, F_OK), ==, 0);
  return full_path;
}

void test_assert_dir_exists(const char *caller, const char *path) {
  char full_path[500];
  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);

  struct stat st = {0};
  assert_int(stat(full_path, &st), ==, 0);
}

void test_assert_dir_not_exists(const char *caller, const char *path) {
  char full_path[500];
  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);

  struct stat st = {0};
  assert_int(stat(full_path, &st), ==, -1);
}

void test_assert_file_exists(const char *caller, const char *path) {
  char full_path[500];
  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);

  assert_int(access(full_path, F_OK), ==, 0);
}

void test_assert_file_not_exists(const char *caller, const char *path) {
  char full_path[500];
  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);

  assert_int(access(full_path, F_OK), ==, -1);
}

void test_assert_string_contains(const char *s, const char *substring) {
  char *res = strstr(s, substring);
  if (res == NULL) {
    fprintf(stderr, "%s'%s' doesn't contain '%s'%s\n", COL_ERR(ANSI_RED), s, substring, COL_ERR(ANSI_RESET));
    assert(res != NULL);
  }
}

void test_cleanup_files(const char *caller) {
  char full_path[500];
  sprintf(full_path, "%s/%s", "/tmp", caller);

  pid_t pid = fork();
  if (pid == 0) {
    execl("/bin/rm", "/bin/rm", "-r", full_path, (char *)0);
  } else if (pid > 0) {
    waitpid(pid, NULL, 0);
  }
}

typedef void (*TestFunc)(void);

typedef struct {
  char *name;
  TestFunc func;
} Test;

typedef struct {
  char *prefix;
  Test *tests;
} Suite;

static int run_suite(const Suite *suite) {
  int passed = 0;
  int failed = 0;

  for (int i = 0; suite->tests[i].name != NULL; i++) {
    printf("%s %s ... ", suite->prefix, suite->tests[i].name);
    fflush(stdout);

    if (setjmp(_test_jmp_buf) == 0) {
      _test_jmp_buf_valid = 1;
      suite->tests[i].func();
      _test_jmp_buf_valid = 0;
      printf("%sok%s\n", COL_OUT(ANSI_GREEN), COL_OUT(ANSI_RESET));
      passed++;
    } else {
      _test_jmp_buf_valid = 0;
      printf("%sFAIL%s\n", COL_OUT(ANSI_RED), COL_OUT(ANSI_RESET));
      failed++;
    }
  }

  if (failed > 0) {
    printf("%sFAILED (%d/%d)%s\n", COL_OUT(ANSI_RED), passed, passed + failed, COL_OUT(ANSI_RESET));
  } else {
    printf("%sPASSED (%d/%d)%s\n", COL_OUT(ANSI_GREEN), passed, passed + failed, COL_OUT(ANSI_RESET));
  }

  return failed;
}

static int run_all(const Suite *suites) {
  int total_failed = 0;
  for (int i = 0; suites[i].prefix != NULL; i++) {
    total_failed += run_suite(&suites[i]);
  }
  return total_failed;
}
