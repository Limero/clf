#pragma once

#include "color.c"
#include "global.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static pthread_t g_preview_thread;
static bool g_preview_thread_started = false;
static pthread_mutex_t g_preview_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_preview_cond = PTHREAD_COND_INITIALIZER;
static pid_t g_preview_child_pid = -1;
static unsigned int g_preview_generation = 0;
static bool g_preview_has_request = false;
static int g_preview_req_offset_x = 0;
static char g_preview_req_file_name[PATH_MAX];

static void preview_stop_previous(void) {
  pid_t pid = g_preview_child_pid;
  if (pid > 0) {
    g_preview_child_pid = -1;

    kill(pid, SIGTERM);
    for (int i = 0; i < 50; i++) {
      int status;
      pid_t r = waitpid(pid, &status, WNOHANG);
      if (r == pid) {
        return;
      }
      usleep(10000);
    }
    kill(pid, SIGKILL);
    int status;
    (void)waitpid(pid, &status, 0);
  }
}

static void render_ansi_line(const int preview_x, const int preview_width, const int preview_bottom, const char *buf,
                             const size_t len, int *y, int *lsp, bool *truncated, ansi_state_t *state) {
  const int y_before = *y;
  render_ansi_str(buf, (int)len, preview_x, y, preview_x + preview_width, true, preview_bottom, truncated, state);

  for (int line = y_before + 1; line <= *y && !*truncated; line++) {
    (*lsp)++;
    if (*lsp >= 3 || line == 2) {
      tb_present();
      *lsp = 0;
    }
  }

  if (!*truncated) {
    (*y)++;
    (*lsp)++;
    if (*lsp >= 3 || *y == 2) {
      tb_present();
      *lsp = 0;
    }
    if (*y >= preview_bottom) {
      *truncated = true;
    }
  } else {
    // render_ansi_str set *truncated when wrapping past bottom_y; the last
    // chunk was written to termbox2's buffer via tb_printf but never flushed
    tb_present();
    *lsp = 0;
  }
}

static void *preview_worker(void *arg) {
  (void)arg;
  unsigned int my_generation = 0;
  int current_offset_x = 0;
  char current_file[PATH_MAX];

  for (;;) {
    pthread_mutex_lock(&g_preview_mutex);
    while (!g_preview_has_request) {
      pthread_cond_wait(&g_preview_cond, &g_preview_mutex);
    }
    my_generation = g_preview_generation;
    current_offset_x = g_preview_req_offset_x;
    strlcpy(current_file, g_preview_req_file_name, sizeof(current_file));
    g_preview_has_request = false;
    pthread_mutex_unlock(&g_preview_mutex);

    preview_stop_previous();

    int pipefd[2];
    if (pipe(pipefd) == -1) {
      continue;
    }

    const pid_t pid = fork();
    if (pid == -1) {
      close(pipefd[0]);
      close(pipefd[1]);
      continue;
    }

    if (pid == 0) {
      dup2(pipefd[1], STDOUT_FILENO);
      dup2(pipefd[1], STDERR_FILENO);
      close(pipefd[0]);
      close(pipefd[1]);

      char width_str[16];
      char height_str[16];
      snprintf(width_str, sizeof(width_str), "%d", tb_width() - current_offset_x);
      snprintf(height_str, sizeof(height_str), "%d", tb_height());
      execlp(CMD_PREVIEW, CMD_PREVIEW, current_file, width_str, height_str, (char *)NULL);
      _exit(127);
    }

    close(pipefd[1]);
    g_preview_child_pid = pid;

    const int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    const int preview_x = current_offset_x + 3;
    const int preview_width = MAX(tb_width() - preview_x, 1);

    const size_t read_cap = (size_t)MAX(4096, preview_width * 4);
    char *read_buf = malloc(read_cap);
    if (!read_buf) {
      close(pipefd[0]);
      continue;
    }

    const size_t line_cap = (size_t)MAX((int)preview_width * 8 + 128, 4096);
    char *line_buf = malloc(line_cap);
    if (!line_buf) {
      free(read_buf);
      close(pipefd[0]);
      continue;
    }
    size_t line_len = 0;

    int y = 1;
    bool truncated = false;
    int lsp = 0;
    bool stale = false;
    ansi_state_t ansi_state;
    ansi_state_reset(&ansi_state);

    int preview_bottom = tb_height() - (g_msg_line_count ? g_msg_line_count : 1);

    for (;;) {
      pthread_mutex_lock(&g_preview_mutex);
      stale = (g_preview_generation != my_generation);
      pthread_mutex_unlock(&g_preview_mutex);

      if (stale) {
        preview_stop_previous();
        break;
      }

      preview_bottom = tb_height() - (g_msg_line_count ? g_msg_line_count : 1);

      bool eof = false;
      struct pollfd pfd = {.fd = pipefd[0], .events = POLLIN | POLLHUP};
      const int pr = poll(&pfd, 1, 50);
      if (pr > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
        for (;;) {
          ssize_t n = read(pipefd[0], read_buf, read_cap);
          if (n > 0) {
            for (ssize_t i = 0; i < n; i++) {
              unsigned char ch = (unsigned char)read_buf[i];

              if (ch == '\n') {
                line_buf[line_len] = '\0';
                pthread_mutex_lock(&g_tb_mutex);
                render_ansi_line(preview_x, preview_width, preview_bottom, line_buf, line_len, &y, &lsp, &truncated,
                                 &ansi_state);
                line_len = 0;
                pthread_mutex_unlock(&g_tb_mutex);
                if (truncated)
                  goto cleanup;
              } else {
                if (line_len >= line_cap - 1) {
                  line_buf[line_len] = '\0';
                  pthread_mutex_lock(&g_tb_mutex);
                  render_ansi_line(preview_x, preview_width, preview_bottom, line_buf, line_len, &y, &lsp, &truncated,
                                   &ansi_state);
                  line_len = 0;
                  pthread_mutex_unlock(&g_tb_mutex);
                  if (truncated)
                    goto cleanup;
                }
                line_buf[line_len++] = ch;
              }
            }
          } else if (n == 0) {
            eof = true;
            break;
          } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break;
            }
            eof = true;
            break;
          }
        }
      }

      int status;
      (void)waitpid(pid, &status, WNOHANG);
      if (eof) {
        break;
      }
    }

  cleanup:
    pthread_mutex_lock(&g_tb_mutex);
    if (!stale && !truncated && line_len > 0) {
      line_buf[line_len] = '\0';
      render_ansi_line(preview_x, preview_width, preview_bottom, line_buf, line_len, &y, &lsp, &truncated, &ansi_state);
      line_len = 0;
    }
    if (lsp > 0) {
      tb_present();
    }
    pthread_mutex_unlock(&g_tb_mutex);

    if (truncated) {
      preview_stop_previous();
    } else if (!stale) {
      int status;
      (void)waitpid(pid, &status, 0);
      g_preview_child_pid = -1;
    }
    close(pipefd[0]);
    free(read_buf);
    free(line_buf);
  }

  preview_stop_previous();
  return NULL;
}

void preview_start(const int offset_x, const char *file_name) {
  pthread_mutex_lock(&g_preview_mutex);
  if (!g_preview_thread_started) {
    if (pthread_create(&g_preview_thread, NULL, preview_worker, NULL) == 0) {
      pthread_detach(g_preview_thread);
      g_preview_thread_started = true;
    }
  }
  g_preview_req_offset_x = offset_x;
  strlcpy(g_preview_req_file_name, file_name, sizeof(g_preview_req_file_name));
  g_preview_has_request = true;
  g_preview_generation++;
  pthread_cond_signal(&g_preview_cond);
  pthread_mutex_unlock(&g_preview_mutex);
}
