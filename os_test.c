#include "os.c"
#include "test.c"

static MunitResult test_os_exec_output(const MunitParameter params[], void *data) {
  // Info message from stdout
  {
    os_exec_output("echo 'hello'", "world");
    assert_string_equal("hello world\n", g_msg);
    assert(g_msg_type == MSG_TYPE_INFO);
  }

  // Error message from invalid command
  {
    os_exec_output("echozzz", "");
    test_assert_string_contains(g_msg, "command not found");
    assert(g_msg_type == MSG_TYPE_ERROR);
  }

  // Error message from stderr
  {
    return MUNIT_SKIP; // TODO
    os_exec_output("echo 'hello' >&2", "");
    // assert_string_equal("hello\n", g_msg);
    // assert(g_msg_type == MSG_TYPE_ERROR);
  }

  // Working directory is updated if cd in command
  {
    assert_string_equal(".", g_cwd);
    os_exec_output("cd /tmp", "");
    test_assert_string_contains(g_cwd, "/tmp");

    char cwd[500];
    assert(getcwd(cwd, sizeof(cwd)));
    test_assert_string_contains(cwd, "/tmp");
  }

  return MUNIT_OK;
}

MunitTest os_tests[] = {
    {"/exec_output", test_os_exec_output, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
