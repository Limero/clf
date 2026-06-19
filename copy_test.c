#define CLIPBOARD_FILE "/tmp/clf-test-clipboard"
#include "copy.c"
#include "test.c"

static void test_copy_file(void) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *d = test_create_dir(c, "dir");

  copy_yank((const char *[]){f}, 1, false);
  copy_paste(d);

  test_assert_file_exists(c, "file");
  test_assert_file_exists(c, "dir/file");

  unlink(CLIPBOARD_FILE);
  test_cleanup_files(c);
}

static void test_copy_directory(void) {
  const char *c = __func__;

  char *src = test_create_dir(c, "source");
  test_create_file(c, "source/file");
  char *target = test_create_dir(c, "target");

  copy_yank((const char *[]){src}, 1, false);
  copy_paste(target);

  test_assert_file_exists(c, "source/file");
  test_assert_file_exists(c, "target/source/file");

  unlink(CLIPBOARD_FILE);
  test_cleanup_files(c);
}

static void test_move_file(void) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *target = test_create_dir(c, "target");

  copy_yank((const char *[]){f}, 1, true);
  copy_paste(target);

  test_assert_file_not_exists(c, "file");
  test_assert_file_exists(c, "target/file");

  unlink(CLIPBOARD_FILE);
  test_cleanup_files(c);
}

static void test_move_directory(void) {
  const char *c = __func__;

  char *src = test_create_dir(c, "source");
  test_create_file(c, "source/file");
  char *target = test_create_dir(c, "target");

  copy_yank((const char *[]){src}, 1, true);
  copy_paste(target);

  test_assert_file_not_exists(c, "source/file");
  test_assert_dir_not_exists(c, "source");
  test_assert_file_exists(c, "target/source/file");

  unlink(CLIPBOARD_FILE);
  test_cleanup_files(c);
}

static void test_copy_file_free_file_name_before_pasting(void) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *d = test_create_dir(c, "dir");

  copy_yank((const char *[]){f}, 1, false);
  free(f);
  copy_paste(d);

  test_assert_file_exists(c, "file");
  test_assert_file_exists(c, "dir/file");

  unlink(CLIPBOARD_FILE);
  test_cleanup_files(c);
}

static void test_copy_file_copy_twice(void) {
  const char *c = __func__;

  char *f1 = test_create_file(c, "file1");
  char *f2 = test_create_file(c, "file2");
  char *d = test_create_dir(c, "dir");

  copy_yank((const char *[]){f1}, 1, false);
  copy_yank((const char *[]){f2}, 1, false);
  copy_paste(d);

  test_assert_file_exists(c, "file1");
  test_assert_file_exists(c, "file2");
  test_assert_file_not_exists(c, "dir/file1");
  test_assert_file_exists(c, "dir/file2");

  unlink(CLIPBOARD_FILE);
  test_cleanup_files(c);
}

static void test_copy_file_paste_twice(void) {
  const char *c = __func__;

  char *f = test_create_file(c, "file");
  char *d1 = test_create_dir(c, "dir1");
  char *d2 = test_create_dir(c, "dir2");

  copy_yank((const char *[]){f}, 1, false);
  copy_paste(d1);
  copy_paste(d2);

  test_assert_file_exists(c, "file");
  test_assert_file_exists(c, "dir1/file");
  test_assert_file_exists(c, "dir2/file");

  unlink(CLIPBOARD_FILE);
  test_cleanup_files(c);
}

static void test_copy_multiple_files(void) {
  const char *c = __func__;

  char *f1 = test_create_file(c, "file1");
  char *f2 = test_create_file(c, "file2");
  char *d = test_create_dir(c, "dir");

  copy_yank((const char *[]){f1, f2}, 2, false);
  copy_paste(d);

  test_assert_file_exists(c, "file1");
  test_assert_file_exists(c, "file2");
  test_assert_file_exists(c, "dir/file1");
  test_assert_file_exists(c, "dir/file2");

  unlink(CLIPBOARD_FILE);
  test_cleanup_files(c);
}

static void test_move_multiple_files(void) {
  const char *c = __func__;

  char *f1 = test_create_file(c, "file1");
  char *f2 = test_create_file(c, "file2");
  char *target = test_create_dir(c, "target");

  copy_yank((const char *[]){f1, f2}, 2, true);
  copy_paste(target);

  test_assert_file_not_exists(c, "file1");
  test_assert_file_not_exists(c, "file2");
  test_assert_file_exists(c, "target/file1");
  test_assert_file_exists(c, "target/file2");

  unlink(CLIPBOARD_FILE);
  test_cleanup_files(c);
}

static void test_copy_file_collision(void) {
  const char *c = __func__;

  {
    // basic: file exists -> paste as file1
    char *src = test_create_file(c, "a_file");
    char *target = test_create_dir(c, "a_tgt");
    test_create_file(c, "a_tgt/a_file");

    copy_yank((const char *[]){src}, 1, false);
    copy_paste(target);

    test_assert_file_exists(c, "a_tgt/a_file");
    test_assert_file_exists(c, "a_tgt/a_file1");
  }

  {
    // increment: file and file1 exist -> paste as file2
    char *src = test_create_file(c, "b_file");
    char *target = test_create_dir(c, "b_tgt");
    test_create_file(c, "b_tgt/b_file");
    test_create_file(c, "b_tgt/b_file1");

    copy_yank((const char *[]){src}, 1, false);
    copy_paste(target);

    test_assert_file_exists(c, "b_tgt/b_file");
    test_assert_file_exists(c, "b_tgt/b_file1");
    test_assert_file_exists(c, "b_tgt/b_file2");
  }

  {
    // extension: color.c exists -> paste as color1.c
    char *src = test_create_file(c, "c_color.c");
    char *target = test_create_dir(c, "c_tgt");
    test_create_file(c, "c_tgt/c_color.c");

    copy_yank((const char *[]){src}, 1, false);
    copy_paste(target);

    test_assert_file_exists(c, "c_tgt/c_color.c");
    test_assert_file_exists(c, "c_tgt/c_color1.c");
    test_assert_file_not_exists(c, "c_tgt/c_color.c1");
  }

  {
    // no extension: Makefile exists -> paste as Makefile1
    char *src = test_create_file(c, "d_Makefile");
    char *target = test_create_dir(c, "d_tgt");
    test_create_file(c, "d_tgt/d_Makefile");

    copy_yank((const char *[]){src}, 1, false);
    copy_paste(target);

    test_assert_file_exists(c, "d_tgt/d_Makefile");
    test_assert_file_exists(c, "d_tgt/d_Makefile1");
  }

  {
    // leading dot: .hidden exists -> paste as .hidden1
    char *src = test_create_file(c, ".e_hidden");
    char *target = test_create_dir(c, "e_tgt");
    test_create_file(c, "e_tgt/.e_hidden");

    copy_yank((const char *[]){src}, 1, false);
    copy_paste(target);

    test_assert_file_exists(c, "e_tgt/.e_hidden");
    test_assert_file_exists(c, "e_tgt/.e_hidden1");
  }

  {
    // move with collision: source deleted, dest gets file1
    char *src = test_create_file(c, "f_file");
    char *target = test_create_dir(c, "f_tgt");
    test_create_file(c, "f_tgt/f_file");

    copy_yank((const char *[]){src}, 1, true);
    copy_paste(target);

    test_assert_file_not_exists(c, "f_file");
    test_assert_file_exists(c, "f_tgt/f_file");
    test_assert_file_exists(c, "f_tgt/f_file1");
  }

  {
    // repeated paste: each paste increments
    char *src = test_create_file(c, "g_file");
    char *target = test_create_dir(c, "g_tgt");
    test_create_file(c, "g_tgt/g_file");

    copy_yank((const char *[]){src}, 1, false);
    copy_paste(target);
    copy_paste(target);
    copy_paste(target);

    test_assert_file_exists(c, "g_tgt/g_file");
    test_assert_file_exists(c, "g_tgt/g_file1");
    test_assert_file_exists(c, "g_tgt/g_file2");
    test_assert_file_exists(c, "g_tgt/g_file3");
    test_assert_file_not_exists(c, "g_tgt/g_file4");
  }

  {
    // no clash: file1 exists but not file -> paste as file
    char *src = test_create_file(c, "h_file");
    char *target = test_create_dir(c, "h_tgt");
    test_create_file(c, "h_tgt/h_file1");

    copy_yank((const char *[]){src}, 1, false);
    copy_paste(target);

    test_assert_file_exists(c, "h_tgt/h_file");
    test_assert_file_exists(c, "h_tgt/h_file1");
  }

  {
    // directory collision: srcdir exists -> paste as srcdir1
    char *src_dir = test_create_dir(c, "i_srcdir");
    test_create_file(c, "i_srcdir/inner");
    char *target = test_create_dir(c, "i_tgt");
    test_create_dir(c, "i_tgt/i_srcdir");

    copy_yank((const char *[]){src_dir}, 1, false);
    copy_paste(target);

    test_assert_file_exists(c, "i_tgt/i_srcdir");
    test_assert_file_exists(c, "i_tgt/i_srcdir1/inner");
  }

  unlink(CLIPBOARD_FILE);
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

    // multi-file
    {"/copy_multiple_files", test_copy_multiple_files},
    {"/move_multiple_files", test_move_multiple_files},

    // collision handling
    {"/collision", test_copy_file_collision},

    {NULL, NULL},
};
