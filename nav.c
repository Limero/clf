#pragma once

#include "command.c"
#include "copy.c"
#include "draw.c"
#include "global.h"

static const int KEY_COMMANDS_LEN = sizeof(KEY_COMMANDS) / sizeof(KEY_COMMANDS[0]);

static int nav_handle_input_key(const struct tb_event *ev, char *buf, int *cursor, int *len) {
  switch (ev->key) {
  case TB_KEY_ENTER:
    return 1;
  case TB_KEY_ESC:
    return -1;
  case TB_KEY_ARROW_LEFT:
    if (*cursor > 0)
      (*cursor)--;
    return 0;
  case TB_KEY_ARROW_RIGHT:
    if (*cursor < *len)
      (*cursor)++;
    return 0;
  case TB_KEY_CTRL_A:
  case TB_KEY_HOME:
    *cursor = 0;
    return 0;
  case TB_KEY_CTRL_E:
  case TB_KEY_END:
    *cursor = *len;
    return 0;
  case TB_KEY_CTRL_U:
    buf[0] = '\0';
    *cursor = 0;
    *len = 0;
    return 0;
  case TB_KEY_BACKSPACE:
  case TB_KEY_BACKSPACE2:
    if (*cursor > 0) {
      (*cursor)--;
      (*len)--;
      string_remove_at(buf, *cursor);
    }
    return 0;
  default:
    if (ev->ch) {
      string_insert_at(buf, *cursor, ev->ch);
      (*cursor)++;
      (*len)++;
    }
    return 0;
  }
}

static bool nav_get_confirmation(const char *msg1, const char *msg2) {
  tb_hide_cursor();
  draw_status_confirmation(msg1, msg2);
  struct tb_event ev = {0};
  tb_poll_event(&ev);
  return ev.ch == 'y' || ev.ch == 'Y';
}

// Show a prompt and return a single character from the user
static int nav_get_prompt_char(const char *prompt) {
  const int y = tb_height() - 1;
  const int prompt_len = strlen(prompt);
  int rendered_lines = 0;

  pthread_mutex_lock(&g_tb_mutex);
  if (g_msg[0] != '\0') {
    rendered_lines = render_multiline_msg(tb_height() - 2);
    if (rendered_lines > 1)
      g_msg_line_count = rendered_lines;
    g_msg[0] = '\0';
  }

  clear_line(y);
  tb_print(0, y, COLOR_DEFAULT, COLOR_DEFAULT, prompt);
  tb_set_cursor(prompt_len, y);
  tb_present();
  pthread_mutex_unlock(&g_tb_mutex);

  struct tb_event ev = {0};
  tb_poll_event(&ev);

  if (rendered_lines > 0) {
    pthread_mutex_lock(&g_tb_mutex);
    for (int i = 0; i < rendered_lines; i++) {
      clear_line(y - rendered_lines + i);
    }
    g_msg_line_count = 0;
    pthread_mutex_unlock(&g_tb_mutex);
  }

  return ev.ch;
}

// Show a prompt with editable input buffer, returns the result or NULL on cancel
static char *nav_get_prompt_input(const char *prompt, const char *initial) {
  static char buf[4096];
  strlcpy(buf, initial, sizeof buf);
  int cursor = strlen(buf);
  int len = cursor;

  while (1) {
    draw_status_prompt(prompt, buf, cursor);
    struct tb_event ev = {0};
    tb_poll_event(&ev);

    int r = nav_handle_input_key(&ev, buf, &cursor, &len);
    if (r == 1)
      return buf;
    if (r == -1)
      return NULL;
  }
}

static void nav_move_to_line(const int l) {
  if (g_cursor.idx == l)
    return;
  g_update.dir_right = true;
  tb_clear();
  g_cursor.idx = l;
  g_right_column_idx = 0;
}

static void nav_move_top(void) {
  nav_move_to_line(0);
}

static void nav_move_bottom(void) {
  nav_move_to_line(g_items_in_middle_dir - 1);
}

static void nav_move_up(const int n) {
  if (g_cursor.idx - n < 0) {
    if (n > 1) {
      nav_move_top();
    } else if (OPT_WRAP_SCROLL) {
      nav_move_bottom();
    }
    return;
  }

  g_update.dir_right = true;
  tb_clear();
  g_cursor.idx -= n;
  g_right_column_idx = 0;
}

static void nav_move_down(const int n) {
  if (g_cursor.idx + n + 1 > g_items_in_middle_dir) {
    if (n > 1) {
      nav_move_bottom();
    } else if (OPT_WRAP_SCROLL) {
      nav_move_top();
    }
    return;
  }

  g_update.dir_right = true;
  tb_clear();
  g_cursor.idx += n;
  g_right_column_idx = 0;
}

static void nav_move_page_up(void) {
  g_update.dir_right = true;
  tb_clear();
  g_cursor.idx = MAX(g_cursor.idx - tb_height() / 2, 0);
  g_right_column_idx = 0;
}

static void nav_move_page_down(void) {
  g_update.dir_right = true;
  tb_clear();
  g_cursor.idx = MIN(g_cursor.idx + tb_height() / 2, g_items_in_middle_dir - 1);
  g_right_column_idx = 0;
}

static void nav_dir_back(void) {
  if (strcmp(g_cwd, "/") == 0) {
    return;
  }

  strlcpy(g_prev_cwd, g_cwd, sizeof g_prev_cwd);

  g_update.dir_left = true;
  g_update.dir_middle = true;
  g_update.dir_right = true;
  tb_clear();

  if (chdir("../") != 0) {
    snprintf(g_msg, sizeof g_msg, "(%s chdir) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }
  if (!getcwd(g_cwd, sizeof(g_cwd))) {
    snprintf(g_msg, sizeof g_msg, "(%s getcwd) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }
  g_right_column_idx = g_cursor.idx;
  g_cursor.idx = g_left_column_idx;
  g_cursor.name[0] = '\0';
}

static void nav_dir_enter(void) {
  strlcpy(g_prev_cwd, g_cwd, sizeof g_prev_cwd);

  g_update.dir_left = true;
  g_update.dir_middle = true;
  g_update.dir_right = true;
  tb_clear();

  if (chdir(g_cursor.name) == -1) {
    switch (errno) {
    case ENOTDIR:
      tb_shutdown();
      os_exec(CMD_OPEN, g_cursor.name);
      tb_init();
      return;
    default:
      snprintf(g_msg, sizeof g_msg, "(%s chdir) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
    }
    return;
  }
  if (!getcwd(g_cwd, sizeof(g_cwd))) {
    snprintf(g_msg, sizeof g_msg, "(%s getcwd) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }
  g_cursor.idx = g_right_column_idx;
  g_cursor.name[0] = '\0';
  g_right_column_idx = 0;
}

static void nav_switch_mode(const modes_t new_mode) {
  switch (new_mode) {
  case MODE_COMMAND:
    g_current_mode = MODE_COMMAND;
    command_mode_enter();
    break;
  case MODE_NORMAL:
    g_current_mode = MODE_NORMAL;
    tb_hide_cursor();
    break;
  }
}

static int nav_up(const int repeat) {
  nav_move_up(repeat ? repeat : 1);
  return 0;
}

static int nav_down(const int repeat) {
  nav_move_down(repeat ? repeat : 1);
  return 0;
}

static int nav_left(const int repeat) {
  (void)repeat;
  nav_dir_back();
  return 0;
}

static int nav_right(const int repeat) {
  (void)repeat;
  nav_dir_enter();
  return 0;
}

static int nav_goto(const int repeat) {
  nav_move_to_line(repeat ? repeat - 1 : 0);
  return 0;
}

static int nav_bottom(const int repeat) {
  (void)repeat;
  nav_move_bottom();
  return 0;
}

static int nav_page_up(const int repeat) {
  (void)repeat;
  nav_move_page_up();
  return 0;
}

static int nav_page_down(const int repeat) {
  (void)repeat;
  nav_move_page_down();
  return 0;
}

static int nav_multi_select(const int repeat) {
  (void)repeat;
  if (!OPT_MULTISELECT || !g_items_in_middle_dir)
    return 0;
  char path[PATH_MAX];
  const int needed = snprintf(path, sizeof path, "%s/%s", g_cwd, g_namelist_middle[g_cursor.idx]->d_name);
  if (needed < 0 || needed >= (int)sizeof path) {
    snprintf(g_msg, sizeof g_msg, "(%s snprintf) path too long", __func__);
    g_msg_type = MSG_TYPE_ERROR;
    return 0;
  }
  const int found = selected_index(path);
  if (found >= 0) {
    g_selected_count--;
    if (found < g_selected_count)
      strlcpy(g_selected_paths[found], g_selected_paths[g_selected_count], sizeof g_selected_paths[0]);
  } else if (g_selected_count >= MAX_SELECTED) {
    snprintf(g_msg, sizeof g_msg, "max %d selected files", MAX_SELECTED);
    g_msg_type = MSG_TYPE_ERROR;
    return 0;
  } else {
    strlcpy(g_selected_paths[g_selected_count++], path, sizeof g_selected_paths[0]);
  }
  g_cursor.name[0] = '\0';
  g_update.dir_middle = true;
  tb_clear();
  nav_move_down(1);
  return 0;
}

static int nav_yank(const int repeat, const bool is_cut) {
  (void)repeat;
  if (g_selected_count > 0) {
    const char *paths[MAX_SELECTED];
    for (int i = 0; i < g_selected_count; i++)
      paths[i] = g_selected_paths[i];
    copy_yank(paths, g_selected_count, is_cut);
    g_selected_count = 0;
    g_update.dir_middle = true;
    tb_clear();
  } else {
    if (g_cursor.name[0] == '\0') {
      snprintf(g_msg, sizeof g_msg, "no file selected");
      g_msg_type = MSG_TYPE_ERROR;
      return 0;
    }
    char cwd_with_cursor[PATH_MAX];
    const int needed = snprintf(cwd_with_cursor, PATH_MAX, "%s/%s", g_cwd, g_cursor.name);
    if (needed < 0 || needed >= PATH_MAX) {
      snprintf(g_msg, sizeof g_msg, "(%s snprintf) path too long", __func__);
      g_msg_type = MSG_TYPE_ERROR;
      return 0;
    }
    const char *p = cwd_with_cursor;
    copy_yank(&p, 1, is_cut);
  }
  return 0;
}

static int nav_yank_copy(const int repeat) {
  return nav_yank(repeat, false);
}
static int nav_yank_cut(const int repeat) {
  return nav_yank(repeat, true);
}

static int nav_delete(const int repeat) {
  (void)repeat;
  if (!g_items_in_middle_dir)
    return 0;
  if (g_selected_count > 0) {
    char msg[64];
    snprintf(msg, sizeof msg, "%d items", g_selected_count);
    if (nav_get_confirmation("delete ", msg)) {
      for (int i = 0; i < g_selected_count; i++)
        os_exec_output(CMD_DELETE, g_selected_paths[i]);
      g_selected_count = 0;
      g_update.dir_middle = true;
      g_update.dir_right = true;
      tb_clear();
    }
  } else if (nav_get_confirmation("delete ", g_cursor.name)) {
    os_exec_output(CMD_DELETE, g_cursor.name);
    g_cursor.idx = g_cursor.idx ? g_cursor.idx - 1 : 0;
    g_update.dir_middle = true;
    g_update.dir_right = true;
    tb_clear();
  }
  return 0;
}

static int nav_paste(const int repeat) {
  (void)repeat;
  if (!copy_paste(g_cwd))
    return 0;
  g_update.dir_left = true;
  g_update.dir_middle = true;
  g_update.dir_right = true;
  tb_clear();
  return 0;
}

static int nav_rename(const int repeat) {
  (void)repeat;
  if (!g_items_in_middle_dir)
    return 0;
  char *new_name = nav_get_prompt_input("rename: ", g_cursor.name);
  if (new_name && new_name[0] && strcmp(new_name, g_cursor.name) != 0) {
    char dest_path[PATH_MAX * 2];
    snprintf(dest_path, sizeof dest_path, "%s/%s", g_cwd, new_name);
    if (access(dest_path, F_OK) == 0) {
      if (!nav_get_confirmation("replace ", new_name))
        return 0;
    }
    os_move(g_cursor.name, new_name);
    memcpy(g_cursor.name, new_name, sizeof(g_cursor.name));
    g_update.dir_middle = true;
    g_update.dir_right = true;
    tb_clear();
  }
  tb_hide_cursor();
  return 0;
}

static int nav_search_next(const int repeat) {
  (void)repeat;
  if (g_search_idx_before == -1)
    return 0;
  set_cursor_idx_to_search(true, g_cursor.idx + 1);
  tb_clear();
  return 0;
}

static int nav_search_prev(const int repeat) {
  (void)repeat;
  if (g_search_idx_before == -1)
    return 0;
  set_cursor_idx_to_search(false, g_cursor.idx - 1);
  tb_clear();
  return 0;
}

static int nav_search(const int repeat) {
  (void)repeat;
  g_search_idx_before = g_cursor.idx;
  nav_switch_mode(MODE_COMMAND);
  return 0;
}

static int nav_command(const int repeat) {
  (void)repeat;
  g_search_idx_before = -1;
  nav_switch_mode(MODE_COMMAND);
  return 0;
}

static int nav_shell(const int repeat) {
  (void)repeat;
  tb_shutdown();
  os_exec("$SHELL", "");
  tb_init();
  return 0;
}

static int nav_find(const int repeat, const bool forward) {
  (void)repeat;
  if (!g_items_in_middle_dir)
    return 0;
  const int ch = nav_get_prompt_char(forward ? "find: " : "find-back: ");
  if (ch && !set_cursor_idx_to_locate(forward, g_cursor.idx + (forward ? 1 : -1), ch)) {
    snprintf(g_msg, sizeof g_msg, "find: pattern not found: %c", ch);
    g_msg_type = MSG_TYPE_ERROR;
  }
  tb_hide_cursor();
  tb_clear();
  return 0;
}

static int nav_find_forward(const int repeat) {
  return nav_find(repeat, true);
}
static int nav_find_backward(const int repeat) {
  return nav_find(repeat, false);
}

static int nav_cd_prev(const int repeat) {
  (void)repeat;
  if (g_prev_cwd[0] == '\0') {
    snprintf(g_msg, sizeof g_msg, "no previous directory");
    g_msg_type = MSG_TYPE_ERROR;
    return 0;
  }

  char target[PATH_MAX];
  strlcpy(target, g_prev_cwd, sizeof target);

  if (chdir(target) != 0) {
    snprintf(g_msg, sizeof g_msg, "(%s chdir) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return 0;
  }

  strlcpy(g_prev_cwd, g_cwd, sizeof g_prev_cwd);
  strlcpy(g_cwd, target, sizeof g_cwd);

  g_update.dir_left = true;
  g_update.dir_middle = true;
  g_update.dir_right = true;
  tb_clear();
  g_cursor.idx = 0;
  g_right_column_idx = 0;
  g_cursor.name[0] = '\0';

  snprintf(g_msg, sizeof g_msg, "%s", g_cwd);
  g_msg_type = MSG_TYPE_INFO;
  return 0;
}

static int nav_quit(const int repeat) {
  (void)repeat;
  return -1;
}

typedef int (*action_fn)(int repeat);

static const struct {
  const char *name;
  action_fn fn;
} BUILTIN_ACTIONS[] = {
    {"nav_up", nav_up},
    {"nav_down", nav_down},
    {"nav_left", nav_left},
    {"nav_right", nav_right},
    {"nav_goto", nav_goto},
    {"nav_bottom", nav_bottom},
    {"nav_page_up", nav_page_up},
    {"nav_page_down", nav_page_down},
    {"multi_select", nav_multi_select},
    {"yank_copy", nav_yank_copy},
    {"yank_cut", nav_yank_cut},
    {"delete", nav_delete},
    {"paste", nav_paste},
    {"rename", nav_rename},
    {"search_next", nav_search_next},
    {"search_prev", nav_search_prev},
    {"search", nav_search},
    {"command", nav_command},
    {"shell", nav_shell},
    {"quit", nav_quit},
    {"nav_cd_prev", nav_cd_prev},
    {"find", nav_find_forward},
    {"find_back", nav_find_backward},
};
static const int BUILTIN_ACTIONS_LEN = sizeof(BUILTIN_ACTIONS) / sizeof(BUILTIN_ACTIONS[0]);

// returns true if program should exit
static bool nav_execute_cmd(const char *cmd, const int repeat) {
  if (cmd[0] == ':') {
    os_exec_output_deferred(cmd + 1, "", draw_status_running_command, OPT_CMD_INDICATOR_DELAY_MS);
    g_update.dir_left = true;
    g_update.dir_middle = true;
    g_update.dir_right = true;
    tb_clear();
    return false;
  }
  for (int j = 0; j < BUILTIN_ACTIONS_LEN; j++) {
    if (strcmp(cmd, BUILTIN_ACTIONS[j].name) == 0) {
      return BUILTIN_ACTIONS[j].fn(repeat) < 0;
    }
  }
  snprintf(g_msg, sizeof g_msg, "unknown action: %s", cmd);
  g_msg_type = MSG_TYPE_ERROR;
  tb_clear();
  return false;
}

// Returns -1 = quit, 0 = command executed, reset repeat 1 = digit accumulated, don't reset repeat
static int nav_handle_event_character_normal(const struct tb_event *ev, int *repeat) {
  if (ev->ch >= '0' && ev->ch <= '9') {
    if (*repeat < 1000)
      *repeat = (*repeat * 10) + (ev->ch - '0');
    return 1;
  }

  bool has_prefix = false;
  for (int i = 0; i < KEY_COMMANDS_LEN; i++) {
    if (KEY_COMMANDS[i].key2 != 0 && KEY_COMMANDS[i].key1 == ev->ch) {
      has_prefix = true;
      break;
    }
  }

  if (has_prefix) {
    int msg_offset = 0;
    for (int i = 0; i < KEY_COMMANDS_LEN; i++) {
      if (KEY_COMMANDS[i].key1 == ev->ch && KEY_COMMANDS[i].key2 != 0) {
        msg_offset += snprintf(g_msg + msg_offset, sizeof g_msg - msg_offset, "%c%c  %s\n", KEY_COMMANDS[i].key1,
                               KEY_COMMANDS[i].key2, KEY_COMMANDS[i].cmd);
      }
    }
    if (msg_offset > 0)
      g_msg[msg_offset - 1] = '\0';
    g_msg_type = MSG_TYPE_INFO;

    char prompt[2];
    snprintf(prompt, sizeof prompt, "%c", ev->ch);
    const int ch2 = nav_get_prompt_char(prompt);
    tb_hide_cursor();

    for (int i = 0; i < KEY_COMMANDS_LEN; i++) {
      if (KEY_COMMANDS[i].key1 == ev->ch && KEY_COMMANDS[i].key2 == ch2) {
        g_msg[0] = '\0';
        if (nav_execute_cmd(KEY_COMMANDS[i].cmd, *repeat))
          return -1;
        return 0;
      }
    }

    snprintf(g_msg, sizeof g_msg, "unknown mapping: %c%c", ev->ch, ch2);
    g_msg_type = MSG_TYPE_ERROR;
    return 0;
  }

  for (int i = 0; i < KEY_COMMANDS_LEN; i++) {
    if (KEY_COMMANDS[i].key2 == 0 && ev->ch == KEY_COMMANDS[i].key1) {
      if (nav_execute_cmd(KEY_COMMANDS[i].cmd, *repeat))
        return -1;
      return 0;
    }
  }

  snprintf(g_msg, sizeof g_msg, "unknown mapping: %c", ev->ch);
  g_msg_type = MSG_TYPE_ERROR;
  return 0;
}

static int nav_handle_event_normal(const struct tb_event *ev, int *repeat) {
  if (ev->ch)
    return nav_handle_event_character_normal(ev, repeat);

  switch (ev->key) {
  case TB_KEY_ARROW_UP:
    nav_move_up(*repeat ? *repeat : 1);
    return 0;
  case TB_KEY_ARROW_DOWN:
    nav_move_down(*repeat ? *repeat : 1);
    return 0;
  case TB_KEY_ARROW_LEFT:
    nav_dir_back();
    return 0;
  case TB_KEY_ENTER:
  case TB_KEY_ARROW_RIGHT:
    nav_dir_enter();
    return 0;
  case TB_KEY_HOME:
    nav_move_top();
    return 0;
  case TB_KEY_END:
    nav_move_bottom();
    return 0;
  case TB_KEY_CTRL_U:
  case TB_KEY_PGUP:
    nav_move_page_up();
    return 0;
  case TB_KEY_CTRL_D:
  case TB_KEY_PGDN:
    nav_move_page_down();
    return 0;
  }

  return 0;
}

static int nav_handle_event_command(const struct tb_event *ev) {
  switch (ev->key) {
  case TB_KEY_ARROW_UP:
  case TB_KEY_CTRL_P:
    if (OPT_CMD_COMPLETE)
      complete_reset();
    command_history_prev();
    return 0;
  case TB_KEY_ARROW_DOWN:
  case TB_KEY_CTRL_N:
    if (OPT_CMD_COMPLETE)
      complete_reset();
    command_history_next();
    return 0;
  case TB_KEY_TAB:
    if (OPT_CMD_COMPLETE && g_search_idx_before < 0) {
      complete_handle_tab();
    }
    return 0;
  case TB_KEY_BACK_TAB:
    if (OPT_CMD_COMPLETE && g_search_idx_before < 0) {
      complete_handle_shift_tab();
    }
    return 0;
  }

  if (OPT_CMD_COMPLETE)
    complete_reset();
  const int prev_len = g_current_command.len;
  const int r = nav_handle_input_key(ev, g_current_command.chars, &g_current_command.cursor, &g_current_command.len);

  if (r == 1) {
    if (g_search_idx_before >= 0) {
      if (!OPT_INCSEARCH)
        set_cursor_idx_to_search(true, 0);
      nav_switch_mode(MODE_NORMAL);
      tb_clear();
      return 0;
    }
    if (g_current_command.chars[0] == 'q' && g_current_command.chars[1] == '\0') {
      return -1;
    }
    command_history_add();
    if (strcmp(g_current_command.chars, "cd -") == 0) {
      nav_cd_prev(0);
      nav_switch_mode(MODE_NORMAL);
      return 0;
    }
    os_exec_output_deferred(g_current_command.chars, "", draw_status_running_command, OPT_CMD_INDICATOR_DELAY_MS);
    nav_switch_mode(MODE_NORMAL);
    g_update.dir_left = true;
    g_update.dir_middle = true;
    g_update.dir_right = true;
    tb_clear();
    return 0;
  }

  const bool exit_to_normal =
      r == -1 || ((ev->key == TB_KEY_BACKSPACE || ev->key == TB_KEY_BACKSPACE2) && prev_len == 0);
  if (exit_to_normal) {
    if (g_search_idx_before >= 0) {
      g_cursor.idx = g_search_idx_before;
      g_search_idx_before = -1;
    }
    nav_switch_mode(MODE_NORMAL);
    return 0;
  }

  if (OPT_INCSEARCH && g_search_idx_before >= 0 &&
      (ev->ch || ev->key == TB_KEY_BACKSPACE || ev->key == TB_KEY_BACKSPACE2)) {
    set_cursor_idx_to_search(true, 0);
    tb_clear();
  }

  return 0;
}

int nav_handle_event(const struct tb_event *ev, int *repeat) {
  switch (ev->type) {
  case TB_EVENT_RESIZE:
    g_update.dir_right = true;
    tb_clear();
    return 0;
  }

  switch (ev->key) {
  case TB_KEY_CTRL_H:
    g_show_hidden = !g_show_hidden;
    g_update.dir_middle = true;
    g_update.dir_right = true;
    tb_clear();
    return 0;
  case TB_KEY_CTRL_R:
    g_update.dir_left = true;
    g_update.dir_middle = true;
    g_update.dir_right = true;
    tb_clear();
    return 0;
  case TB_KEY_CTRL_C:
    return -1;
  }

  switch (g_current_mode) {
  case MODE_NORMAL:
    return nav_handle_event_normal(ev, repeat);
  case MODE_COMMAND:
    return nav_handle_event_command(ev);
  }

  return 0;
}
