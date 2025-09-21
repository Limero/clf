#pragma once

#include "os.c"

static const char *NAMED_PIPE = "/tmp/clf-pipe";

typedef struct {
  char source_file_path[PATH_MAX];
  bool delete_source_on_paste;
} message_t;

void copy_yank(const char *source_file_path, const bool delete_source_on_paste) {
  int ret = mkfifo(NAMED_PIPE, 0666);
  if (ret == -1) {
    switch (errno) {
    case EEXIST:
      // TODO: drain any other pending copies
      break;
    default:
      snprintf(g_msg, sizeof g_msg, "(%s mkfifo) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
      return;
    }
  }

  const pid_t p = fork();
  if (p == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s fork) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  if (p == 0) {
    const int fd = open(NAMED_PIPE, O_WRONLY);
    if (fd == -1) {
      snprintf(g_msg, sizeof g_msg, "(%s open) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
      return;
    }

    message_t msg;
    strlcpy(msg.source_file_path, source_file_path, PATH_MAX);
    msg.delete_source_on_paste = delete_source_on_paste;
    ret = write(fd, &msg, sizeof(msg));
    if (ret == -1) {
      snprintf(g_msg, sizeof g_msg, "(%s write) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
    }

    ret = close(fd);
    if (ret == -1) {
      snprintf(g_msg, sizeof g_msg, "(%s close) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
    }
    _exit(0);
  }
}

void copy_paste(const char *target_dir_path) {
  int ret = mkfifo(NAMED_PIPE, 0666);
  if (ret == -1) {
    switch (errno) {
    case EEXIST:
      break;
    default:
      snprintf(g_msg, sizeof g_msg, "(%s mkfifo) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
      return;
    }
  }

  const int fd = open(NAMED_PIPE, O_RDONLY);
  if (fd == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s open) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  message_t msg;
  ret = read(fd, &msg, sizeof(msg));
  if (ret == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s read) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  ret = close(fd);
  if (ret == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s close) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  if (msg.delete_source_on_paste) {
    if (os_move(msg.source_file_path, target_dir_path) == 0) {
      snprintf(g_msg, sizeof g_msg, "Moved successfully");
      g_msg_type = MSG_TYPE_SUCCESS;
    }
  } else {
    if (os_copy(msg.source_file_path, target_dir_path) == 0) {
      snprintf(g_msg, sizeof g_msg, "Copied successfully");
      g_msg_type = MSG_TYPE_SUCCESS;
    }
  }

  ret = unlink(NAMED_PIPE);
  if (ret == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s unlink) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
  }
}
