#pragma once

#include "os.c"
#include <sys/file.h>

// Define so it can be overriden in tests to never touch main clipboard
#ifndef CLIPBOARD_FILE
#define CLIPBOARD_FILE "/tmp/clf-clipboard"
#endif

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
    snprintf(g_msg, sizeof g_msg, "Yanked %s", source_file_path);
    g_msg_type = MSG_TYPE_SUCCESS;
  }

  close(fd);
}

void copy_paste(const char *target_dir_path) {
  const int fd = open(CLIPBOARD_FILE, O_RDONLY);
  if (fd == -1) {
    if (errno == ENOENT) {
      snprintf(g_msg, sizeof g_msg, "No files in clipboard");
      g_msg_type = MSG_TYPE_ERROR;
    } else {
      snprintf(g_msg, sizeof g_msg, "(%s open) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
    }
    return;
  }

  flock(fd, LOCK_SH);

  char buf[PATH_MAX + 6];
  const ssize_t n = read(fd, buf, sizeof buf - 1);
  close(fd);

  if (n <= 0) {
    snprintf(g_msg, sizeof g_msg, "(%s read) %s", __func__, n == 0 ? "empty clipboard" : strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }
  buf[n] = '\0';

  char *line = buf;
  char *newline = strchr(line, '\n');
  if (!newline) {
    snprintf(g_msg, sizeof g_msg, "(%s) invalid clipboard format", __func__);
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }
  *newline = '\0';

  const bool delete_source = (strcmp(line, "move") == 0);

  line = newline + 1;
  newline = strchr(line, '\n');
  if (newline) {
    *newline = '\0';
  }

  if (line[0] == '\0') {
    snprintf(g_msg, sizeof g_msg, "(%s) empty clipboard", __func__);
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  if (delete_source) {
    if (os_move(line, target_dir_path) == 0) {
      snprintf(g_msg, sizeof g_msg, "Moved successfully");
      g_msg_type = MSG_TYPE_SUCCESS;
    }
  } else {
    if (os_copy(line, target_dir_path) == 0) {
      snprintf(g_msg, sizeof g_msg, "Copied successfully");
      g_msg_type = MSG_TYPE_SUCCESS;
    }
  }
}
