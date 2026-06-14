#include "draw.c"
#include "test.c"

static void test_locate_forward(void) {
  const char *c = __func__;

  test_create_dir(c, "files");
  test_create_file(c, "files/apple");
  test_create_file(c, "files/avocado");
  test_create_file(c, "files/banana");
  test_create_file(c, "files/blueberry");
  test_create_file(c, "files/cherry");

  snprintf(g_cwd, sizeof g_cwd, "/tmp/%s/files", c);
  g_cursor.name[0] = '\0';
  g_update.dir_middle = true;
  reload_dir_middle();

  // Forward from start finds first 'a' (apple, alphabetically)
  assert(set_cursor_idx_to_locate(true, 0, 'a'));
  assert_string_equal(g_namelist_middle[g_cursor.idx]->d_name, "apple");

  // Forward from next finds second 'a' (avocado)
  assert(set_cursor_idx_to_locate(true, g_cursor.idx + 1, 'a'));
  assert_string_equal(g_namelist_middle[g_cursor.idx]->d_name, "avocado");

  // Forward from beyond last wraps to first 'a'
  assert(set_cursor_idx_to_locate(true, g_cursor.idx + 1, 'a'));
  assert_string_equal(g_namelist_middle[g_cursor.idx]->d_name, "apple");

  // Forward find 'c'
  assert(set_cursor_idx_to_locate(true, 0, 'c'));
  assert_string_equal(g_namelist_middle[g_cursor.idx]->d_name, "cherry");

  // No match returns false
  assert(!set_cursor_idx_to_locate(true, 0, 'z'));

  g_update.dir_middle = true;
  reload_dir_middle();
  test_cleanup_files(c);
}

static void test_locate_backward(void) {
  const char *c = __func__;

  test_create_dir(c, "files");
  test_create_file(c, "files/apple");
  test_create_file(c, "files/avocado");
  test_create_file(c, "files/banana");
  test_create_file(c, "files/blueberry");
  test_create_file(c, "files/cherry");

  snprintf(g_cwd, sizeof g_cwd, "/tmp/%s/files", c);
  g_cursor.name[0] = '\0';
  g_update.dir_middle = true;
  reload_dir_middle();

  // Backward from end finds first 'a' going backward (avocado at idx 1)
  const int last = g_items_in_middle_dir - 1;
  assert(set_cursor_idx_to_locate(false, last, 'a'));
  assert(g_cursor.idx == 1);
  assert_string_equal(g_namelist_middle[g_cursor.idx]->d_name, "avocado");

  // Backward from 0 with 'a' finds apple at 0 (before wrapping)
  assert(set_cursor_idx_to_locate(false, g_cursor.idx - 1, 'a'));
  assert(g_cursor.idx == 0);
  assert_string_equal(g_namelist_middle[g_cursor.idx]->d_name, "apple");

  // Backward from 0 with 'b' wraps to last 'b' (blueberry at idx 3)
  assert(set_cursor_idx_to_locate(false, 0, 'b'));
  assert(g_cursor.idx == 3);
  assert_string_equal(g_namelist_middle[g_cursor.idx]->d_name, "blueberry");

  // No match returns false
  assert(!set_cursor_idx_to_locate(false, last, 'z'));

  g_update.dir_middle = true;
  reload_dir_middle();
  test_cleanup_files(c);
}

Test nav_tests[] = {
    {"/locate_forward", test_locate_forward},
    {"/locate_backward", test_locate_backward},
    {NULL, NULL},
};
