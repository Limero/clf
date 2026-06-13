#pragma once

#include "global.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define COMPLETION_MAX 4096
#define COMPLETION_MAX_MATCHES 100
#define COMPLETION_NAME_LEN 1024

static char g_cache_cmds[COMPLETION_MAX][COMPLETION_NAME_LEN]; // 4MB
static int g_cache_cmd_count = 0;
static bool g_cache_ready = false;

static char g_matches[COMPLETION_MAX_MATCHES][COMPLETION_NAME_LEN]; // ~100KB
static int g_match_count = 0;
static int g_match_idx = 0;
static char g_last_prefix[COMPLETION_NAME_LEN] = "";
static bool g_completing = false;

int complete_match_count(void) {
  return g_match_count;
}

int complete_match_idx(void) {
  return g_match_idx;
}

bool complete_is_active(void) {
  return g_completing;
}

static int complete_cmp(const void *a, const void *b) {
  if (OPT_IGNORE_CASE)
    return strcasecmp((const char *)a, (const char *)b);
  return strcmp((const char *)a, (const char *)b);
}

static void escape_path(const char *src, char *dst, size_t dst_size) {
  while (*src && dst_size > 1) {
    if (*src == '\\' || *src == ' ') {
      if (dst_size < 3)
        break;
      *dst++ = '\\';
      *dst++ = *src;
      dst_size -= 2;
    } else {
      *dst++ = *src;
      dst_size--;
    }
    src++;
  }
  *dst = '\0';
}

static const char *complete_shell_name(void) {
  const char *shell = getenv("SHELL");
  if (!shell)
    return "sh";
  const char *name = strrchr(shell, '/');
  return name ? name + 1 : shell;
}

void complete_cache(void) {
  if (g_cache_ready)
    return;
  g_cache_cmd_count = 0;

  const char *path_env = getenv("PATH");
  if (path_env) {
    char path_copy[4096];
    strlcpy(path_copy, path_env, sizeof path_copy);
    char *dir = strtok(path_copy, ":");
    while (dir && g_cache_cmd_count < COMPLETION_MAX) {
      struct dirent **entries = NULL;
      int n = scandir(dir, &entries, NULL, alphasort);
      if (n > 0) {
        for (int i = 0; i < n; i++) {
          if (entries[i]->d_name[0] == '.') {
            free(entries[i]);
            continue;
          }
          char full[4096];
          int sn = snprintf(full, sizeof full, "%s/%s", dir, entries[i]->d_name);
          if (sn >= 0 && (size_t)sn < sizeof full && access(full, X_OK) == 0) {
            if (g_cache_cmd_count < COMPLETION_MAX) {
              strlcpy(g_cache_cmds[g_cache_cmd_count], entries[i]->d_name, COMPLETION_NAME_LEN);
              g_cache_cmd_count++;
            }
          }
          free(entries[i]);
        }
        free(entries);
      }
      dir = strtok(NULL, ":");
    }
  }

  if (OPT_FULL_SHELL) {
    const char *shell_name = complete_shell_name();
    const char *alias_cmd = NULL;
    if (strcmp(shell_name, "bash") == 0) {
      alias_cmd = "bash -i -c 'compgen -a' 2>/dev/null";
    } else if (strcmp(shell_name, "zsh") == 0) {
      alias_cmd = "zsh -i -c 'print -l ${(k)aliases}' 2>/dev/null";
    }
    if (alias_cmd) {
      FILE *fp = popen(alias_cmd, "r");
      if (fp) {
        char line[512];
        while (fgets(line, sizeof line, fp) && g_cache_cmd_count < COMPLETION_MAX) {
          size_t len = strlen(line);
          if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
          if (len == 0)
            continue;
          strlcpy(g_cache_cmds[g_cache_cmd_count], line, COMPLETION_NAME_LEN);
          g_cache_cmd_count++;
        }
        pclose(fp);
      }
    }
  }

  if (g_cache_cmd_count > 1) {
    qsort(g_cache_cmds, g_cache_cmd_count, COMPLETION_NAME_LEN, complete_cmp);
    int j = 0;
    for (int i = 0; i < g_cache_cmd_count; i++) {
      if (j == 0 || strcmp(g_cache_cmds[j - 1], g_cache_cmds[i]) != 0) {
        if (i != j)
          strcpy(g_cache_cmds[j], g_cache_cmds[i]);
        j++;
      }
    }
    g_cache_cmd_count = j;
  }

  g_cache_ready = true;
}

static bool is_escaped_space(const char *buf, const int space_pos) {
  int bs = 0;
  for (int i = space_pos - 1; i >= 0 && buf[i] == '\\'; i--)
    bs++;
  return bs % 2 == 1;
}

static void complete_get_word_bounds(const char *buf, const int cursor, int *start, int *end) {
  *start = cursor;
  while (*start > 0) {
    if (buf[*start - 1] == ' ' && !is_escaped_space(buf, *start - 1))
      break;
    (*start)--;
  }

  *end = cursor;
  while (buf[*end] != '\0') {
    if (buf[*end] == ' ' && !is_escaped_space(buf, *end))
      break;
    (*end)++;
  }
}

static void complete_word_at_cursor(const char *buf, const int cursor, char *word, const int word_size,
                                    bool *is_first) {
  int start, end;
  complete_get_word_bounds(buf, cursor, &start, &end);

  int len = end - start;
  if (len >= word_size)
    len = word_size - 1;
  if (len > 0)
    memcpy(word, buf + start, len);
  word[len] = '\0';

  *is_first = (start == 0);
}

static void complete_generate_commands(const char *prefix) {
  const int prefix_len = strlen(prefix);
  for (int i = 0; i < g_cache_cmd_count && g_match_count < COMPLETION_MAX_MATCHES; i++) {
    if (strncasecmp(g_cache_cmds[i], prefix, prefix_len) == 0) {
      strlcpy(g_matches[g_match_count], g_cache_cmds[i], COMPLETION_NAME_LEN);
      g_match_count++;
    }
  }
}

static bool entry_is_dir(const struct dirent *entry, const char *dir_part) {
  if (entry->d_type == DT_DIR)
    return true;
  if (entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN)
    return false;
  char full_path[PATH_MAX * 2];
  snprintf(full_path, sizeof full_path, "%s/%s", dir_part, entry->d_name);
  struct stat st;
  return stat(full_path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void complete_generate_paths(const char *prefix, const bool dirs_only) {
  char dir_buf[PATH_MAX];
  const char *dir_part;
  const char *file_part;

  bool use_tilde = false;

  char expanded_prefix[PATH_MAX];
  const char *scan_prefix = prefix;

  if (prefix[0] == '~' && (prefix[1] == '/' || prefix[1] == '\0')) {
    assert(g_homepath && g_homepath[0]);
    if (prefix[1] == '/')
      snprintf(expanded_prefix, sizeof expanded_prefix, "%s%s", g_homepath, prefix + 1);
    else
      snprintf(expanded_prefix, sizeof expanded_prefix, "%s/", g_homepath);
    scan_prefix = expanded_prefix;
    use_tilde = true;
  }

  const char *slash = strrchr(scan_prefix, '/');
  if (slash) {
    file_part = slash + 1;
    size_t dir_len = slash - scan_prefix;
    if (dir_len == 0) {
      dir_buf[0] = '/';
      dir_buf[1] = '\0';
    } else {
      memcpy(dir_buf, scan_prefix, dir_len);
      dir_buf[dir_len] = '\0';
    }
    dir_part = dir_buf;
  } else {
    file_part = scan_prefix;
    dir_part = ".";
  }

  struct dirent **entries = NULL;
  int n = scandir(dir_part, &entries, NULL, NULL);
  if (n < 0)
    return;

  const int file_part_len = strlen(file_part);
  const bool dir_is_dot = (dir_part[0] == '.' && dir_part[1] == '\0');

  for (int i = 0; i < n; i++) {
    const char *name = entries[i]->d_name;

    if (name[0] == '.' && file_part[0] != '.') {
      free(entries[i]);
      continue;
    }

    if (dirs_only && !entry_is_dir(entries[i], dir_part)) {
      free(entries[i]);
      continue;
    }

    if (strncasecmp(name, file_part, file_part_len) == 0 && g_match_count < COMPLETION_MAX_MATCHES) {
      if (use_tilde) {
        const char *rest = dir_part + g_homepath_len;
        char path_buf[PATH_MAX * 2];
        if (rest[0] == '\0')
          snprintf(path_buf, sizeof path_buf, "~/%s", name);
        else
          snprintf(path_buf, sizeof path_buf, "~%s/%s", rest, name);
        escape_path(path_buf, g_matches[g_match_count], COMPLETION_NAME_LEN);
      } else if (dir_is_dot) {
        escape_path(name, g_matches[g_match_count], COMPLETION_NAME_LEN);
      } else {
        char path_buf[PATH_MAX * 2];
        snprintf(path_buf, sizeof path_buf, "%s/%s", dir_part, name);
        escape_path(path_buf, g_matches[g_match_count], COMPLETION_NAME_LEN);
      }
      g_match_count++;
    }
    free(entries[i]);
  }
  free(entries);
}

static void complete_generate(const char *word, const bool is_first) {
  g_match_count = 0;

  if (is_first) {
    complete_generate_commands(word);
  } else {
    bool dirs_only = strncmp(g_current_command.chars, "cd ", 3) == 0 ||
                     strncmp(g_current_command.chars, "pushd ", 6) == 0 ||
                     strncmp(g_current_command.chars, "rmdir ", 6) == 0;
    complete_generate_paths(word, dirs_only);
  }

  if (g_match_count > 1) {
    qsort(g_matches, g_match_count, COMPLETION_NAME_LEN, complete_cmp);
  }

  g_match_idx = g_match_count > 0 ? 0 : -1;
  g_completing = g_match_count > 0;
}

static void complete_apply(const char *completion) {
  int start, end;
  complete_get_word_bounds(g_current_command.chars, g_current_command.cursor, &start, &end);

  const int new_len = strlen(completion);
  const int tail_len = strlen(g_current_command.chars) - end;

  const int new_total = start + new_len + tail_len;
  if (new_total >= (int)sizeof(g_current_command.chars))
    return;

  memmove(g_current_command.chars + start + new_len, g_current_command.chars + end, tail_len + 1);

  memcpy(g_current_command.chars + start, completion, new_len);

  g_current_command.len = new_total;
  g_current_command.cursor = start + new_len;
}

void complete_reset(void) {
  g_match_count = 0;
  g_match_idx = -1;
  g_last_prefix[0] = '\0';
  g_completing = false;
}

bool complete_handle_tab(void) {
  complete_cache();

  char word[COMPLETION_NAME_LEN];
  bool is_first;
  complete_word_at_cursor(g_current_command.chars, g_current_command.cursor, word, sizeof word, &is_first);

  if (g_completing && strcmp(word, g_last_prefix) == 0) {
    g_match_idx = (g_match_idx + 1) % g_match_count;
    complete_apply(g_matches[g_match_idx]);
    strlcpy(g_last_prefix, g_matches[g_match_idx], sizeof g_last_prefix);
    return true;
  }

  complete_generate(word, is_first);

  if (g_match_count == 0)
    return false;

  complete_apply(g_matches[0]);
  strlcpy(g_last_prefix, g_matches[0], sizeof g_last_prefix);
  g_match_idx = 0;
  return true;
}

bool complete_handle_shift_tab(void) {
  if (!g_completing || g_match_count == 0)
    return false;

  g_match_idx = (g_match_idx - 1 + g_match_count) % g_match_count;
  complete_apply(g_matches[g_match_idx]);
  strlcpy(g_last_prefix, g_matches[g_match_idx], sizeof g_last_prefix);
  return true;
}
