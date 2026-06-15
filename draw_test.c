#include "draw.c"
#include "test.c"

static void test_count_msg_lines(void) {
  // Empty string
  g_msg[0] = '\0';
  assert_int(count_msg_lines(), ==, 0);

  // Single line, no trailing newline
  strcpy(g_msg, "hello");
  assert_int(count_msg_lines(), ==, 1);

  // Single line, trailing newline
  strcpy(g_msg, "hello\n");
  assert_int(count_msg_lines(), ==, 1);

  // Two lines, no trailing newline
  strcpy(g_msg, "hello\nworld");
  assert_int(count_msg_lines(), ==, 2);

  // Two lines, trailing newline
  strcpy(g_msg, "hello\nworld\n");
  assert_int(count_msg_lines(), ==, 2);

  // Three lines
  strcpy(g_msg, "a\nb\nc");
  assert_int(count_msg_lines(), ==, 3);

  // Three lines, trailing newline
  strcpy(g_msg, "a\nb\nc\n");
  assert_int(count_msg_lines(), ==, 3);

  // Internal empty lines, trailing newlines stripped
  strcpy(g_msg, "hello\n\nworld\n\n");
  assert_int(count_msg_lines(), ==, 3);

  // Only newlines (all empty lines stripped)
  strcpy(g_msg, "\n\n\n");
  assert_int(count_msg_lines(), ==, 0);

  // Single newline
  strcpy(g_msg, "\n");
  assert_int(count_msg_lines(), ==, 0);
}

Test draw_tests[] = {
    {"/count_msg_lines", test_count_msg_lines},
    {NULL, NULL},
};
