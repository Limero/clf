#pragma once

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "draw.c"
#include "global.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static volatile sig_atomic_t sig_child_pid;

static void sigint_handler(int sig) {
  (void)sig;
  if (sig_child_pid > 0)
    kill(-sig_child_pid, SIGKILL);
}

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

  const pid_t pid = fork();
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

  const ssize_t n = read(pipefd[0], g_msg, sizeof(g_msg) - 1);
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

static int pty_open(void) {
  int fd = posix_openpt(O_RDWR | O_NOCTTY);
  if (fd == -1)
    return -1;
  if (grantpt(fd) == -1) {
    close(fd);
    return -1;
  }
  if (unlockpt(fd) == -1) {
    close(fd);
    return -1;
  }
  // Set raw mode to avoid \n -> \r\n conversion, echo, etc.
  struct termios tio;
  if (tcgetattr(fd, &tio) == 0) {
    cfmakeraw(&tio);
    tcsetattr(fd, TCSANOW, &tio);
  }
  return fd;
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

// Returns true if buf contains escape sequences typical of interactive TUI programs (e.g., vim).
static bool is_tui_output(const char *buf) {
  return strstr(buf, "\033[?1049h") || strstr(buf, "\033[?1047h") || strstr(buf, "\033[?47h") ||
         strstr(buf, "\033[6n") || strstr(buf, "\033[c");
}

static bool build_cmd(char *cmd, const size_t cmd_size, const char *c, const char *arg, const char *suffix) {
  char name_quoted[sizeof g_cursor.name * 4 + 2];
  char arg_quoted[sizeof g_cursor.name * 4 + 2] = "";

  shell_quote(g_cursor.name, name_quoted, sizeof name_quoted);
  if (arg[0] != '\0')
    shell_quote(arg, arg_quoted, sizeof arg_quoted);

  const int n = snprintf(cmd, cmd_size, "f=%s %s %s%s", name_quoted, c, arg_quoted, suffix);
  return n >= 0 && (size_t)n < cmd_size;
}

static int os_popen_full_shell(const char *cmd, char *buf, const size_t buf_size, void (*indicator_cb)(void),
                               const int threshold_ms) {
  const char *const source_cmd_default = ". /dev/stdin";

  const char *shell;
  const char *source_cmd;
  if (OPT_FULL_SHELL) {
    shell = getenv("SHELL");
    if (!shell)
      shell = "sh";

    const char *name = strrchr(shell, '/');
    name = name ? name + 1 : shell;
    const char *const source_cmd_bash = "shopt -s expand_aliases 2>/dev/null; "
                                        ". \"$HOME/.bashrc\" >/dev/null 2>&1; "
                                        ". /dev/stdin";
    const char *const source_cmd_zsh = ". \"$HOME/.zshrc\" >/dev/null 2>&1; "
                                       ". /dev/stdin";

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
  } else {
    shell = "sh";
    source_cmd = source_cmd_default;
  }

  struct termios saved_tios;
  const bool has_term = isatty(STDIN_FILENO) == 1 && tcgetattr(STDIN_FILENO, &saved_tios) == 0;
  if (has_term) {
    struct termios tios = saved_tios;
    tios.c_lflag |= ISIG;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tios);
  }

  struct sigaction sa_old[3];
  struct sigaction sa_new;
  memset(&sa_new, 0, sizeof sa_new);
  sigemptyset(&sa_new.sa_mask);
  sa_new.sa_handler = sigint_handler;
  sigaction(SIGINT, &sa_new, &sa_old[0]);
  sa_new.sa_handler = SIG_IGN;
  sigaction(SIGQUIT, &sa_new, &sa_old[1]);
  sigaction(SIGPIPE, &sa_new, &sa_old[2]);

  int stdinpipe[2] = {-1, -1};
  int stdoutpipe[2] = {-1, -1};
  int ptm_fd = -1;

  ptm_fd = pty_open();
  if (ptm_fd == -1 && pipe(stdoutpipe) == -1) {
    popen_cleanup(stdinpipe, stdoutpipe, has_term, &saved_tios, sa_old);
    return -1;
  }

  if (pipe(stdinpipe) == -1) {
    if (ptm_fd != -1)
      close(ptm_fd);
    popen_cleanup(stdinpipe, stdoutpipe, has_term, &saved_tios, sa_old);
    return -1;
  }

  const pid_t pid = fork();
  if (pid > 0)
    sig_child_pid = pid;
  if (pid == -1) {
    if (ptm_fd != -1)
      close(ptm_fd);
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
    if (ptm_fd != -1) {
      const char *pts_name = ptsname(ptm_fd);
      if (!pts_name)
        _exit(127);
      int pts_fd = open(pts_name, O_RDWR | O_NOCTTY);
      if (pts_fd == -1)
        _exit(127);
      close(ptm_fd);
      close(stdoutpipe[0]);
      close(stdoutpipe[1]);
      setsid();
      ioctl(pts_fd, TIOCSCTTY, 0);
      dup2(pts_fd, STDOUT_FILENO);
      dup2(pts_fd, STDERR_FILENO);
      close(pts_fd);
    } else {
      setpgid(0, 0);
      close(stdoutpipe[0]);
      dup2(stdoutpipe[1], STDOUT_FILENO);
      close(stdoutpipe[1]);
    }
    execlp(shell, shell, "-c", source_cmd, (char *)NULL);
    _exit(127);
  }

  close(stdinpipe[0]);
  if (ptm_fd != -1) {
    close(stdoutpipe[0]);
    close(stdoutpipe[1]);
  } else {
    close(stdoutpipe[1]);
  }

  static const char hook_suppress[] = "PROMPT_COMMAND=; precmd() { :; }; preexec() { :; }; chpwd() { :; }; "
                                      "export PAGER=cat GIT_PAGER=cat SYSTEMD_PAGER=cat; ";
  write(stdinpipe[1], hook_suppress, sizeof hook_suppress - 1);
  write(stdinpipe[1], cmd, strlen(cmd));
  close(stdinpipe[1]);

  const int read_fd = ptm_fd != -1 ? ptm_fd : stdoutpipe[0];
  size_t total = 0;
  ssize_t n;
  bool indicator_shown = false;
  bool streaming = false;
  bool screen_full = false;
  struct pollfd pfd = {.fd = read_fd, .events = POLLIN};

  while (total + 1 < buf_size) {
    int timeout = (indicator_shown || indicator_cb == NULL) ? -1 : threshold_ms;
    int pret = poll(&pfd, 1, timeout);

    if (pret > 0) {
      n = read(read_fd, buf + total, buf_size - total - 1);
      if (n == -1 && errno == EIO)
        break;
      if (n <= 0)
        break;
      ssize_t wp = total;
      for (ssize_t i = 0; i < n; i++)
        if (buf[total + i] != '\r')
          buf[wp++] = buf[total + i];
      total = wp;
      buf[total] = '\0';

      if (is_tui_output(buf))
        screen_full = true;

      if (!streaming && total > 0)
        streaming = true;

      if (streaming && !screen_full)
        screen_full = draw_streaming_msg();
    } else if (pret == 0) {
      indicator_cb();
      indicator_shown = true;
    } else if (errno == EINTR) {
      break;
    } else {
      break;
    }
  }

  sig_child_pid = 0;

  {
    char drain[4096];
    while ((n = read(read_fd, drain, sizeof(drain))) > 0)
      ;
  }

  if (total > 0 && buf[total - 1] == '\n')
    total--;
  buf[total] = '\0';
  close(read_fd);

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

void os_exec_output_deferred(const char *c, const char *arg, void (*indicator_cb)(void), const int threshold_ms) {
  char cmd[4096];

  const char *marker = "--CWD_MARKER--";
  const char *suffix = " 2>&1; _clf_s=$?; printf '%s%s\\n' '--CWD_MARKER--' \"$PWD\"; exit $_clf_s";

  if (!build_cmd(cmd, sizeof cmd, c, arg, suffix)) {
    snprintf(g_msg, sizeof g_msg, "(%s snprintf) command too long %lu", __func__, sizeof(cmd));
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

  g_msg_type = MSG_TYPE_INFO;
  const int status = os_popen_full_shell(cmd, g_msg, sizeof g_msg, indicator_cb, threshold_ms);
  if (status == -1) {
    snprintf(g_msg, sizeof g_msg, "(%s) failed to run command", __func__);
    g_msg_type = MSG_TYPE_ERROR;
    return;
  }

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
    const char *new_cwd = cwd_marker_start + strlen(marker);
    strlcpy(g_prev_cwd, g_cwd, sizeof g_prev_cwd);
    if (chdir(new_cwd) == -1) {
      snprintf(g_msg, sizeof g_msg, "(%s chdir) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
    } else if (getcwd(g_cwd, sizeof g_cwd) == NULL) {
      snprintf(g_msg, sizeof g_msg, "(%s getcwd) %s", __func__, strerror(errno));
      g_msg_type = MSG_TYPE_ERROR;
    }
    if (strcmp(g_prev_cwd, g_cwd) != 0) {
      g_cursor.idx = 0;
    }
  }
}

void os_exec_output(const char *c, const char *arg) {
  os_exec_output_deferred(c, arg, NULL, 0);
}
