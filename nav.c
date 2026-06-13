#pragma once

#include "copy.c"
#include "draw.c"
#include "global.h"

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
  draw_status_confirmation(msg1, msg2);
  struct tb_event ev;
  tb_poll_event(&ev);
  return ev.ch == 'y' || ev.ch == 'Y';
}

static char *nav_get_prompt_input(const char *prompt, const char *initial) {
  static char buf[4096];
  strlcpy(buf, initial, sizeof buf);
  int cursor = strlen(buf);
  int len = cursor;

  while (1) {
    draw_status_prompt(prompt, buf, cursor);
    struct tb_event ev;
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

static void nav_top(void) {
  nav_move_to_line(0);
}

static void nav_bottom(void) {
  nav_move_to_line(g_items_in_middle_dir - 1);
}

static void nav_up(const int n) {
  if (g_cursor.idx - n < 0) {
    if (n > 1) {
      nav_top();
    } else if (OPT_WRAP_SCROLL) {
      nav_bottom();
    }
    return;
  }

  g_update.dir_right = true;
  tb_clear();
  g_cursor.idx -= n;
  g_right_column_idx = 0;
}

static void nav_down(const int n) {
  if (g_cursor.idx + n + 1 > g_items_in_middle_dir) {
    if (n > 1) {
      nav_bottom();
    } else if (OPT_WRAP_SCROLL) {
      nav_top();
    }
    return;
  }

  g_update.dir_right = true;
  tb_clear();
  g_cursor.idx += n;
  g_right_column_idx = 0;
}

static void nav_page_up(void) {
  g_update.dir_right = true;
  tb_clear();
  g_cursor.idx = MAX(g_cursor.idx - tb_height() / 2, 0);
  g_right_column_idx = 0;
}

static void nav_page_down(void) {
  g_update.dir_right = true;
  tb_clear();
  g_cursor.idx = MIN(g_cursor.idx + tb_height() / 2, g_items_in_middle_dir - 1);
  g_right_column_idx = 0;
}

static void nav_left(void) {
  if (strcmp(g_cwd, "/") == 0) {
    return;
  }

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
}

static void nav_right(void) {
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

static int nav_handle_event_normal(const struct tb_event *ev, int *repeat) {
  switch (ev->key) {
  case TB_KEY_ARROW_UP:
    nav_up(*repeat ? *repeat : 1);
    return 0;
  case TB_KEY_ARROW_DOWN:
    nav_down(*repeat ? *repeat : 1);
    return 0;
  case TB_KEY_ARROW_LEFT:
    nav_left();
    return 0;
  case TB_KEY_ENTER:
  case TB_KEY_ARROW_RIGHT:
    nav_right();
    return 0;
  case TB_KEY_HOME:
    nav_move_to_line(*repeat ? *repeat - 1 : 0);
    return 0;
  case TB_KEY_END:
    nav_bottom();
    return 0;
  case TB_KEY_CTRL_U:
  case TB_KEY_PGUP:
    nav_page_up();
    return 0;
  case TB_KEY_CTRL_D:
  case TB_KEY_PGDN:
    nav_page_down();
    return 0;
  }

  switch (ev->ch) {
  case 'k':
    nav_up(*repeat ? *repeat : 1);
    return 0;
  case 'j':
    nav_down(*repeat ? *repeat : 1);
    return 0;
  case 'h':
    nav_left();
    return 0;
  case 'l':
    nav_right();
    return 0;
  case 'g':
    nav_move_to_line(*repeat ? *repeat - 1 : 0);
    return 0;
  case 'G':
    nav_bottom();
    return 0;
  case ' ':
    if (!g_items_in_middle_dir) {
      return 0;
    }
    // TODO: select
    return 0;
  case 'y':
  case 'd': {
    if (g_cursor.name[0] == '\0') {
      snprintf(g_msg, sizeof g_msg, "no file selected");
      g_msg_type = MSG_TYPE_ERROR;
      return 0;
    }
    char cwd_with_cursor[PATH_MAX];
    int needed = snprintf(cwd_with_cursor, PATH_MAX, "%s/%s", g_cwd, g_cursor.name);
    if (needed < 0 || needed >= PATH_MAX) {
      snprintf(g_msg, sizeof g_msg, "(%s snprintf) path too long", __func__);
      g_msg_type = MSG_TYPE_ERROR;
      return 0;
    }
    copy_yank(cwd_with_cursor, ev->ch == 'd');
    return 0;
  }
  case 'D':
    if (!g_items_in_middle_dir) {
      return 0;
    }
    if (nav_get_confirmation("delete ", g_cursor.name)) {
      os_exec_output(CMD_DELETE, g_cursor.name);
      g_cursor.idx = g_cursor.idx ? g_cursor.idx - 1 : 0;
      g_update.dir_middle = true;
      g_update.dir_right = true;
      tb_clear();
    }
    return 0;
  case 'p':
    if (!copy_paste(g_cwd)) {
      return 0;
    }
    g_update.dir_left = true;
    g_update.dir_middle = true;
    g_update.dir_right = true;
    tb_clear();
    return 0;
  case 'r': {
    if (!g_items_in_middle_dir) {
      return 0;
    }
    char *new_name = nav_get_prompt_input("rename: ", g_cursor.name);
    if (new_name && new_name[0] && strcmp(new_name, g_cursor.name) != 0) {
      os_move(g_cursor.name, new_name);
      g_update.dir_middle = true;
      g_update.dir_right = true;
      tb_clear();
    }
    tb_hide_cursor();
    return 0;
  }
  case 'n':
    if (g_search_idx_before == -1) {
      return 0;
    }
    set_cursor_idx_to_search(true, g_cursor.idx + 1);
    tb_clear();
    return 0;
  case 'N':
    if (g_search_idx_before == -1) {
      return 0;
    }
    set_cursor_idx_to_search(false, g_cursor.idx - 1);
    tb_clear();
    return 0;
  case '/':
  case '?': // use N to search backwards, no need to have separate logic for it
    g_search_idx_before = g_cursor.idx;
    nav_switch_mode(MODE_COMMAND);
    return 0;
  case ':':
    g_search_idx_before = -1;
    nav_switch_mode(MODE_COMMAND);
    return 0;
  case 'S':
    tb_shutdown();
    os_exec("$SHELL", "");
    tb_init();
    return 0;
  case 'q':
    return -1;
  }

  if (ev->ch >= '0' && ev->ch <= '9') {
    if (*repeat < 1000)
      *repeat = (*repeat * 10) + (ev->ch - '0');
    return 1;
  }

  return 0;
}

static int nav_handle_event_command(const struct tb_event *ev) {
  switch (ev->key) {
  case TB_KEY_ARROW_UP:
  case TB_KEY_CTRL_P:
    command_history_prev();
    return 0;
  case TB_KEY_ARROW_DOWN:
  case TB_KEY_CTRL_N:
    command_history_next();
    return 0;
  }

  const int prev_len = g_current_command.len;
  const int r = nav_handle_input_key(ev, g_current_command.chars, &g_current_command.cursor, &g_current_command.len);

  if (r == 1) {
    if (g_search_idx_before >= 0) {
      nav_switch_mode(MODE_NORMAL);
      tb_clear();
      return 0;
    }
    if (g_current_command.chars[0] == 'q' && g_current_command.chars[1] == '\0') {
      return -1;
    }
    command_history_add();
    os_exec_output_deferred(g_current_command.chars, "", draw_status_running_command, CMD_INDICATOR_DELAY_MS);
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

  if (g_search_idx_before >= 0 && (ev->ch || ev->key == TB_KEY_BACKSPACE || ev->key == TB_KEY_BACKSPACE2)) {
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
