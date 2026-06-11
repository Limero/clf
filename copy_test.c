#define CLIPBOARD_FILE "/tmp/clf-test-clipboard"
#include "copy.c"
#include "test.c"

static void test_copy_file(void) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *d = test_create_dir(c, "dir");

  copy_yank(f, false);
  copy_paste(d);

  test_assert_file_exists(c, "file");
  test_assert_file_exists(c, "dir/file");

  test_cleanup_files(c);
}

static void test_copy_directory(void) {
  const char *c = __func__;

  char *src = test_create_dir(c, "source");
  test_create_file(c, "source/file");
  char *target = test_create_dir(c, "target");

  copy_yank(src, false);
  copy_paste(target);

  test_assert_file_exists(c, "source/file");
  test_assert_file_exists(c, "target/source/file");

  test_cleanup_files(c);
}

static void test_move_file(void) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *target = test_create_dir(c, "target");

  copy_yank(f, true);
  copy_paste(target);

  test_assert_file_not_exists(c, "file");
  test_assert_file_exists(c, "target/file");

  test_cleanup_files(c);
}

static void test_move_directory(void) {
  const char *c = __func__;

  char *src = test_create_dir(c, "source");
  test_create_file(c, "source/file");
  char *target = test_create_dir(c, "target");

  copy_yank(src, true);
  copy_paste(target);

  test_assert_file_not_exists(c, "source/file");
  test_assert_dir_not_exists(c, "source");
  test_assert_file_exists(c, "target/source/file");

  test_cleanup_files(c);
}

static void test_copy_file_free_file_name_before_pasting(void) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *d = test_create_dir(c, "dir");

  copy_yank(f, false);
  free(f);
  copy_paste(d);

  test_assert_file_exists(c, "file");
  test_assert_file_exists(c, "dir/file");

  test_cleanup_files(c);
}

static void test_copy_file_copy_twice(void) {
  const char *c = __func__;

  char *f1 = test_create_file(c, "file1");
  char *f2 = test_create_file(c, "file2");
  char *d = test_create_dir(c, "dir");

  copy_yank(f1, false);
  copy_yank(f2, false);
  copy_paste(d);

  test_assert_file_exists(c, "file1");
  test_assert_file_exists(c, "file2");
  test_assert_file_not_exists(c, "dir/file1");
  test_assert_file_exists(c, "dir/file2");

  test_cleanup_files(c);
}

static void test_copy_file_paste_twice(void) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *d1 = test_create_dir(c, "dir1");
  char *d2 = test_create_dir(c, "dir2");

  copy_yank(f, false);
  copy_paste(d1);
  copy_paste(d2);

  test_assert_file_exists(c, "file");
  test_assert_file_exists(c, "dir1/file");
  test_assert_file_exists(c, "dir2/file");

  test_cleanup_files(c);
}

Test copy_tests[] = {
    {"/copy_file", test_copy_file},
    {"/copy_directory", test_copy_directory},
    {"/move_file", test_move_file},
    {"/move_directory", test_move_directory},

    // edge cases
    {"/copy_file_free_file_name_before_pasting", test_copy_file_free_file_name_before_pasting},
    {"/copy_file_copy_twice", test_copy_file_copy_twice},
    {"/copy_file_paste_twice", test_copy_file_paste_twice},

    {NULL, NULL},
};
