#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct {
  char chars[1024];
  int len; // without NUL-termination
  int cursor;
} g_current_command;

static struct {
  char history[100][256];
  int count;
  int cursor;
} command_history;

static void string_insert_at(char *text, const int idx, const char c) {
  for (int i = strlen(text) + 1; i > idx; --i) {
    text[i] = text[i - 1];
  }

  text[idx] = c;
}

static void string_remove_at(char *text, const int idx) {
  for (int i = idx;; i++) {
    text[i] = text[i + 1];
    if (text[i] == '\0') {
      return;
    }
  }
}

void command_mode_enter(void) {
  g_current_command.cursor = 0;
  g_current_command.chars[0] = '\0';
  g_current_command.len = 0;
  command_history.cursor = command_history.count;
}

void command_history_prev(void) {
  if (command_history.cursor == 0) {
    return;
  }
  command_history.cursor--;
  strlcpy(g_current_command.chars, command_history.history[command_history.cursor], sizeof(g_current_command.chars));
  int len = strlen(g_current_command.chars);
  g_current_command.len = len;
  g_current_command.cursor = len;
}

void command_history_next(void) {
  if (command_history.cursor >= command_history.count) {
    return;
  }
  command_history.cursor++;
  strlcpy(g_current_command.chars, command_history.history[command_history.cursor], sizeof(g_current_command.chars));
  const int len = strlen(g_current_command.chars);
  g_current_command.len = len;
  g_current_command.cursor = len;
}

void command_history_add(void) {
  strlcpy(command_history.history[command_history.count], g_current_command.chars,
          sizeof(command_history.history[command_history.count]));
  command_history.count++;
}

void command_input_add(const uint32_t ch) {
  g_current_command.len++;
  if (g_current_command.cursor < g_current_command.len) {
    // middle of command
    string_insert_at(g_current_command.chars, g_current_command.cursor, ch);
  } else {
    // end of command
    g_current_command.chars[g_current_command.cursor] = ch;
    g_current_command.chars[g_current_command.len] = '\0';
  }
  g_current_command.cursor++;
}

bool command_input_backspace(void) {
  if (g_current_command.len > 0 && g_current_command.cursor > 0) {
    g_current_command.len--;
    g_current_command.cursor--;
    if (g_current_command.cursor < g_current_command.len) {
      // middle of command
      string_remove_at(g_current_command.chars, g_current_command.cursor);
    } else {
      // end of command
      g_current_command.chars[g_current_command.len] = '\0';
    }
    return false; // stay in command mode
  }
  return true; // enter normal mode
}
