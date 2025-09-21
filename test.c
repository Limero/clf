#pragma once

#define MUNIT_ENABLE_ASSERT_ALIASES
#include "../include/munit.c"
#include <sys/stat.h>

#define TEST_SLEEP 100000 // 0.1s in microseconds

char *test_create_dir(const char *caller, char *path) {
  struct stat st = {0};
  char *full_path = munit_newa(char, 500);

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
  char *full_path = munit_newa(char, 500);

  sprintf(full_path, "%s/%s", "/tmp", caller);
  if (stat(full_path, &st) == -1) {
    assert_int(mkdir(full_path, 0700), ==, 0);
  }

  sprintf(full_path, "%s/%s/%s", "/tmp", caller, path);
  FILE *fptr;
  fptr = fopen(full_path, "w");
  assert_ptr_not_null(fptr);
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
    printf("'%s' doesn't contain '%s'\n", s, substring);
    assert(res != NULL);
  }
}

MunitResult test_cleanup_files(const char *caller) {
  char full_path[500];
  sprintf(full_path, "%s/%s", "/tmp", caller);

  pid_t pid = fork();
  if (pid == 0) {
    execl("/bin/rm", "/bin/rm", "-r", full_path, (char *)0);
  }

  return MUNIT_OK;
}
