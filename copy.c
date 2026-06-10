#pragma once

#include "os.c"
#include <assert.h>
#include <sys/file.h>

// Define so it can be overriden in tests to never touch main clipboard
#ifndef CLIPBOARD_FILE
#define CLIPBOARD_FILE "/tmp/clf-clipboard"
#endif

static const char *parse_clipboard(char *buf, bool *is_move) {
  assert(buf != NULL && is_move != NULL);
  char *nl = strchr(buf, '\n');
  if (!nl)
    return NULL;
  *nl = '\0';

  if (strcmp(buf, "move") == 0)
    *is_move = true;
  else if (strcmp(buf, "copy") == 0)
    *is_move = false;
  else
    return NULL;

  char *path = nl + 1;
  nl = strchr(path, '\n');
  if (nl)
    *nl = '\0';

  if (path[0] == '\0')
    return NULL;
  return path;
}

void copy_yank(const char *source_file_path, const bool delete_source_on_paste) {
  const int fd = open(CLIPBOARD_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s open) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  flock(fd, LOCK_EX);

  char line[PATH_MAX + 6];
  const int len = snprintf(line, sizeof line, "%s\n%s\n", delete_source_on_paste ? "move" : "copy", source_file_path);
  if (write(fd, line, len) == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s write) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
  } else {
    strlcpy(g_yanked_path, source_file_path, sizeof g_yanked_path);
    g_yanked_is_move = delete_source_on_paste;
  }

  close(fd);
}

void copy_init(void) {
  g_yanked_path[0] = '\0';
  const int fd = open(CLIPBOARD_FILE, O_RDONLY);
  if (fd == -1)
    return;

  flock(fd, LOCK_SH);

  char buf[PATH_MAX + 6];
  const ssize_t n = read(fd, buf, sizeof buf - 1);
  close(fd);

  if (n <= 0)
    return;
  buf[n] = '\0';

  const char *path = parse_clipboard(buf, &g_yanked_is_move);
  if (path) {
    strlcpy(g_yanked_path, path, sizeof g_yanked_path);
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

  char buf[PATH_MAX + 6];
  const ssize_t n = read(fd, buf, sizeof buf - 1);
  close(fd);

  if (n <= 0) {
    snprintf(g_msg, sizeof g_msg, "(%s read) %s", __func__, n == 0 ? "empty clipboard" : strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return false;
  }
  buf[n] = '\0';

  bool delete_source;
  const char *path = parse_clipboard(buf, &delete_source);
  if (!path) {
    snprintf(g_msg, sizeof g_msg, "(%s) invalid clipboard format", __func__);
    g_msg_type = MSG_TYPE_ERROR;
    return false;
  }

  if (delete_source) {
    if (os_move(path, target_dir_path)) {
      snprintf(g_msg, sizeof g_msg, "Moved successfully");
      g_msg_type = MSG_TYPE_SUCCESS;
      return true;
    }
  } else {
    if (os_copy(path, target_dir_path)) {
      snprintf(g_msg, sizeof g_msg, "Copied successfully");
      g_msg_type = MSG_TYPE_SUCCESS;
      return true;
    }
  }
  return false;
}
