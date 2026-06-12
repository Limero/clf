#pragma once

#include "global.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static void shell_quote(const char *s, char *buf, const size_t buf_size) {
  if (buf_size == 0)
    return;
  char *p = buf;
  const char *end = buf + buf_size - 1;
  if (p < end)
    *p++ = '\'';
  while (*s && p < end) {
    if (*s == '\'') {
      if (p + 4 > end)
        break;
      *p++ = '\'';
      *p++ = '\\';
      *p++ = '\'';
      *p++ = '\'';
    } else {
      *p++ = *s;
    }
    s++;
  }
  if (p < end)
    *p++ = '\'';
  *p = '\0';
}

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

static void strip_ansi(char *s) {
  char *src = s, *dst = s;
  while (*src) {
    if (*src == '\033' && *(src + 1) == '[') {
      src += 2;
      while (*src && !(*src >= '@' && *src <= '~'))
        src++;
      if (*src)
        src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}

static void popen_cleanup(const int *stdinpipe, const int *stdoutpipe, const bool has_term,
                          const struct termios *saved_tios, const struct sigaction *sa_old) {
  if (stdinpipe[0] != -1) {
    close(stdinpipe[0]);
    close(stdinpipe[1]);
  }
  if (stdoutpipe[0] != -1) {
    close(stdoutpipe[0]);
    close(stdoutpipe[1]);
  }
  if (has_term)
    tcsetattr(STDIN_FILENO, TCSANOW, saved_tios);
  sigaction(SIGINT, &sa_old[0], NULL);
  sigaction(SIGQUIT, &sa_old[1], NULL);
  sigaction(SIGPIPE, &sa_old[2], NULL);
}

static bool build_cmd(char *cmd, size_t cmd_size, const char *c, const char *arg, const char *suffix) {
  char name_quoted[sizeof g_cursor.name * 4 + 2];
  char arg_quoted[sizeof g_cursor.name * 4 + 2] = "";

  shell_quote(g_cursor.name, name_quoted, sizeof name_quoted);
  if (arg[0] != '\0')
    shell_quote(arg, arg_quoted, sizeof arg_quoted);

  int n = snprintf(cmd, cmd_size, "f=%s %s %s%s", name_quoted, c, arg_quoted, suffix);
  return n >= 0 && (size_t)n < cmd_size;
}

static int os_popen_full_shell(const char *cmd, char *buf, size_t buf_size) {
  const char *shell = getenv("SHELL");
  if (!shell)
    shell = "sh";

  const char *name = strrchr(shell, '/');
  name = name ? name + 1 : shell;
  const char *const source_cmd_bash = "shopt -s expand_aliases 2>/dev/null; "
                                      ". \"$HOME/.bashrc\" >/dev/null 2>&1; "
                                      ". /dev/stdin";
  const char *const source_cmd_zsh = ". \"$HOME/.zshrc\" >/dev/null 2>&1; "
                                     ". /dev/stdin";
  const char *const source_cmd_default = ". /dev/stdin";

  const char *source_cmd;
  if (strcmp(name, "zsh") == 0) {
    source_cmd = source_cmd_zsh;
  } else if (strcmp(name, "bash") == 0) {
    source_cmd = source_cmd_bash;
  } else if (strcmp(name, "fish") == 0) {
    // TODO: Support fish directly instead of falling back on bash
    shell = "/bin/bash";
    source_cmd = source_cmd_bash;
  } else {
    source_cmd = source_cmd_default;
  }

  struct termios saved_tios;
  bool has_term = isatty(STDIN_FILENO) == 1 && tcgetattr(STDIN_FILENO, &saved_tios) == 0;
  if (has_term) {
    struct termios tios = saved_tios;
    tios.c_lflag |= ISIG;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tios);
  }

  struct sigaction sa_old[3];
  struct sigaction sa_ignore;
  memset(&sa_ignore, 0, sizeof sa_ignore);
  sa_ignore.sa_handler = SIG_IGN;
  sigemptyset(&sa_ignore.sa_mask);
  sigaction(SIGINT, &sa_ignore, &sa_old[0]);
  sigaction(SIGQUIT, &sa_ignore, &sa_old[1]);
  sigaction(SIGPIPE, &sa_ignore, &sa_old[2]);

  int stdinpipe[2] = {-1, -1};
  int stdoutpipe[2] = {-1, -1};
  if (pipe(stdinpipe) == -1 || pipe(stdoutpipe) == -1) {
    popen_cleanup(stdinpipe, stdoutpipe, has_term, &saved_tios, sa_old);
    return -1;
  }

  pid_t pid = fork();
  if (pid == -1) {
    popen_cleanup(stdinpipe, stdoutpipe, has_term, &saved_tios, sa_old);
    return -1;
  }

  if (pid == 0) {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
    close(stdinpipe[1]);
    dup2(stdinpipe[0], STDIN_FILENO);
    close(stdinpipe[0]);
    close(stdoutpipe[0]);
    dup2(stdoutpipe[1], STDOUT_FILENO);
    close(stdoutpipe[1]);
    execlp(shell, shell, "-c", source_cmd, (char *)NULL);
    _exit(127);
  }

  close(stdinpipe[0]);
  close(stdoutpipe[1]);

  static const char hook_suppress[] = "PROMPT_COMMAND=; precmd() { :; }; preexec() { :; }; chpwd() { :; }; ";
  write(stdinpipe[1], hook_suppress, sizeof hook_suppress - 1);
  write(stdinpipe[1], cmd, strlen(cmd));
  close(stdinpipe[1]);

  size_t total = 0;
  ssize_t n;
  while (total + 1 < buf_size && (n = read(stdoutpipe[0], buf + total, buf_size - total - 1)) > 0)
    total += n;
  if (total > 0 && buf[total - 1] == '\n')
    total--;
  buf[total] = '\0';
  close(stdoutpipe[0]);

  int child_status;
  pid_t ret;
  do {
    ret = waitpid(pid, &child_status, 0);
  } while (ret == -1 && errno == EINTR);

  if (has_term) {
    tcflush(STDIN_FILENO, TCIFLUSH);
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_tios);
  }
  sigaction(SIGINT, &sa_old[0], NULL);
  sigaction(SIGQUIT, &sa_old[1], NULL);
  sigaction(SIGPIPE, &sa_old[2], NULL);

  if (ret == -1)
    return -1;
  if (WIFEXITED(child_status))
    return WEXITSTATUS(child_status);
  return -1;
}

void os_exec(const char *c, const char *arg) {
  char cmd[4096];
  if (!build_cmd(cmd, sizeof cmd, c, arg, "")) {
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
  char cmd[4096];

  const char *marker = "--CWD_MARKER--";
  const char *suffix = " 2>&1; _clf_s=$?; printf '%s%s\\n' '--CWD_MARKER--' \"$PWD\"; exit $_clf_s";

  if (!build_cmd(cmd, sizeof cmd, c, arg, suffix)) {
    snprintf(g_msg, sizeof g_msg, "(%s snprintf) command too long %lu", __func__, sizeof(cmd));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  int status;
  if (OPT_FULL_SHELL) {
    status = os_popen_full_shell(cmd, g_msg, sizeof g_msg);
    if (status == -1) {
      snprintf(g_msg, sizeof g_msg, "(%s) failed to run command", __func__);
      g_msg_type = MSG_TYPE_ERROR;
      return;
    }
    strip_ansi(g_msg);
  } else {
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
    if (total > 0 && g_msg[total - 1] == '\n')
      total--;
    g_msg[total] = '\0';

    status = pclose(fp);
  }

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
      g_cursor.idx = 0;
    }
  }
}
