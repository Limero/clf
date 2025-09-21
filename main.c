#include "nav.c"

static int main_exit(void) {
  tb_shutdown();
  free(g_username);
  free(g_hostname);
  free(g_homepath);

  if (g_namelist_left != NULL) {
    for (int i = 0; i < g_items_in_left_dir; i++) {
      free(g_namelist_left[i]);
    }
    free(g_namelist_left);
  }

  if (g_namelist_middle != NULL) {
    for (int i = 0; i < g_items_in_middle_dir; i++) {
      free(g_namelist_middle[i]);
    }
    free(g_namelist_middle);
  }

  if (g_namelist_right != NULL) {
    for (int i = 0; i < g_items_in_right_dir; i++) {
      free(g_namelist_right[i]);
    }
    free(g_namelist_right);
  }

  return 0;
}

int main(int argc, char **argv) {
  if (argc == 2 && chdir(argv[1]) == -1) {
    switch (errno) {
    case ENOTDIR: {
      char *p = argv[1] + strlen(argv[1]) - 1;
      while (*p != '/')
        --p;
      *p = '\0';

      if (chdir(argv[1]) == -1) {
        if (errno == ENOTDIR) {
          set_current_selection_idx_to_name(argv[1]);
        } else {
          fprintf(stderr, "chdir to %s failed: %s\n", argv[1], strerror(errno));
          return 1;
        }
      } else {
        set_current_selection_idx_to_name(p + 1);
      }
      break;
    }
    default:
      fprintf(stderr, "chdir to %s failed: %s\n", argv[1], strerror(errno));
      return 1;
    }
  }

  if (!getcwd(g_cwd, sizeof(g_cwd))) {
    fprintf(stderr, "getcwd failed: %s\n", strerror(errno));
    return 1;
  }

  const char *username = getenv("USER");
  if (!username) {
    fprintf(stderr, "Failed to get username from environment\n");
    return 1;
  }
  g_username = strdup(username);
  assert(g_username);
  g_username_len = strlen(g_username);

  g_hostname = malloc(_POSIX_HOST_NAME_MAX + 1);
  assert(g_hostname);
  if (gethostname(g_hostname, _POSIX_HOST_NAME_MAX + 1) != 0) {
    fprintf(stderr, "Failed to get hostname from environment\n");
    free(g_username);
    free(g_hostname);
    return 1;
  }
  g_hostname_len = strlen(g_hostname);

  const char *home_path = getenv("HOME");
  if (!home_path) {
    fprintf(stderr, "Failed to get home path from environment\n");
    free(g_username);
    free(g_hostname);
    return 1;
  }
  g_homepath = strdup(home_path);
  assert(g_homepath);
  g_homepath_len = strlen(g_homepath);

  int repeat = 0;
  struct tb_event ev;

  tb_init();

  for (;;) {
    draw_screen(repeat);
    tb_poll_event(&ev);

    switch (nav_handle_event(&ev, &repeat)) {
    case -1:
      return main_exit();
    case 0:
      repeat = 0;
    }
  }

  return 0;
}
