#include <dirent.h>

#include "complete_test.c"
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

void test_all_testfiles_included(void);

Test main_tests[] = {
    {"/all_testfiles_included", test_all_testfiles_included},
    {NULL, NULL},
};

Suite test_suites[] = {
    {"main", main_tests}, {"complete", complete_tests}, {"copy", copy_tests}, {"os", os_tests}, {NULL, NULL},
};

void test_all_testfiles_included(void) {
  assert_int(count_test_files("."), ==, sizeof(test_suites) / sizeof(test_suites[0]) - 2);
}

int main(void) {
  return run_all(test_suites) > 0;
}
