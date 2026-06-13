#pragma once

#include "os.c"
#include <assert.h>
#include <sys/file.h>

// Define so it can be overriden in tests to never touch main clipboard
#ifndef CLIPBOARD_FILE
#define CLIPBOARD_FILE "/tmp/clf-clipboard"
#endif

#define CLIPBOARD_BUF_SIZE (PATH_MAX * (MAX_SELECTED + 1) + 10)

static char g_clipboard_buf[CLIPBOARD_BUF_SIZE];
static char g_clipboard_paths[MAX_SELECTED][PATH_MAX];

static int parse_clipboard(char *buf, bool *is_move, char (*paths)[PATH_MAX], int max_paths) {
  assert(buf != NULL && is_move != NULL && paths != NULL);
  char *nl = strchr(buf, '\n');
  if (!nl)
    return 0;
  *nl = '\0';

  if (strcmp(buf, "move") == 0)
    *is_move = true;
  else if (strcmp(buf, "copy") == 0)
    *is_move = false;
  else
    return 0;

  char *line = nl + 1;
  int count = 0;
  while (*line && count < max_paths) {
    if (*line == '\n') {
      line++;
      continue;
    }
    nl = strchr(line, '\n');
    if (nl)
      *nl = '\0';
    strlcpy(paths[count], line, PATH_MAX);
    count++;
    if (!nl)
      break;
    line = nl + 1;
  }
  return count;
}

void copy_yank(const char *paths[], const int count, const bool delete_source_on_paste) {
  if (count <= 0)
    return;

  const char *op = delete_source_on_paste ? "move\n" : "copy\n";
  int offset = strlen(op);
  memcpy(g_clipboard_buf, op, offset);

  for (int i = 0; i < count; i++) {
    const int len = snprintf(g_clipboard_buf + offset, sizeof g_clipboard_buf - offset, "%s\n", paths[i]);
    if (len < 0) {
      snprintf(g_msg, sizeof g_msg, "(%s snprintf) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
      return;
    }
    offset += len;
  }

  if ((size_t)offset > sizeof g_clipboard_buf) {
    snprintf(g_msg, sizeof g_msg, "(%s) clipboard buffer too small", __func__);
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  const int fd = open(CLIPBOARD_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s open) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  flock(fd, LOCK_EX);

  if (write(fd, g_clipboard_buf, offset) == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s write) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    close(fd);
    return;
  }

  g_yanked_is_move = delete_source_on_paste;
  g_yanked_count = MIN(count, MAX_SELECTED);
  for (int i = 0; i < g_yanked_count; i++)
    strlcpy(g_yanked_paths[i], paths[i], sizeof g_yanked_paths[0]);

  close(fd);
}

void copy_init(void) {
  g_yanked_count = 0;
  const int fd = open(CLIPBOARD_FILE, O_RDONLY);
  if (fd == -1)
    return;

  flock(fd, LOCK_SH);

  const ssize_t n = read(fd, g_clipboard_buf, sizeof g_clipboard_buf - 1);
  close(fd);

  if (n <= 0)
    return;
  g_clipboard_buf[n] = '\0';

  bool is_move;
  const int path_count = parse_clipboard(g_clipboard_buf, &is_move, g_clipboard_paths, MAX_SELECTED);
  if (path_count > 0) {
    g_yanked_is_move = is_move;
    g_yanked_count = path_count;
    for (int i = 0; i < path_count; i++)
      strlcpy(g_yanked_paths[i], g_clipboard_paths[i], sizeof g_yanked_paths[0]);
  }
}

bool copy_paste(const char *target_dir_path) {
  const int fd = open(CLIPBOARD_FILE, O_RDONLY);
  if (fd == -1) {
    if (errno == ENOENT) {
      snprintf(g_msg, sizeof g_msg, "No files in clipboard");
      g_msg_type = MSG_TYPE_ERROR;
    } else {
      snprintf(g_msg, sizeof g_msg, "(%s open) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
    }
    return false;
  }

  flock(fd, LOCK_SH);

  const ssize_t n = read(fd, g_clipboard_buf, sizeof g_clipboard_buf - 1);
  close(fd);

  if (n <= 0) {
    snprintf(g_msg, sizeof g_msg, "(%s read) %s", __func__, n == 0 ? "empty clipboard" : strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return false;
  }
  g_clipboard_buf[n] = '\0';

  bool is_move;
  const int path_count = parse_clipboard(g_clipboard_buf, &is_move, g_clipboard_paths, MAX_SELECTED);
  if (path_count <= 0) {
    snprintf(g_msg, sizeof g_msg, "(%s) invalid clipboard format", __func__);
    g_msg_type = MSG_TYPE_ERROR;
    return false;
  }

  int success_count = 0;
  for (int i = 0; i < path_count; i++) {
    bool ok = is_move ? os_move(g_clipboard_paths[i], target_dir_path) : os_copy(g_clipboard_paths[i], target_dir_path);
    if (ok)
      success_count++;
  }

  if (success_count > 0) {
    const char *op = is_move ? "Moved" : "Copied";
    snprintf(g_msg, sizeof g_msg, "%s %d/%d", op, success_count, path_count);
    g_msg_type = MSG_TYPE_SUCCESS;

    const char *base = strrchr(g_clipboard_paths[0], '/');
    strlcpy(g_cursor.name, base ? base + 1 : g_clipboard_paths[0], sizeof g_cursor.name);
    return true;
  }
  return false;
}
