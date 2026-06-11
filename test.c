#pragma once

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static jmp_buf _test_jmp_buf;
static int _test_jmp_buf_valid;

#undef assert
#define assert(expr)                                                                                                   \
  do {                                                                                                                 \
    if (!(expr)) {                                                                                                     \
      fprintf(stderr, "assertion failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__);                                    \
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
      fprintf(stderr, "assertion failed: %s %s %s (%d %s %d) [%s:%d]\n", #a, #op, #b, _ta, #op, _tb, __FILE__,         \
              __LINE__);                                                                                               \
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
      fprintf(stderr, "assertion failed: string %s == %s (\"%s\" == \"%s\") [%s:%d]\n", #a, #b, _tsa, _tsb, __FILE__,  \
              __LINE__);                                                                                               \
      if (_test_jmp_buf_valid)                                                                                         \
        longjmp(_test_jmp_buf, 1);                                                                                     \
      abort();                                                                                                         \
    }                                                                                                                  \
  } while (0)

char *test_create_dir(const char *caller, char *path) {
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

char *test_create_file(const char *caller, char *path) {
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

void test_assert_dir_exists(const char *caller, char *path) {
  char full_path[500];
  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);

  struct stat st = {0};
  assert_int(stat(full_path, &st), ==, 0);
}

void test_assert_dir_not_exists(const char *caller, char *path) {
  char full_path[500];
  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);

  struct stat st = {0};
  assert_int(stat(full_path, &st), ==, -1);
}

void test_assert_file_exists(const char *caller, char *path) {
  char full_path[500];
  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);

  assert_int(access(full_path, F_OK), ==, 0);
}

void test_assert_file_not_exists(const char *caller, char *path) {
  char full_path[500];
  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);

  assert_int(access(full_path, F_OK), ==, -1);
}

void test_assert_string_contains(const char *s, const char *substring) {
  char *res = strstr(s, substring);
  if (res == NULL) {
    fprintf(stderr, "'%s' doesn't contain '%s'\n", s, substring);
    assert(res != NULL);
  }
}

void test_cleanup_files(const char *caller) {
  char full_path[500];
  sprintf(full_path, "%s/%s", "/tmp", caller);

  pid_t pid = fork();
  if (pid == 0) {
    execl("/bin/rm", "/bin/rm", "-r", full_path, (char *)0);
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

static int run_suite(Suite *suite) {
  int passed = 0;
  int failed = 0;

  for (int i = 0; suite->tests[i].name != NULL; i++) {
    printf("%s %s ... ", suite->prefix, suite->tests[i].name);
    fflush(stdout);

    if (setjmp(_test_jmp_buf) == 0) {
      _test_jmp_buf_valid = 1;
      suite->tests[i].func();
      _test_jmp_buf_valid = 0;
      printf("ok\n");
      passed++;
    } else {
      _test_jmp_buf_valid = 0;
      printf("FAIL\n");
      failed++;
    }
  }

  if (failed > 0) {
    printf("FAILED (%d/%d)\n", passed, passed + failed);
  } else {
    printf("PASSED (%d/%d)\n", passed, passed + failed);
  }

  return failed;
}

static int run_all(Suite *suites) {
  int total_failed = 0;
  for (int i = 0; suites[i].prefix != NULL; i++) {
    total_failed += run_suite(&suites[i]);
  }
  return total_failed;
}
