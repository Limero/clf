#pragma once

#include "color.c"
#include "command.c"
#include "global.h"
#include "preview.c"
#include "status.c"
#include <assert.h>
#include <dirent.h>

static int compare_dirs_first(const struct dirent **a, const struct dirent **b) {
  if ((*a)->d_type == DT_DIR && (*b)->d_type != DT_DIR) {
    return -1;
  }
  if ((*b)->d_type == DT_DIR && (*a)->d_type != DT_DIR) {
    return 1;
  }

  return strcasecmp((*a)->d_name, (*b)->d_name);
}

static int filter_dir(const struct dirent *d) {
  return (strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0) && (g_show_hidden || d->d_name[0] != '.');
}

static void reload_dir_middle(void) {
  if (!g_update.dir_middle) {
    return;
  }

  if (g_namelist_middle != NULL) {
    for (int i = 0; i < g_items_in_middle_dir; i++) {
      free(g_namelist_middle[i]);
    }
    free(g_namelist_middle);
  }

  const int n = scandir(g_cwd, &g_namelist_middle, filter_dir, compare_dirs_first);
  if (n == -1) {
    g_namelist_middle = NULL;
    snprintf(g_msg, sizeof g_msg, "(middle scandir) %s", strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    g_items_in_middle_dir = 0;
    return;
  } else if (n == 0) {
    g_current_selection.name[0] = '\0';
  }

  g_items_in_middle_dir = n;
  g_update.dir_middle = false;
}

void set_current_selection_idx_to_name(const char *name) {
  reload_dir_middle();

  for (int i = 0; i < g_items_in_middle_dir; i++) {
    if (strcmp(g_namelist_middle[i]->d_name, name) == 0) {
      g_current_selection.idx = i;
      return;
    }
  }
}

void set_current_selection_idx_to_search(const bool forward, const int start_idx) {
  if (forward) {
    for (int i = start_idx; i < g_items_in_middle_dir; i++) {
      if (OPT_IGNORE_CASE && strcasestr(g_namelist_middle[i]->d_name, g_current_command.chars) != NULL) {
        g_current_selection.idx = i;
        return;
      }
      if (!OPT_IGNORE_CASE && strstr(g_namelist_middle[i]->d_name, g_current_command.chars) != NULL) {
        g_current_selection.idx = i;
        return;
      }
    }
  }

  for (int i = start_idx; i >= 0; i--) {
    if (OPT_IGNORE_CASE && strcasestr(g_namelist_middle[i]->d_name, g_current_command.chars) != NULL) {
      g_current_selection.idx = i;
      return;
    }
    if (!OPT_IGNORE_CASE && strstr(g_namelist_middle[i]->d_name, g_current_command.chars) != NULL) {
      g_current_selection.idx = i;
      return;
    }
  }
}

static int int_digits(const int n) {
  if (n < 10)
    return 1;
  if (n < 100)
    return 2;
  if (n < 1000)
    return 3;
  assert(n < 100000);
  return 4;
}

static void draw_left_column(const int offset_x, const int width) {
  if (strcmp(g_cwd, "/") == 0) {
    return;
  }

  if (g_update.dir_left) {
    if (g_namelist_left != NULL) {
      for (int i = 0; i < g_items_in_left_dir; i++) {
        free(g_namelist_left[i]);
      }
      free(g_namelist_left);
    }

    const int n = scandir("../", &g_namelist_left, filter_dir, compare_dirs_first);
    if (n == -1) {
      g_namelist_left = NULL;
      snprintf(g_msg, sizeof g_msg, "(%s scandir) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
      g_items_in_left_dir = 0;
      return;
    }

    const char *current_dir = strrchr(g_cwd, '/') + 1;
    for (int i = 0; i < n; i++) {
      if (strcmp(g_namelist_left[i]->d_name, current_dir) == 0) {
        g_left_column_idx = i;
        break;
      }
    }

    g_items_in_left_dir = n;
    g_update.dir_left = false;
  }

  if (g_items_in_left_dir == 0) {
    return;
  }

  int start_idx = 0;
  if (g_left_column_idx >= tb_height() - 2) {
    start_idx = g_left_column_idx - (tb_height() - 3);
    assert(start_idx >= 0);
  }

  const int stop_idx = MIN(start_idx + tb_height() - 2, g_items_in_left_dir);

  int y = 1;
  for (int i = start_idx; i < stop_idx; i++) {
    const uintattr_t fg = color_file(g_namelist_left[i]->d_type);
    const uintattr_t bg = i == g_left_column_idx ? COLOR_REVERSE : COLOR_DEFAULT;
    tb_printf(offset_x + 1, y, fg, bg, " %-*.*s", width - 1, width - 1, g_namelist_left[i]->d_name);
    y++;
  }
}

static void draw_middle_column(const int offset_x, const int width) {
  reload_dir_middle();

  int y = 1;
  if (g_items_in_middle_dir == 0) {
    tb_print(offset_x + 3, y, COLOR_REVERSE, COLOR_DEFAULT, "empty");
    return;
  }

  static int start_idx = 0;
  const int view_height = MAX(tb_height() - 2, 1);

  const int max_start = MAX(0, g_items_in_middle_dir - view_height);
  if (start_idx > max_start) {
    start_idx = max_start;
  }

  if (g_current_selection.idx < start_idx) {
    start_idx = g_current_selection.idx;
  } else if (g_current_selection.idx >= start_idx + view_height) {
    start_idx = g_current_selection.idx - (view_height - 1);
    if (start_idx < 0) {
      start_idx = 0;
    } else if (start_idx > max_start) {
      start_idx = max_start;
    }
  }

  const int stop_idx = MIN(start_idx + view_height, g_items_in_middle_dir);

  for (int i = start_idx; i < stop_idx; i++) {
    if (i == g_current_selection.idx) {
      strlcpy(g_current_selection.name, g_namelist_middle[i]->d_name, sizeof(g_current_selection.name));
    }
    if (OPT_NUMBER) {
      tb_printf(offset_x, y, COLOR_LINENUMBER, COLOR_DEFAULT, "%d",
                (g_current_selection.idx == i || !OPT_RELATIVE_NUMBER) ? i + 1 : abs(g_current_selection.idx - i));
    }

    const uintattr_t fg = color_file(g_namelist_middle[i]->d_type);
    const uintattr_t bg = i == g_current_selection.idx ? COLOR_REVERSE : COLOR_DEFAULT;
    tb_printf(offset_x + 2, y, fg, bg, " %-*.*s", width - 4, width - 4, g_namelist_middle[i]->d_name);

    y++;
  }
}

static void draw_right_column(const int offset_x, const int width) {
  int y = 1;

  if (g_update.dir_right) {
    if (g_namelist_right != NULL) {
      for (int i = 0; i < g_items_in_right_dir; i++) {
        free(g_namelist_right[i]);
      }
      free(g_namelist_right);
    }

    const int n = scandir(g_current_selection.name, &g_namelist_right, filter_dir, compare_dirs_first);
    g_update.dir_right = false;
    if (n == -1) {
      g_namelist_right = NULL;
      g_items_in_right_dir = 0;
      switch (errno) {
      case ENOTDIR:
        preview_start(offset_x, g_current_selection.name);
        return;
      case ENOENT:
        return;
      default:
        tb_print(offset_x + 2, y, COLOR_REVERSE, COLOR_DEFAULT, strerror(errno));
        return;
      }
    }

    g_items_in_right_dir = n;
  }

  if (g_items_in_right_dir == 0) {
    tb_print(offset_x + 2, y, COLOR_REVERSE, COLOR_DEFAULT, "empty");
    return;
  }

  int start_idx = 0;
  if (g_right_column_idx >= tb_height() - 2) {
    start_idx = g_right_column_idx - (tb_height() - 3);
    assert(start_idx >= 0);
  }

  const int stop_idx = MIN(start_idx + tb_height() - 2, g_items_in_right_dir);

  for (int i = start_idx; i < stop_idx; i++) {
    uintattr_t c = color_file(g_namelist_right[i]->d_type);
    if (i == g_right_column_idx) {
      c |= COLOR_UNDERLINE;
    }
    tb_printf(offset_x + 1, y, c, COLOR_DEFAULT, " %-*.*s", width - 1, width - 1, g_namelist_right[i]->d_name);

    y++;
  }
}

static void draw_path(void) {
  tb_printf(0, 0, COLOR_PATH_USER_HOST, COLOR_DEFAULT, "%s@%s", g_username, g_hostname);
  const int offset = g_username_len + g_hostname_len + 2;
  tb_print(offset, 0, COLOR_DEFAULT, COLOR_DEFAULT, ":");

  if (strcmp(g_cwd, "/") == 0) {
    tb_print(offset, 0, COLOR_PATH_DIR, COLOR_DEFAULT, "/");
    tb_print(offset + 1, 0, COLOR_PATH_SELECTION, COLOR_DEFAULT, g_current_selection.name);
  } else {
    if (strncmp(g_homepath, g_cwd, g_homepath_len) == 0) {
      tb_printf(offset, 0, COLOR_PATH_DIR, COLOR_DEFAULT, "~%s/", g_cwd + g_homepath_len);
      tb_print(offset + strlen(g_cwd) - g_homepath_len + 2, 0, COLOR_PATH_SELECTION, COLOR_DEFAULT,
               g_current_selection.name);
    } else {
      tb_printf(offset, 0, COLOR_PATH_DIR, COLOR_DEFAULT, "%s/", g_cwd);
      tb_print(offset + strlen(g_cwd) + 1, 0, COLOR_PATH_SELECTION, COLOR_DEFAULT, g_current_selection.name);
    }
  }
}

static void clear_line(const int y) {
  for (int i = 0; i <= tb_width(); i++) {
    tb_set_cell(i, y, ' ', 0, 0);
  }
}

static void draw_message(void) {
  clear_line(tb_height() - 1);
  switch (g_msg_type) {
  case MSG_TYPE_INFO:
    tb_print(0, tb_height() - 1, COLOR_DEFAULT, COLOR_DEFAULT, g_msg);
    break;
  case MSG_TYPE_SUCCESS:
    tb_print(0, tb_height() - 1, COLOR_SUCCESS_FG, COLOR_DEFAULT, g_msg);
    break;
  case MSG_TYPE_ERROR:
    tb_printf(0, tb_height() - 1, COLOR_DEFAULT, COLOR_ERROR_BG, "Error: %s", g_msg);
    break;
  }
  g_msg[0] = '\0';
}

static void draw_status_normal(const int repeat) {
  clear_line(tb_height() - 1);
  if (!g_items_in_middle_dir) {
    return;
  }

  char path[sizeof(g_cwd) + 1 /* slash */ + sizeof(g_current_selection.name)];
  ssize_t needed = snprintf(path, sizeof(path), "%s/%s", g_cwd, g_current_selection.name);
  if (needed < 0 || (size_t)needed >= sizeof(path)) {
    snprintf(g_msg, sizeof g_msg, "Path too long (needed %zd bytes)\n", needed + 1);
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  struct stat st;
  if (lstat(path, &st) < 0) {
    snprintf(g_msg, sizeof g_msg, "(%s lstat) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  char *status_str = status_normal_string(&st);
  tb_printf(0, tb_height() - 1, COLOR_STATUS_PERM, COLOR_DEFAULT, "%.10s", status_str);
  tb_print(11, tb_height() - 1, COLOR_DEFAULT, COLOR_DEFAULT, status_str + 11);

  const int end_offset = tb_width() - int_digits(g_current_selection.idx + 1) - int_digits(g_items_in_middle_dir) - 1;
  tb_printf(end_offset, tb_height() - 1, COLOR_DEFAULT, COLOR_DEFAULT, "%d/%d", g_current_selection.idx + 1,
            g_items_in_middle_dir);

  if (repeat) {
    tb_printf(end_offset - int_digits(repeat) - 1, tb_height() - 1, COLOR_DEFAULT, COLOR_DEFAULT, "%d", repeat);
  }
}

static void draw_status_command(void) {
  const int y = tb_height() - 1;
  clear_line(y);
  tb_print(0, y, COLOR_DEFAULT, COLOR_DEFAULT, g_search_idx_before >= 0 ? "/" : ":");
  tb_print(1, y, COLOR_DEFAULT, COLOR_DEFAULT, g_current_command.chars);
  tb_set_cursor(g_current_command.cursor + 1,
                y); // offset by one because we prefix with :
}

void draw_status_running_command(void) {
  const int y = tb_height() - 1;
  clear_line(y);
  tb_print(0, y, COLOR_DEFAULT, COLOR_DEFAULT, ">");
  tb_set_cursor(1, y);
  tb_present();
}

static int draw_confirmation(const char *msg1, const char *msg2) {
  clear_line(tb_height() - 1);
  tb_printf(0, tb_height() - 1, COLOR_DEFAULT, COLOR_DEFAULT, "%s%s? [y/n]", msg1, msg2);
  tb_present();

  struct tb_event ev;
  tb_poll_event(&ev);
  return ev.ch == 'y';
}

void draw_screen(const int repeat) {
  const int column_width = tb_width() / 6;
  draw_left_column(column_width * 0, column_width * 1 - 2);
  draw_middle_column(column_width * 1 + 2, column_width * 2 - 2);
  draw_right_column(column_width * 3, column_width * 3);

  draw_path();

  if (g_msg[0] != '\0') {
    draw_message();
  } else {
    switch (g_current_mode) {
    case MODE_COMMAND:
      draw_status_command();
      break;
    case MODE_NORMAL:
      draw_status_normal(repeat);
      break;
    }
  }

  pthread_mutex_lock(&g_tb_mutex);
  tb_present();
  pthread_mutex_unlock(&g_tb_mutex);
}
