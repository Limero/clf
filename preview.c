#pragma once

/*
Disclaimer:
  Most of the content in this file is "vibe coded" and probably overly complex
  The initial manually written version was blocking, so it was terrible when previewing large files
  Eventually this should be re-written and simplified
  Here be dragons
*/

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
static volatile pid_t g_preview_child_pid = -1;
static unsigned int g_preview_generation = 0;
static bool g_preview_has_request = false;
static int g_preview_req_offset_x = 0;
static char g_preview_req_file_name[PATH_MAX];

static void preview_stop_previous(void) {
  pid_t pid = g_preview_child_pid;
  if (pid > 0) {
    // Atomically clear the PID to prevent double-kill
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

    pid_t pid = fork();
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

    int flags = fcntl(pipefd[0], F_GETFL, 0);
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
    int lines_since_present = 0;

    // Clear preview area once at the start (without immediate present)
    pthread_mutex_lock(&g_tb_mutex);
    for (int cy = 1; cy < tb_height() - 1; cy++) {
      for (int cx = preview_x; cx < tb_width(); cx++) {
        tb_set_cell(cx, cy, ' ', 0, 0);
      }
    }
    pthread_mutex_unlock(&g_tb_mutex);

    for (;;) {
      // Check generation before any processing
      pthread_mutex_lock(&g_preview_mutex);
      bool should_continue = (g_preview_generation == my_generation);
      pthread_mutex_unlock(&g_preview_mutex);

      if (!should_continue) {
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
              // Check generation more frequently to abort quickly
              pthread_mutex_lock(&g_preview_mutex);
              bool should_abort = (g_preview_generation != my_generation);
              pthread_mutex_unlock(&g_preview_mutex);

              if (should_abort) {
                preview_stop_previous();
                goto cleanup_and_exit;
              }

              char ch = read_buf[i];
              if (ch == '\n') {
                // Print the accumulated line with soft wrapping to preview_width
                pthread_mutex_lock(&g_tb_mutex);
                if (line_len == 0) {
                  // Preserve empty line: print a blank padded row
                  tb_printf(preview_x, y, COLOR_DEFAULT, COLOR_DEFAULT, "%-*s", preview_width, "");
                  if (y == tb_height() - 2) {
                    truncated = true;
                    tb_present();
                    pthread_mutex_unlock(&g_tb_mutex);
                    goto cleanup_and_exit;
                  }
                  y++;
                  lines_since_present++;
                  if (lines_since_present >= 3 || y == 2) {
                    tb_present();
                    lines_since_present = 0;
                  }
                } else {
                  size_t start = 0;
                  while (start < line_len) {
                    size_t chunk = MIN((size_t)preview_width, line_len - start);
                    tb_printf(preview_x, y, COLOR_DEFAULT, COLOR_DEFAULT, "%-*.*s", preview_width, (int)chunk,
                              line_buf + start);
                    if (y == tb_height() - 2) {
                      truncated = true;
                      tb_present();
                      pthread_mutex_unlock(&g_tb_mutex);
                      goto cleanup_and_exit;
                    }
                    y++;
                    lines_since_present++;
                    if (lines_since_present >= 3 || y == 2) {
                      tb_present();
                      lines_since_present = 0;
                    }
                    start += chunk;
                  }
                }
                // Reset line buffer after printing newline-terminated line
                line_len = 0;
                pthread_mutex_unlock(&g_tb_mutex);
              } else {
                // Append character and soft-wrap on the fly when reaching width
                if (line_len < (size_t)preview_width) {
                  line_buf[line_len++] = ch;
                } else {
                  // Flush current full-width line chunk
                  pthread_mutex_lock(&g_tb_mutex);
                  tb_printf(preview_x, y, COLOR_DEFAULT, COLOR_DEFAULT, "%-*.*s", preview_width, preview_width,
                            line_buf);
                  if (y == tb_height() - 2) {
                    truncated = true;
                    tb_present();
                    pthread_mutex_unlock(&g_tb_mutex);
                    goto cleanup_and_exit;
                  }
                  y++;
                  lines_since_present++;
                  if (lines_since_present >= 3 || y == 2) {
                    tb_present();
                    lines_since_present = 0;
                  }
                  pthread_mutex_unlock(&g_tb_mutex);

                  // Start new line buffer with current char
                  line_len = 0;
                  line_buf[line_len++] = ch;
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

  cleanup_and_exit:
    // Present any remaining buffered content
    if (lines_since_present > 0) {
      pthread_mutex_lock(&g_tb_mutex);
      tb_present();
      pthread_mutex_unlock(&g_tb_mutex);
    }

    if (!truncated && line_len > 0) {
      size_t start = 0;
      pthread_mutex_lock(&g_tb_mutex);
      while (start < line_len) {
        size_t chunk = MIN((size_t)preview_width, line_len - start);
        tb_printf(preview_x, y, COLOR_DEFAULT, COLOR_DEFAULT, "%-*.*s", preview_width, (int)chunk, line_buf + start);
        if (y == tb_height() - 2) {
          truncated = true;
          tb_present();
          pthread_mutex_unlock(&g_tb_mutex);
          break;
        }
        y++;
      }
      if (!truncated) {
        tb_present();
        pthread_mutex_unlock(&g_tb_mutex);
      }
    }
    if (truncated) {
      // Stop child to avoid blocking on a full pipe
      preview_stop_previous();
    } else {
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
