#include <dirent.h>

#include "copy_test.c"
#include "os_test.c"

static int count_test_files(char *base_path) {
  int count = 0;

  char path[1000];
  struct dirent *dp;
  DIR *dir = opendir(base_path);

  if (!dir)
    return 0;

  while ((dp = readdir(dir)) != NULL) {
    if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
      continue;
    }

    size_t len = strlen(dp->d_name);
    if (len > 6 && strcmp(dp->d_name + (len - 7), "_test.c") == 0) {
      count++;
    }

    strcpy(path, base_path);
    strcat(path, "/");
    strcat(path, dp->d_name);
    count += count_test_files(path);
  }

  closedir(dir);
  return count;
}

MunitResult test_all_testfiles_included(const MunitParameter params[], void *data);

MunitTest main_tests[] = {
    {"/all_testfiles_included", test_all_testfiles_included, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_suites[] = {
    {"main", main_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"copy", copy_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"os", os_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},

    {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE},
};

MunitResult test_all_testfiles_included(const MunitParameter params[], void *data) {
  assert_int(count_test_files("."), ==, sizeof(test_suites) / sizeof(test_suites[0]) - 2);

  return MUNIT_OK;
}

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
  return munit_suite_main(&(MunitSuite){"", NULL, test_suites, 1, MUNIT_SUITE_OPTION_NONE}, NULL, argc, argv);
}
