#include "copy.c"
#include "test.c"

static MunitResult test_copy_file(const MunitParameter params[], void *data) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *d = test_create_dir(c, "dir");

  copy_yank(f, false);
  copy_paste(d);

  usleep(TEST_SLEEP);

  test_assert_file_exists(c, "file");
  test_assert_file_exists(c, "dir/file");

  return test_cleanup_files(c);
}

static MunitResult test_copy_directory(const MunitParameter params[], void *data) {
  const char *c = __func__;

  char *src = test_create_dir(c, "source");
  test_create_file(c, "source/file");
  char *target = test_create_dir(c, "target");

  copy_yank(src, false);
  copy_paste(target);

  usleep(TEST_SLEEP);

  test_assert_file_exists(c, "source/file");
  test_assert_file_exists(c, "target/source/file");

  return test_cleanup_files(c);
}

static MunitResult test_move_file(const MunitParameter params[], void *data) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *target = test_create_dir(c, "target");

  copy_yank(f, true);
  copy_paste(target);

  usleep(TEST_SLEEP);

  test_assert_file_not_exists(c, "file");
  test_assert_file_exists(c, "target/file");

  return test_cleanup_files(c);
}

static MunitResult test_move_directory(const MunitParameter params[], void *data) {
  const char *c = __func__;

  char *src = test_create_dir(c, "source");
  test_create_file(c, "source/file");
  char *target = test_create_dir(c, "target");

  copy_yank(src, true);
  copy_paste(target);

  usleep(TEST_SLEEP);

  test_assert_file_not_exists(c, "source/file");
  test_assert_dir_not_exists(c, "source");
  test_assert_file_exists(c, "target/source/file");

  return test_cleanup_files(c);
}

static MunitResult test_copy_file_free_file_name_before_pasting(const MunitParameter params[], void *data) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *d = test_create_dir(c, "dir");

  copy_yank(f, false);
  free(f);
  copy_paste(d);

  usleep(TEST_SLEEP);

  test_assert_file_exists(c, "file");
  test_assert_file_exists(c, "dir/file");

  return test_cleanup_files(c);
}

static MunitResult test_copy_file_copy_twice(const MunitParameter params[], void *data) {
  return MUNIT_SKIP; // TODO
  // Expected behavior is to ignore the first copy
  const char *c = __func__;

  char *f1 = test_create_file(c, "file1");
  char *f2 = test_create_file(c, "file2");
  char *d = test_create_dir(c, "dir");

  copy_yank(f1, false);
  copy_yank(f2, false);
  copy_paste(d);

  usleep(TEST_SLEEP);

  test_assert_file_exists(c, "file1");
  test_assert_file_exists(c, "file2");
  test_assert_file_not_exists(c, "dir/file1");
  test_assert_file_exists(c, "dir/file2");

  return test_cleanup_files(c);
}

static MunitResult test_copy_file_paste_twice(const MunitParameter params[], void *data) {
  return MUNIT_SKIP; // TODO
  // Expected behavior is to fail on the second paste
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *d1 = test_create_dir(c, "dir1");
  char *d2 = test_create_dir(c, "dir2");

  copy_yank(f, false);
  copy_paste(d1);
  copy_paste(d2);

  usleep(TEST_SLEEP);

  test_assert_file_exists(c, "file");
  test_assert_file_exists(c, "dir1/file");
  test_assert_file_not_exists(c, "dir2/file");

  return test_cleanup_files(c);
}

MunitTest copy_tests[] = {
    {"/copy_file", test_copy_file, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/copy_directory", test_copy_directory, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/move_file", test_move_file, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/move_directory", test_move_directory, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

    // edge cases
    {"/copy_file_free_file_name_before_pasting", test_copy_file_free_file_name_before_pasting, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {"/copy_file_copy_twice", test_copy_file_copy_twice, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/copy_file_paste_twice", test_copy_file_paste_twice, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
