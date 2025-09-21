#pragma once

#include "copy.c"
#include "draw.c"
#include "global.h"

static void nav_move_to_line(const int l) {
  if (g_current_selection.idx == l)
    return;
  g_update.dir_right = true;
  tb_clear();
  g_current_selection.idx = l;
  g_right_column_idx = 0;
}

static void nav_top(void) {
  nav_move_to_line(0);
}

static void nav_bottom(void) {
  nav_move_to_line(g_items_in_middle_dir - 1);
}

static void nav_up(const int n) {
  if (g_current_selection.idx - n < 0) {
    if (n > 1) {
      nav_top();
    } else if (OPT_WRAP_SCROLL) {
      nav_bottom();
    }
    return;
  }

  g_update.dir_right = true;
  tb_clear();
  g_current_selection.idx -= n;
  g_right_column_idx = 0;
}

static void nav_down(const int n) {
  if (g_current_selection.idx + n + 1 > g_items_in_middle_dir) {
    if (n > 1) {
      nav_bottom();
    } else if (OPT_WRAP_SCROLL) {
      nav_top();
    }
    return;
  }

  g_update.dir_right = true;
  tb_clear();
  g_current_selection.idx += n;
  g_right_column_idx = 0;
}

static void nav_page_up(void) {
  g_update.dir_right = true;
  tb_clear();
  g_current_selection.idx = MAX(g_current_selection.idx - tb_height() / 2, 0);
  g_right_column_idx = 0;
}

static void nav_page_down(void) {
  g_update.dir_right = true;
  tb_clear();
  g_current_selection.idx = MIN(g_current_selection.idx + tb_height() / 2, g_items_in_middle_dir - 1);
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
  g_right_column_idx = g_current_selection.idx;
  g_current_selection.idx = g_left_column_idx;
}

static void nav_right(void) {
  g_update.dir_left = true;
  g_update.dir_middle = true;
  g_update.dir_right = true;
  tb_clear();

  if (chdir(g_current_selection.name) == -1) {
    switch (errno) {
    case ENOTDIR:
      tb_shutdown();
      os_exec(CMD_OPEN, g_current_selection.name);
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
  g_current_selection.idx = g_right_column_idx;
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
    if (g_current_selection.name[0] == '\0') {
      snprintf(g_msg, sizeof g_msg, "no file selected");
      g_msg_type = MSG_TYPE_ERROR;
      return 0;
    }
    char cwd_with_selection[PATH_MAX];
    int needed = snprintf(cwd_with_selection, PATH_MAX, "%s/%s", g_cwd, g_current_selection.name);
    if (needed < 0 || needed >= PATH_MAX) {
      snprintf(g_msg, sizeof g_msg, "(%s snprintf) path too long", __func__);
      g_msg_type = MSG_TYPE_ERROR;
      return 0;
    }
    copy_yank(cwd_with_selection, ev->ch == 'd');
    return 0;
  }
  case 'D':
    if (!g_items_in_middle_dir) {
      return 0;
    }
    if (draw_confirmation("delete ", g_current_selection.name)) {
      os_exec_output(CMD_DELETE, g_current_selection.name);
      g_current_selection.idx = g_current_selection.idx ? g_current_selection.idx - 1 : 0;
      g_update.dir_middle = true;
      g_update.dir_right = true;
      tb_clear();
    }
    return 0;
  case 'p':
    copy_paste(g_cwd);
    // TODO: Refresh when paste is actually done
    usleep(500000);
    g_update.dir_left = true;
    g_update.dir_middle = true;
    g_update.dir_right = true;
    tb_clear();
    return 0;
  case 'r':
    if (!g_items_in_middle_dir) {
      return 0;
    }
    nav_switch_mode(MODE_COMMAND);
    snprintf(g_current_command.chars, sizeof g_current_command.chars, "%s %s %s", CMD_RENAME, g_current_selection.name,
             g_current_selection.name);
    g_current_command.len = strlen(g_current_command.chars);
    g_current_command.cursor = g_current_command.len;
    return 0;
  case 'n':
    if (g_search_idx_before == -1) {
      return 0;
    }
    set_current_selection_idx_to_search(true, g_current_selection.idx + 1);
    tb_clear();
    return 0;
  case 'N':
    if (g_search_idx_before == -1) {
      return 0;
    }
    set_current_selection_idx_to_search(false, g_current_selection.idx - 1);
    tb_clear();
    return 0;
  case '/':
  case '?': // use N to search backwards, no need to have separate logic for it
    g_search_idx_before = g_current_selection.idx;
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
  case TB_KEY_ARROW_LEFT:
    if (g_current_command.cursor > 0) {
      g_current_command.cursor--;
    }
    return 0;
  case TB_KEY_ARROW_RIGHT:
    if (g_current_command.cursor < g_current_command.len) {
      g_current_command.cursor++;
    }
    return 0;
  case TB_KEY_ARROW_UP:
  case TB_KEY_CTRL_P:
    command_history_prev();
    return 0;
  case TB_KEY_ARROW_DOWN:
  case TB_KEY_CTRL_N:
    command_history_next();
    return 0;
  case TB_KEY_CTRL_U:
    command_mode_enter();
    return 0;
  case TB_KEY_CTRL_A:
  case TB_KEY_HOME:
    g_current_command.cursor = 0;
    return 0;
  case TB_KEY_CTRL_E:
  case TB_KEY_END:
    g_current_command.cursor = g_current_command.len;
    return 0;
  case TB_KEY_ESC:
    if (g_search_idx_before >= 0) {
      g_current_selection.idx = g_search_idx_before;
      g_search_idx_before = -1;
    }
    nav_switch_mode(MODE_NORMAL);
    return 0;
  case TB_KEY_ENTER:
    if (g_search_idx_before >= 0) {
      nav_switch_mode(MODE_NORMAL);
      tb_clear();
      return 0;
    }
    if (g_current_command.chars[0] == 'q' && g_current_command.chars[1] == '\0') {
      return -1;
    }
    command_history_add();
    draw_status_running_command();
    os_exec_output(g_current_command.chars, "");
    nav_switch_mode(MODE_NORMAL);
    g_update.dir_left = true;
    g_update.dir_middle = true;
    g_update.dir_right = true;
    tb_clear();
    return 0;
  case TB_KEY_BACKSPACE2:
    if (command_input_backspace()) {
      if (g_search_idx_before >= 0) {
        g_current_selection.idx = g_search_idx_before;
        g_search_idx_before = -1;
      }
      nav_switch_mode(MODE_NORMAL);
      return 0;
    }
    if (g_search_idx_before >= 0) {
      set_current_selection_idx_to_search(true, 0);
      tb_clear();
    }
    return 0;
  }

  if (ev->ch) {
    command_input_add(ev->ch);
    if (g_search_idx_before >= 0) {
      set_current_selection_idx_to_search(true, 0);
      tb_clear();
    }
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
