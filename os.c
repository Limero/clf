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

int os_handle_child_exit(const char *func, const int childExitStatus) {
  if (WIFEXITED(childExitStatus)) {
    const int status = WEXITSTATUS(childExitStatus);
    if (status == 0) {
      return 0;
    }
    snprintf(g_msg, sizeof g_msg, "(%s) exited with code %d", func, status);
    g_msg_type = MSG_TYPE_ERROR;
    return -1;
  } else if (WIFSIGNALED(childExitStatus)) {
    return 0;
  } else if (WIFSTOPPED(childExitStatus)) {
    const int sig = WSTOPSIG(childExitStatus);
    snprintf(g_msg, sizeof g_msg, "(%s) stopped by signal %d (%s)", func, sig, strsignal(sig));
    g_msg_type = MSG_TYPE_ERROR;
    return -1;
  } else if (WIFCONTINUED(childExitStatus)) {
    return -1;
  } else {
    snprintf(g_msg, sizeof g_msg, "(%s) unknown status 0x%x", func, (unsigned)childExitStatus);
    g_msg_type = MSG_TYPE_ERROR;
    return -1;
  }
}

int os_copy(const char *source, const char *dest) {
  assert(source);
  assert(dest);

  pid_t pid = fork();
  if (pid == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s fork) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return -1;
  }

  if (pid == 0) {
    execlp(CMD_COPY, CMD_COPY, "-r", source, dest, (char *)0);
  } else {
    int childExitStatus;
    pid_t ws = waitpid(pid, &childExitStatus, WNOHANG);
    if (ws == -1) {
      snprintf(g_msg, sizeof g_msg, "(%s waitpid) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
      return -1;
    }

    return os_handle_child_exit("os_copy", childExitStatus);
  }

  assert(0 && "unreachable");
  return 0;
}

int os_move(const char *source, const char *dest) {
  assert(source);
  assert(dest);

  pid_t pid = fork();
  if (pid == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s fork) %s", __func__, strerror(errno));
    g_msg_type = MSG_TYPE_ERROR;
    return -1;
  }

  if (pid == 0) {
    execlp(CMD_MOVE, CMD_MOVE, source, dest, (char *)0);
  } else {
    int childExitStatus;
    pid_t ws = waitpid(pid, &childExitStatus, WNOHANG);
    if (ws == -1) {
      snprintf(g_msg, sizeof g_msg, "(%s waitpid) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
      return -1;
    }
    return os_handle_child_exit("os_move", childExitStatus);
  }

  assert(0 && "unreachable");
  return 0;
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
    g_current_selection.idx = 0;
  }
}
