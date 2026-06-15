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

static bool flush_line(const int x, int *y, const int width, const char *buf, const size_t len, int *lsp) {
  if (*y >= tb_height() - MAX(1, g_msg_line_count) - 1)
    return true;
  tb_printf(x, *y, COLOR_DEFAULT, COLOR_DEFAULT, "%-*.*s", width, (int)len, buf);
  (*y)++;
  (*lsp)++;
  if (*lsp >= 3 || *y == 2) {
    tb_present();
    *lsp = 0;
  }
  return false;
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

    const size_t line_cap = (size_t)preview_width + 1;
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

    pthread_mutex_lock(&g_tb_mutex);
    const int preview_bottom = tb_height() - MAX(1, g_msg_line_count) - 1;
    for (int cy = 1; cy <= preview_bottom; cy++) {
      for (int cx = preview_x; cx < tb_width(); cx++) {
        tb_set_cell(cx, cy, ' ', 0, 0);
      }
    }
    pthread_mutex_unlock(&g_tb_mutex);

    for (;;) {
      pthread_mutex_lock(&g_preview_mutex);
      stale = (g_preview_generation != my_generation);
      pthread_mutex_unlock(&g_preview_mutex);

      if (stale) {
        preview_stop_previous();
        break;
      }

      bool eof = false;
      struct pollfd pfd = {.fd = pipefd[0], .events = POLLIN | POLLHUP};
      const int pr = poll(&pfd, 1, 50);
      if (pr > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
        for (;;) {
          ssize_t n = read(pipefd[0], read_buf, read_cap);
          if (n > 0) {
            for (ssize_t i = 0; i < n; i++) {
              char ch = read_buf[i];
              if (ch == '\n') {
                pthread_mutex_lock(&g_tb_mutex);
                if (line_len == 0) {
                  truncated = flush_line(preview_x, &y, preview_width, "", 0, &lsp);
                } else {
                  size_t start = 0;
                  while (start < line_len) {
                    size_t chunk = MIN((size_t)preview_width, line_len - start);
                    truncated = flush_line(preview_x, &y, preview_width, line_buf + start, chunk, &lsp);
                    if (truncated)
                      break;
                    start += chunk;
                  }
                }
                line_len = 0;
                pthread_mutex_unlock(&g_tb_mutex);
                if (truncated)
                  goto cleanup;
              } else if (ch == '\t') {
                size_t spaces = TAB_WIDTH - (line_len % TAB_WIDTH);
                while (spaces > 0 && line_len < (size_t)preview_width) {
                  line_buf[line_len++] = ' ';
                  spaces--;
                }
                if (line_len >= (size_t)preview_width) {
                  pthread_mutex_lock(&g_tb_mutex);
                  truncated = flush_line(preview_x, &y, preview_width, line_buf, preview_width, &lsp);
                  pthread_mutex_unlock(&g_tb_mutex);
                  line_len = 0;
                  if (truncated)
                    goto cleanup;
                }
              } else {
                if (line_len < (size_t)preview_width) {
                  line_buf[line_len++] = ch;
                } else {
                  pthread_mutex_lock(&g_tb_mutex);
                  truncated = flush_line(preview_x, &y, preview_width, line_buf, preview_width, &lsp);
                  pthread_mutex_unlock(&g_tb_mutex);
                  line_len = 0;
                  line_buf[line_len++] = ch;
                  if (truncated)
                    goto cleanup;
                }
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
      size_t start = 0;
      while (start < line_len) {
        size_t chunk = MIN((size_t)preview_width, line_len - start);
        truncated = flush_line(preview_x, &y, preview_width, line_buf + start, chunk, &lsp);
        if (truncated)
          break;
        start += chunk;
      }
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
