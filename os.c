#pragma once

#include "global.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static bool os_run(const char *func, const char *file, char *const argv[]) {
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s pipe) %s", func, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return false;
  }

  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    snprintf(g_msg, sizeof g_msg, "(%s fork) %s", func, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return false;
  }

  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    execvp(file, argv);
    _exit(1);
  }

  close(pipefd[1]);

  int child_status;
  if (waitpid(pid, &child_status, 0) == -1) {
    close(pipefd[0]);
    snprintf(g_msg, sizeof g_msg, "(%s waitpid) %s", func, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return false;
  }

  if (WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0) {
    close(pipefd[0]);
    return true;
  }

  ssize_t n = read(pipefd[0], g_msg, sizeof(g_msg) - 1);
  close(pipefd[0]);
  if (n > 0) {
    g_msg[n] = '\0';
    size_t len = strlen(g_msg);
    while (len > 0 && (g_msg[len - 1] == '\n' || g_msg[len - 1] == '\r'))
      g_msg[--len] = '\0';
  } else if (n == 0) {
    snprintf(g_msg, sizeof g_msg, "(%s) exited with code %d", func,
             WIFEXITED(child_status) ? WEXITSTATUS(child_status) : -1);
  } else {
    snprintf(g_msg, sizeof g_msg, "(%s read) %s", func, strerror(errno));
  }
  g_msg_type = MSG_TYPE_ERROR;
  return false;
}

bool os_copy(const char *source, const char *dest) {
  assert(source);
  assert(dest);

  char *argv[] = {(char *)CMD_COPY, "-r", (char *)source, (char *)dest, NULL};
  return os_run("os_copy", CMD_COPY, argv);
}

bool os_move(const char *source, const char *dest) {
  assert(source);
  assert(dest);

  char *argv[] = {(char *)CMD_MOVE, (char *)source, (char *)dest, NULL};
  return os_run("os_move", CMD_MOVE, argv);
}

void os_exec(const char *c, const char *arg) {
  char cmd[256];

  if (snprintf(cmd, sizeof(cmd), "f='%s' %s %s", g_current_selection.name, c, arg) >= (int)sizeof(cmd)) {
    snprintf(g_msg, sizeof g_msg, "(%s snprintf) command too long %lu", __func__, sizeof(cmd));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }
  const int ret = system(cmd);
  if (ret == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s system) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }
}

void os_exec_output(const char *c, const char *arg) {
  char cmd[1024];

  const char *marker = "--CWD_MARKER--";
  const char *suffix = "; status=$?; printf '%s%s\\n' '--CWD_MARKER--' \"$PWD\"; exit $status";
  if (snprintf(cmd, sizeof(cmd), "f='%s' %s %s 2>&1%s", g_current_selection.name, c, arg, suffix) >= (int)sizeof(cmd)) {
    snprintf(g_msg, sizeof g_msg, "(%s snprintf) command too long %lu", __func__, sizeof(cmd));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  FILE *fp = popen(cmd, "r");
  if (!fp) {
    snprintf(g_msg, sizeof g_msg, "(%s popen) failed to run command", __func__);
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  const size_t msg_len = sizeof(g_msg);
  size_t total = 0;
  while (!feof(fp) && total + 1 < msg_len) {
    size_t n = fread(g_msg + total, 1, msg_len - total - 1, fp);
    if (n == 0) {
      if (ferror(fp)) {
        if (pclose(fp) == -1) {
          snprintf(g_msg, sizeof g_msg, "(%s pclose) %s", __func__, strerror(errno));
          g_msg_type = MSG_TYPE_ERROR;
        }
        return;
      }
      break;
    }
    total += n;
  }
  g_msg[total - 1] = '\0';

  int status = pclose(fp);
  if (status == 0) {
    g_msg_type = MSG_TYPE_INFO;
  } else {
    g_msg_type = MSG_TYPE_ERROR;
  }

  char prev_cwd[PATH_MAX];
  if (getcwd(prev_cwd, sizeof prev_cwd) == NULL) {
    prev_cwd[0] = '\0';
  }

  char *cwd_marker_start = NULL;
  for (char *p = g_msg; (p = strstr(p, marker)) != NULL; p += 1) {
    cwd_marker_start = p;
  }
  if (cwd_marker_start) {
    *cwd_marker_start = '\0';
    if (chdir(cwd_marker_start + strlen(marker)) == -1) {
      snprintf(g_msg, sizeof g_msg, "(%s chdir) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
    } else if (getcwd(g_cwd, sizeof g_cwd) == NULL) {
      snprintf(g_msg, sizeof g_msg, "(%s getcwd) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
    }
    if (strcmp(prev_cwd, g_cwd) != 0) {
      g_current_selection.idx = 0;
    }
  }
}
