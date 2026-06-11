#include "os.c"
#include "test.c"

static void test_os_exec_output(void) {
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

  // Error message from stderr (command fails with non-zero exit)
  {
    os_exec_output("sh -c 'echo error >&2; exit 1'", "");
    test_assert_string_contains(g_msg, "error");
    assert(g_msg_type == MSG_TYPE_ERROR);
  }

  // Working directory is updated if cd in command
  {
    char initial_cwd[PATH_MAX];
    assert(getcwd(initial_cwd, sizeof(initial_cwd)));
    assert_string_equal(initial_cwd, g_cwd);

    os_exec_output("cd /tmp", "");
    test_assert_string_contains(g_cwd, "/tmp");

    char cwd[500];
    assert(getcwd(cwd, sizeof(cwd)));
    test_assert_string_contains(cwd, "/tmp");
  }
}

Test os_tests[] = {
    {"/exec_output", test_os_exec_output},

    {NULL, NULL},
};
