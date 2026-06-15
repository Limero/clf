#pragma once

#include "color.c"
#include "complete.c"
#include "preview.c"
#include "status.c"
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

static int g_msg_clear_top = -1;

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
    g_cursor.name[0] = '\0';
  }

  g_items_in_middle_dir = n;
  g_update.dir_middle = false;

  // When items are added/removed with commands the cursor may land on the wrong item
  if (g_cursor.idx >= n) {
    g_cursor.idx = MAX(0, g_items_in_middle_dir - 1);
  } else if (g_cursor.name[0] != '\0' && strcmp(g_namelist_middle[g_cursor.idx]->d_name, g_cursor.name) != 0) {
    for (int i = 0; i < n; i++) {
      if (strcmp(g_namelist_middle[i]->d_name, g_cursor.name) == 0) {
        g_cursor.idx = i;
        break;
      }
    }
  }
}

void set_cursor_idx_to_name(const char *name) {
  reload_dir_middle();

  for (int i = 0; i < g_items_in_middle_dir; i++) {
    if (strcmp(g_namelist_middle[i]->d_name, name) == 0) {
      g_cursor.idx = i;
      return;
    }
  }
}

// Since strcasestr is nonstandard, we always define our own
char *strcasestr(const char *haystack, const char *needle) {
  if (!*needle)
    return (char *)haystack;
  for (; *haystack; haystack++) {
    const char *h = haystack;
    const char *n = needle;
    while (*n && *h && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
      h++;
      n++;
    }
    if (*n == '\0')
      return (char *)haystack;
  }
  return NULL;
}

static bool search_name_matches(const char *file_name) {
  if (OPT_IGNORE_CASE)
    return strcasestr(file_name, g_current_command.chars) != NULL;
  return strstr(file_name, g_current_command.chars) != NULL;
}

void set_cursor_idx_to_search(const bool forward, const int start_idx) {
  assert(start_idx >= -1 && start_idx <= g_items_in_middle_dir);

  if (forward) {
    for (int i = start_idx; i < g_items_in_middle_dir; i++) {
      if (search_name_matches(g_namelist_middle[i]->d_name)) {
        g_cursor.idx = i;
        return;
      }
    }
    if (OPT_WRAP_SCAN) {
      for (int i = 0; i < start_idx; i++) {
        if (search_name_matches(g_namelist_middle[i]->d_name)) {
          g_cursor.idx = i;
          return;
        }
      }
    }
  } else {
    for (int i = start_idx; i >= 0; i--) {
      if (search_name_matches(g_namelist_middle[i]->d_name)) {
        g_cursor.idx = i;
        return;
      }
    }
    if (OPT_WRAP_SCAN) {
      for (int i = g_items_in_middle_dir - 1; i > start_idx; i--) {
        if (search_name_matches(g_namelist_middle[i]->d_name)) {
          g_cursor.idx = i;
          return;
        }
      }
    }
  }
}

bool set_cursor_idx_to_locate(const bool forward, const int start_idx, const char ch) {
  assert(start_idx >= -1 && start_idx <= g_items_in_middle_dir);

  if (forward) {
    for (int i = start_idx; i < g_items_in_middle_dir; i++) {
      if (g_namelist_middle[i]->d_name[0] == ch) {
        g_cursor.idx = i;
        return true;
      }
    }
    for (int i = 0; i < start_idx; i++) {
      if (g_namelist_middle[i]->d_name[0] == ch) {
        g_cursor.idx = i;
        return true;
      }
    }
  } else {
    for (int i = start_idx; i >= 0; i--) {
      if (g_namelist_middle[i]->d_name[0] == ch) {
        g_cursor.idx = i;
        return true;
      }
    }
    for (int i = g_items_in_middle_dir - 1; i > start_idx; i--) {
      if (g_namelist_middle[i]->d_name[0] == ch) {
        g_cursor.idx = i;
        return true;
      }
    }
  }

  return false;
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

static bool is_yanked_entry(const char *base_dir, const char *entry_name) {
  char entry_path[PATH_MAX];
  const int needed = snprintf(entry_path, sizeof entry_path, "%s/%s", base_dir, entry_name);
  if (needed < 0 || needed >= (int)sizeof entry_path)
    return false;

  for (int i = 0; i < g_yanked_count; i++) {
    if (strcmp(entry_path, g_yanked_paths[i]) == 0)
      return true;
  }
  return false;
}

static uintattr_t yank_bg(void) {
  return g_yanked_is_move ? COLOR_YANK_MOVE : COLOR_YANK_COPY;
}

static int selected_index(const char *path) {
  for (int i = 0; i < g_selected_count; i++) {
    if (strcmp(g_selected_paths[i], path) == 0)
      return i;
  }
  return -1;
}

static bool is_selected_in_dir(const char *dir, const char *name) {
  if (!OPT_MULTISELECT)
    return false;
  char path[PATH_MAX];
  const int needed = snprintf(path, sizeof path, "%s/%s", dir, name);
  return needed >= 0 && needed < (int)sizeof path && selected_index(path) >= 0;
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
    if (g_left_column_idx >= g_items_in_left_dir)
      g_left_column_idx = MAX(0, g_items_in_left_dir - 1);
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
  char left_parent[PATH_MAX];
  strlcpy(left_parent, g_cwd, sizeof left_parent);
  char *slash = strrchr(left_parent, '/');
  if (slash == left_parent) {
    left_parent[1] = '\0';
  } else {
    *slash = '\0';
  }

  const bool has_yanked = g_yanked_count > 0;

  int y = 1;
  for (int i = start_idx; i < stop_idx; i++) {
    const uintattr_t fg = color_file(g_namelist_left[i]->d_type);
    uintattr_t bg = (i == g_left_column_idx) ? COLOR_REVERSE : COLOR_DEFAULT;

    const bool selected = is_selected_in_dir(left_parent, g_namelist_left[i]->d_name);
    const bool yanked = !selected && has_yanked && is_yanked_entry(left_parent, g_namelist_left[i]->d_name);
    tb_set_cell(offset_x, y, ' ', fg, selected ? COLOR_SELECT : yanked ? yank_bg() : COLOR_DEFAULT);
    tb_printf(offset_x + 1, y, fg, bg, " %-*.*s", width - 1, width - 1, g_namelist_left[i]->d_name);
    if (i == g_left_column_idx) {
      tb_set_cell(offset_x + width, y, ' ', fg, bg);
    }

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

  if (g_cursor.idx < start_idx) {
    start_idx = g_cursor.idx;
  } else if (g_cursor.idx >= start_idx + view_height) {
    start_idx = g_cursor.idx - (view_height - 1);
    if (start_idx < 0) {
      start_idx = 0;
    } else if (start_idx > max_start) {
      start_idx = max_start;
    }
  }

  const int stop_idx = MIN(start_idx + view_height, g_items_in_middle_dir);
  const int number_width = OPT_NUMBER ? int_digits(g_items_in_middle_dir) : 0;

  for (int i = start_idx; i < stop_idx; i++) {
    if (i == g_cursor.idx) {
      strlcpy(g_cursor.name, g_namelist_middle[i]->d_name, sizeof(g_cursor.name));
    }
    if (OPT_NUMBER) {
      const int num = (g_cursor.idx == i || !OPT_RELATIVE_NUMBER) ? i + 1 : abs(g_cursor.idx - i);
      const char *fmt = (g_cursor.idx == i && num < 10) ? "%-*d" : "%*d";
      tb_printf(offset_x, y, COLOR_LINENUMBER, COLOR_DEFAULT, fmt, number_width, num);
    }

    const uintattr_t fg = color_file(g_namelist_middle[i]->d_type);
    uintattr_t bg = (i == g_cursor.idx) ? COLOR_REVERSE : COLOR_DEFAULT;

    const bool selected = is_selected_in_dir(g_cwd, g_namelist_middle[i]->d_name);
    const bool yanked = !selected && is_yanked_entry(g_cwd, g_namelist_middle[i]->d_name);
    tb_set_cell(offset_x + number_width, y, ' ', fg, selected ? COLOR_SELECT : yanked ? yank_bg() : COLOR_DEFAULT);
    tb_printf(offset_x + number_width + 1, y, fg, bg, " %-*.*s", width - 2 - number_width, width - 2 - number_width,
              g_namelist_middle[i]->d_name);
    if (i == g_cursor.idx) {
      tb_set_cell(offset_x + width, y, ' ', fg, bg);
    }

    y++;
  }
}

static void draw_right_column(const int offset_x, const int width) {
  int y = 1;

  if (g_update.dir_right) {
    char prev_name[sizeof g_cursor.name] = "";
    if (g_namelist_right != NULL && g_right_column_idx < g_items_in_right_dir)
      strlcpy(prev_name, g_namelist_right[g_right_column_idx]->d_name, sizeof prev_name);

    if (g_namelist_right != NULL) {
      for (int i = 0; i < g_items_in_right_dir; i++) {
        free(g_namelist_right[i]);
      }
      free(g_namelist_right);
    }

    const int n = scandir(g_cursor.name, &g_namelist_right, filter_dir, compare_dirs_first);
    g_update.dir_right = false;
    if (n == -1) {
      g_namelist_right = NULL;
      g_items_in_right_dir = 0;
      switch (errno) {
      case ENOTDIR:
        if (g_current_mode != MODE_COMMAND)
          preview_start(offset_x, g_cursor.name);
        return;
      case ENOENT:
        return;
      default:
        tb_print(offset_x + 2, y, COLOR_REVERSE, COLOR_DEFAULT, strerror(errno));
        return;
      }
    }

    g_items_in_right_dir = n;

    // When items are added/removed with commands the cursor may land on the wrong item
    if (g_right_column_idx >= n) {
      g_right_column_idx = MAX(0, g_items_in_right_dir - 1);
    } else if (prev_name[0] != '\0' && strcmp(g_namelist_right[g_right_column_idx]->d_name, prev_name) != 0) {
      for (int i = 0; i < n; i++) {
        if (strcmp(g_namelist_right[i]->d_name, prev_name) == 0) {
          g_right_column_idx = i;
          break;
        }
      }
    }
  }

  if (g_items_in_right_dir == 0) {
    struct stat st;
    if (lstat(g_cursor.name, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        tb_print(offset_x + 2, y, COLOR_REVERSE, COLOR_DEFAULT, "empty");
      } else if (g_current_mode != MODE_COMMAND) {
        preview_start(offset_x, g_cursor.name);
      }
    }
    return;
  }

  int start_idx = 0;
  if (g_right_column_idx >= tb_height() - 2) {
    start_idx = g_right_column_idx - (tb_height() - 3);
    assert(start_idx >= 0);
  }

  const int stop_idx = MIN(start_idx + tb_height() - 2, g_items_in_right_dir);

  char right_base[PATH_MAX];
  const int needed = snprintf(right_base, sizeof right_base, "%s/%s", g_cwd, g_cursor.name);
  const bool has_yanked = g_yanked_count > 0 && needed >= 0 && needed < (int)sizeof right_base;

  for (int i = start_idx; i < stop_idx; i++) {
    const uintattr_t fg = color_file(g_namelist_right[i]->d_type);
    uintattr_t sel_fg = fg;
    if (i == g_right_column_idx) {
      sel_fg |= COLOR_UNDERLINE;
    }

    const bool selected = is_selected_in_dir(right_base, g_namelist_right[i]->d_name);
    const bool yanked = !selected && has_yanked && is_yanked_entry(right_base, g_namelist_right[i]->d_name);
    tb_set_cell(offset_x, y, ' ', fg, selected ? COLOR_SELECT : yanked ? yank_bg() : COLOR_DEFAULT);
    tb_set_cell(offset_x + 1, y, ' ', sel_fg, COLOR_DEFAULT);
    tb_printf(offset_x + 2, y, sel_fg, COLOR_DEFAULT, "%-*.*s", width - 1, width - 1, g_namelist_right[i]->d_name);

    y++;
  }
}

static void draw_path(void) {
  tb_printf(0, 0, COLOR_PATH_USER_HOST, COLOR_DEFAULT, "%s@%s", g_username, g_hostname);
  const int offset = g_username_len + g_hostname_len + 2;
  tb_print(offset, 0, COLOR_DEFAULT, COLOR_DEFAULT, ":");

  if (strcmp(g_cwd, "/") == 0) {
    tb_print(offset, 0, COLOR_PATH_DIR, COLOR_DEFAULT, "/");
    tb_print(offset + 1, 0, COLOR_PATH_CURSOR, COLOR_DEFAULT, g_cursor.name);
  } else {
    if (strncmp(g_homepath, g_cwd, g_homepath_len) == 0) {
      tb_printf(offset, 0, COLOR_PATH_DIR, COLOR_DEFAULT, "~%s/", g_cwd + g_homepath_len);
      tb_print(offset + strlen(g_cwd) - g_homepath_len + 2, 0, COLOR_PATH_CURSOR, COLOR_DEFAULT, g_cursor.name);
    } else {
      tb_printf(offset, 0, COLOR_PATH_DIR, COLOR_DEFAULT, "%s/", g_cwd);
      tb_print(offset + strlen(g_cwd) + 1, 0, COLOR_PATH_CURSOR, COLOR_DEFAULT, g_cursor.name);
    }
  }
}

static void clear_line(const int y) {
  for (int i = 0; i <= tb_width(); i++) {
    tb_set_cell(i, y, ' ', 0, 0);
  }
}

static int count_msg_lines(void) {
  if (g_msg[0] == '\0')
    return 0;

  int msg_len = strlen(g_msg);
  while (msg_len > 0 && g_msg[msg_len - 1] == '\n')
    msg_len--;
  if (msg_len == 0)
    return 0;

  int count = 1;
  for (int i = 0; i < msg_len; i++) {
    if (g_msg[i] == '\n')
      count++;
  }
  return count;
}

// Renders g_msg as multiline text at the bottom of the screen,
// with the last line at bottom_y. Returns number of lines rendered (0 if empty).
static int render_multiline_msg(int bottom_y) {
  const int line_count = count_msg_lines();
  if (line_count == 0)
    return 0;

  const int display_lines = MIN(line_count, bottom_y);

  for (int i = 0; i < display_lines; i++) {
    clear_line(bottom_y - display_lines + 1 + i);
  }

  const char *start = g_msg;
  for (int i = 0; i < display_lines; i++) {
    const char *end = strchr(start, '\n');
    const int len = end ? (int)(end - start) : (int)strlen(start);
    const int y = bottom_y - display_lines + 1 + i;

    switch (g_msg_type) {
    case MSG_TYPE_INFO:
      tb_printf(0, y, COLOR_DEFAULT, COLOR_DEFAULT, "%.*s", len, start);
      break;
    case MSG_TYPE_SUCCESS:
      tb_printf(0, y, COLOR_SUCCESS_FG, COLOR_DEFAULT, "%.*s", len, start);
      break;
    case MSG_TYPE_ERROR:
      if (i == 0) {
        tb_printf(0, y, COLOR_DEFAULT, COLOR_ERROR_BG, "Error: %.*s", len, start);
      } else {
        tb_printf(0, y, COLOR_DEFAULT, COLOR_DEFAULT, "%.*s", len, start);
      }
      break;
    }

    start = end ? end + 1 : start + len;
  }

  return display_lines;
}

static void draw_message(void) {
  const int line_count = render_multiline_msg(tb_height() - 1);
  if (line_count == 0)
    return;
  g_msg_clear_top = tb_height() - line_count;
  g_msg[0] = '\0';
}

static void draw_status_normal(const int repeat) {
  clear_line(tb_height() - 1);
  if (!g_items_in_middle_dir) {
    return;
  }

  char path[sizeof(g_cwd) + 1 /* slash */ + sizeof(g_cursor.name)];
  const ssize_t needed = snprintf(path, sizeof(path), "%s/%s", g_cwd, g_cursor.name);
  if (needed < 0 || (size_t)needed >= sizeof(path)) {
    snprintf(g_msg, sizeof g_msg, "Path too long (needed %zd bytes)", needed + 1);
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  struct stat st;
  if (lstat(path, &st) < 0) {
    snprintf(g_msg, sizeof g_msg, "(%s lstat) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  const char *status_str = status_normal_string(&st);
  tb_printf(0, tb_height() - 1, COLOR_STATUS_PERM, COLOR_DEFAULT, "%.10s", status_str);
  tb_print(11, tb_height() - 1, COLOR_DEFAULT, COLOR_DEFAULT, status_str + 11);

  const int end_offset = tb_width() - int_digits(g_cursor.idx + 1) - int_digits(g_items_in_middle_dir) - 1;
  tb_printf(end_offset, tb_height() - 1, COLOR_DEFAULT, COLOR_DEFAULT, "%d/%d", g_cursor.idx + 1,
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

  if (OPT_CMD_COMPLETE && g_search_idx_before < 0 && complete_is_active()) {
    char info[64];
    const int count = complete_match_count();
    const int idx = complete_match_idx() + 1;
    if (count >= COMPLETION_MAX_MATCHES) {
      snprintf(info, sizeof info, " [%d/99+]", idx);
    } else {
      snprintf(info, sizeof info, " [%d/%d]", idx, count);
    }
    tb_print(1 + g_current_command.len, y, COLOR_STATUS_PERM, COLOR_DEFAULT, info);
  }

  tb_set_cursor(g_current_command.cursor + 1,
                y); // offset by one because we prefix with :
}

static void draw_status_prompt(const char *prompt, const char *buf, const int cursor) {
  const int y = tb_height() - 1;
  clear_line(y);
  tb_print(0, y, COLOR_DEFAULT, COLOR_DEFAULT, prompt);
  tb_print(strlen(prompt), y, COLOR_DEFAULT, COLOR_DEFAULT, buf);
  tb_set_cursor(strlen(prompt) + cursor, y);
  tb_present();
}

void draw_status_running_command(void) {
  const int y = tb_height() - 1;
  clear_line(y);
  tb_print(0, y, COLOR_DEFAULT, COLOR_DEFAULT, ">");
  tb_set_cursor(1, y);
  tb_present();
}

static void draw_status_confirmation(const char *msg1, const char *msg2) {
  clear_line(tb_height() - 1);
  tb_printf(0, tb_height() - 1, COLOR_DEFAULT, COLOR_DEFAULT, "%s%s? [y/n]", msg1, msg2);
  tb_present();
}

void draw_screen(const int repeat) {
  pthread_mutex_lock(&g_tb_mutex);

  if (g_msg_clear_top >= 0) {
    for (int i = g_msg_clear_top; i < tb_height(); i++)
      clear_line(i);
    g_msg_clear_top = -1;
  }

  g_msg_line_count = MIN(count_msg_lines(), tb_height() - 1);

  const int column_width = tb_width() / 6;
  draw_left_column(column_width * 0, column_width * 1 - 1);
  draw_middle_column(column_width * 1 + 2, column_width * 2 - 2);
  draw_right_column(column_width * 3 + 1, column_width * 3 - 1);

  draw_path();

  if (g_msg[0] != '\0') {
    draw_message();
  } else {
    g_msg_line_count = 0;
    switch (g_current_mode) {
    case MODE_COMMAND:
      draw_status_command();
      break;
    case MODE_NORMAL:
      draw_status_normal(repeat);
      break;
    }
  }
  tb_present();
  pthread_mutex_unlock(&g_tb_mutex);
}
