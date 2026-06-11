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

static void test_shell_quote(void) {
  char buf[4096];

  shell_quote("", buf, sizeof buf);
  assert_string_equal("''", buf);

  shell_quote("file.txt", buf, sizeof buf);
  assert_string_equal("'file.txt'", buf);

  shell_quote("my file.txt", buf, sizeof buf);
  assert_string_equal("'my file.txt'", buf);

  shell_quote("it's.txt", buf, sizeof buf);
  assert_string_equal("'it'\\''s.txt'", buf);

  shell_quote("$HOME/file", buf, sizeof buf);
  assert_string_equal("'$HOME/file'", buf);

  shell_quote("`backtick`", buf, sizeof buf);
  assert_string_equal("'`backtick`'", buf);
}

static void test_os_exec_spaces(void) {
  g_msg[0] = '\0';

  strcpy(g_cursor.name, "my file.txt");
  os_exec("true", g_cursor.name);
  assert(g_msg[0] == '\0');

  strcpy(g_cursor.name, "it's.txt");
  os_exec("true", g_cursor.name);
  assert(g_msg[0] == '\0');
}

static void test_os_exec_output_spaces(void) {
  // $f is exported to child process, so use sh -c to read it
  g_msg[0] = '\0';
  strcpy(g_cursor.name, "my file.txt");
  os_exec_output("sh -c 'echo \"$f\"'", "");
  test_assert_string_contains(g_msg, "my file.txt");
  assert(g_msg_type == MSG_TYPE_INFO);

  g_msg[0] = '\0';
  strcpy(g_cursor.name, "it's.txt");
  os_exec_output("sh -c 'echo \"$f\"'", "");
  test_assert_string_contains(g_msg, "it's.txt");
  assert(g_msg_type == MSG_TYPE_INFO);
}

Test os_tests[] = {
    {"/exec_output", test_os_exec_output},
    {"/shell_quote", test_shell_quote},
    {"/exec_spaces", test_os_exec_spaces},
    {"/exec_output_spaces", test_os_exec_output_spaces},

    {NULL, NULL},
};
