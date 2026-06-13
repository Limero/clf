#include "complete.c"
#include "test.c"

static void test_word_extraction(void) {
  char word[256];
  bool is_first;

  // Single word, cursor at end
  strcpy(g_current_command.chars, "echo");
  g_current_command.len = 4;
  g_current_command.cursor = 4;
  complete_word_at_cursor(g_current_command.chars, g_current_command.cursor, word, sizeof word, &is_first);
  assert_string_equal(word, "echo");
  assert(is_first);

  // Single word, cursor in middle
  g_current_command.cursor = 2;
  complete_word_at_cursor(g_current_command.chars, g_current_command.cursor, word, sizeof word, &is_first);
  assert_string_equal(word, "echo");
  assert(is_first);

  // Two words, cursor at end of second word
  strcpy(g_current_command.chars, "cd /tmp");
  g_current_command.len = 7;
  g_current_command.cursor = 7;
  complete_word_at_cursor(g_current_command.chars, g_current_command.cursor, word, sizeof word, &is_first);
  assert_string_equal(word, "/tmp");
  assert(!is_first);

  // Two words, cursor on first word
  g_current_command.cursor = 1;
  complete_word_at_cursor(g_current_command.chars, g_current_command.cursor, word, sizeof word, &is_first);
  assert_string_equal(word, "cd");
  assert(is_first);

  // Two words, cursor on start of second word
  g_current_command.cursor = 3;
  complete_word_at_cursor(g_current_command.chars, g_current_command.cursor, word, sizeof word, &is_first);
  assert_string_equal(word, "/tmp");
  assert(!is_first);
}

static void test_command_matching(void) {
  g_cache_cmd_count = 0;
  strcpy(g_cache_cmds[g_cache_cmd_count++], "cat");
  strcpy(g_cache_cmds[g_cache_cmd_count++], "cd");
  strcpy(g_cache_cmds[g_cache_cmd_count++], "chmod");
  strcpy(g_cache_cmds[g_cache_cmd_count++], "cp");
  strcpy(g_cache_cmds[g_cache_cmd_count++], "echo");
  strcpy(g_cache_cmds[g_cache_cmd_count++], "ls");
  strcpy(g_cache_cmds[g_cache_cmd_count++], "mkdir");

  g_match_count = 0;
  complete_generate_commands("c");
  assert_int(g_match_count, ==, 4);
  bool found[4] = {false, false, false, false};
  for (int i = 0; i < g_match_count; i++) {
    if (strcmp(g_matches[i], "cat") == 0)
      found[0] = true;
    if (strcmp(g_matches[i], "cd") == 0)
      found[1] = true;
    if (strcmp(g_matches[i], "chmod") == 0)
      found[2] = true;
    if (strcmp(g_matches[i], "cp") == 0)
      found[3] = true;
  }
  for (int i = 0; i < 4; i++)
    assert(found[i]);

  g_match_count = 0;
  complete_generate_commands("ec");
  assert_int(g_match_count, ==, 1);
  assert_string_equal(g_matches[0], "echo");

  g_match_count = 0;
  complete_generate_commands("xyz");
  assert_int(g_match_count, ==, 0);
}

static void test_path_matching(void) {
  const char *c = __func__;
  char *dir = test_create_dir(c, "sub");
  test_create_file(c, "sub/file1.txt");
  test_create_file(c, "sub/file2.txt");
  test_create_file(c, "sub/other.txt");

  char prev[PATH_MAX];
  assert(getcwd(prev, sizeof prev));
  assert_int(chdir(dir), ==, 0);

  // Match from current directory
  g_match_count = 0;
  complete_generate_paths("file", false);
  assert_int(g_match_count, >=, 2);
  bool f1 = false, f2 = false;
  for (int i = 0; i < g_match_count; i++) {
    if (strcmp(g_matches[i], "file1.txt") == 0)
      f1 = true;
    if (strcmp(g_matches[i], "file2.txt") == 0)
      f2 = true;
  }
  assert(f1);
  assert(f2);

  // Empty prefix lists all
  g_match_count = 0;
  complete_generate_paths("", false);
  assert_int(g_match_count, >=, 3);

  // No match
  g_match_count = 0;
  complete_generate_paths("zzz", false);
  assert_int(g_match_count, ==, 0);

  // Dot prefix shows . and ..
  g_match_count = 0;
  complete_generate_paths(".", false);
  assert_int(g_match_count, >=, 2);

  assert_int(chdir(prev), ==, 0);
  test_cleanup_files(c);
}

static void test_apply_replaces_word(void) {
  // First word: "ec" -> "echo"
  strcpy(g_current_command.chars, "ec");
  g_current_command.len = 2;
  g_current_command.cursor = 2;
  complete_apply("echo");
  assert_string_equal(g_current_command.chars, "echo");
  assert_int(g_current_command.cursor, ==, 4);
  assert_int(g_current_command.len, ==, 4);

  // Second word: "cd /tm" -> "cd /tmp"
  strcpy(g_current_command.chars, "cd /tm");
  g_current_command.len = 6;
  g_current_command.cursor = 6;
  complete_apply("/tmp");
  assert_string_equal(g_current_command.chars, "cd /tmp");
  assert_int(g_current_command.cursor, ==, 7);
  assert_int(g_current_command.len, ==, 7);

  // Middle word: "mv src dest" -> "mv source dest" (cursor at end of "src")
  strcpy(g_current_command.chars, "mv src dest");
  g_current_command.len = 11;
  g_current_command.cursor = 6;
  complete_apply("source");
  assert_string_equal(g_current_command.chars, "mv source dest");
  assert_int(g_current_command.cursor, ==, 9);
  assert_int(g_current_command.len, ==, 14);

  // Shorter word: "rm longfile" -> "rm f"
  strcpy(g_current_command.chars, "rm longfile");
  g_current_command.len = 11;
  g_current_command.cursor = 11;
  complete_apply("f");
  assert_string_equal(g_current_command.chars, "rm f");
  assert_int(g_current_command.cursor, ==, 4);
  assert_int(g_current_command.len, ==, 4);
}

static void test_complete_cycle(void) {
  g_cache_cmd_count = 0;
  strcpy(g_cache_cmds[g_cache_cmd_count++], "cat");
  strcpy(g_cache_cmds[g_cache_cmd_count++], "chmod");
  strcpy(g_cache_cmds[g_cache_cmd_count++], "cp");
  g_cache_ready = true;

  strcpy(g_current_command.chars, "c");
  g_current_command.len = 1;
  g_current_command.cursor = 1;

  // Cycle order: cat (0), chmod (1), cp (2), cat (0), ...
  assert(complete_handle_tab());
  assert_string_equal(g_current_command.chars, "cat");

  assert(complete_handle_tab());
  assert_string_equal(g_current_command.chars, "chmod");

  assert(complete_handle_tab());
  assert_string_equal(g_current_command.chars, "cp");

  assert(complete_handle_tab());
  assert_string_equal(g_current_command.chars, "cat");

  g_cache_ready = false;
}

static void test_complete_apply_shorter(void) {
  // Tests that cursor beyond original NUL is handled
  strcpy(g_current_command.chars, "c");
  g_current_command.len = 1;
  g_current_command.cursor = 1;
  complete_apply("cat");
  assert_string_equal(g_current_command.chars, "cat");
}

static void test_complete_cycle_with_spaces(void) {
  const char *c = __func__;
  g_cache_cmd_count = 0;
  strcpy(g_cache_cmds[g_cache_cmd_count++], "cd");
  g_cache_ready = true;

  const char *dir = test_create_dir(c, "");
  char dpath[500];
  snprintf(dpath, sizeof dpath, "%s/a_dir", dir);
  mkdir(dpath, 0700);
  snprintf(dpath, sizeof dpath, "%s/b dir", dir);
  mkdir(dpath, 0700);
  snprintf(dpath, sizeof dpath, "%s/c_dir", dir);
  mkdir(dpath, 0700);

  char prev[PATH_MAX];
  assert(getcwd(prev, sizeof prev));
  assert_int(chdir(dir), ==, 0);

  // "cd " to trigger path completion
  strcpy(g_current_command.chars, "cd ");
  g_current_command.len = 3;
  g_current_command.cursor = 3;

  // First tab finds all matches, applies first (a_dir)
  assert(complete_handle_tab());
  assert_string_equal(g_current_command.chars, "cd a_dir");

  // Second tab cycles to b dir (escaped)
  assert(complete_handle_tab());
  assert_string_equal(g_current_command.chars, "cd b\\ dir");

  // Third tab cycles to c_dir
  assert(complete_handle_tab());
  assert_string_equal(g_current_command.chars, "cd c_dir");

  // Fourth tab wraps back to a_dir
  assert(complete_handle_tab());
  assert_string_equal(g_current_command.chars, "cd a_dir");

  g_cache_ready = false;
  assert_int(chdir(prev), ==, 0);
  test_cleanup_files(c);
}

static void test_path_matching_with_spaces(void) {
  const char *c = __func__;
  const char *dir = test_create_dir(c, "");
  test_create_file(c, "my file.txt");
  test_create_file(c, "file.txt");
  test_create_file(c, "file\\2.txt");

  char prev[PATH_MAX];
  assert(getcwd(prev, sizeof prev));
  assert_int(chdir(dir), ==, 0);

  g_match_count = 0;
  complete_generate_paths("", false);
  bool found_escaped_space = false;
  bool found_escaped_backslash = false;
  bool found_plain = false;
  for (int i = 0; i < g_match_count; i++) {
    if (strcmp(g_matches[i], "my\\ file.txt") == 0)
      found_escaped_space = true;
    if (strcmp(g_matches[i], "file\\\\2.txt") == 0)
      found_escaped_backslash = true;
    if (strcmp(g_matches[i], "file.txt") == 0)
      found_plain = true;
  }
  assert(found_escaped_space);
  assert(found_escaped_backslash);
  assert(found_plain);

  assert_int(chdir(prev), ==, 0);
  test_cleanup_files(c);
}

static void test_path_matching_dirs_only(void) {
  const char *c = __func__;
  char *dir = test_create_dir(c, "");
  test_create_file(c, "file1.txt");
  test_create_file(c, "file2.txt");
  char dpath[500];
  snprintf(dpath, sizeof dpath, "%s/sub1", dir);
  mkdir(dpath, 0700);
  snprintf(dpath, sizeof dpath, "%s/sub2", dir);
  mkdir(dpath, 0700);

  char prev[PATH_MAX];
  assert(getcwd(prev, sizeof prev));
  assert_int(chdir(dir), ==, 0);

  // With dirs_only=true, only directories should appear
  g_match_count = 0;
  complete_generate_paths("", true);
  assert_int(g_match_count, ==, 2);
  bool found_sub1 = false, found_sub2 = false;
  for (int i = 0; i < g_match_count; i++) {
    if (strcmp(g_matches[i], "sub1") == 0)
      found_sub1 = true;
    if (strcmp(g_matches[i], "sub2") == 0)
      found_sub2 = true;
  }
  assert(found_sub1);
  assert(found_sub2);

  // With dirs_only=false, both files and dirs appear
  g_match_count = 0;
  complete_generate_paths("", false);
  assert_int(g_match_count, >=, 4);

  assert_int(chdir(prev), ==, 0);
  test_cleanup_files(c);
}

static void test_complete_resets_on_type(void) {
  g_cache_cmd_count = 0;
  strcpy(g_cache_cmds[g_cache_cmd_count++], "cat");
  strcpy(g_cache_cmds[g_cache_cmd_count++], "cd");
  g_cache_ready = true;

  strcpy(g_current_command.chars, "c");
  g_current_command.len = 1;
  g_current_command.cursor = 1;

  assert(complete_handle_tab());
  assert_string_equal(g_current_command.chars, "cat");

  complete_reset();
  assert(!complete_is_active());
  assert_int(complete_match_count(), ==, 0);

  // Change prefix to "cd"
  strcpy(g_current_command.chars, "cd");
  g_current_command.len = 2;
  g_current_command.cursor = 2;

  assert(complete_handle_tab());
  assert_string_equal(g_current_command.chars, "cd");

  g_cache_ready = false;
}

Test complete_tests[] = {
    {"/word_extraction", test_word_extraction},
    {"/command_matching", test_command_matching},
    {"/path_matching", test_path_matching},
    {"/apply_replaces_word", test_apply_replaces_word},
    {"/complete_cycle", test_complete_cycle},
    {"/complete_apply_shorter", test_complete_apply_shorter},
    {"/complete_resets_on_type", test_complete_resets_on_type},
    {"/path_matching_with_spaces", test_path_matching_with_spaces},
    {"/path_matching_dirs_only", test_path_matching_dirs_only},
    {"/complete_cycle_with_spaces", test_complete_cycle_with_spaces},

    {NULL, NULL},
};
