#pragma once

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#include "config.h"
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

typedef enum {
  MODE_NORMAL,
  MODE_COMMAND,
} modes_t;

typedef enum {
  MSG_TYPE_INFO,
  MSG_TYPE_SUCCESS,
  MSG_TYPE_ERROR,
} msg_type_t;

static int g_left_column_idx = 0;
static int g_right_column_idx = 0;
static char g_msg[4096] = "";
static msg_type_t g_msg_type;

static char *g_username;
static int g_username_len;
static char *g_hostname;
static int g_hostname_len;
static char *g_homepath;
static int g_homepath_len;

static bool g_show_hidden = false;

static struct {
  char name[256];
  int idx;
} g_current_selection;

static modes_t g_current_mode = MODE_NORMAL;

static char g_cwd[PATH_MAX] = ".";

static int g_items_in_left_dir = 0;
static int g_items_in_middle_dir = 0;
static int g_items_in_right_dir = 0;
static int g_search_idx_before = -1;

static struct {
  bool dir_left;
  bool dir_middle;
  bool dir_right;
} g_update = {true, true, true};

static struct dirent **g_namelist_left = NULL;
static struct dirent **g_namelist_middle = NULL;
static struct dirent **g_namelist_right = NULL;
static pthread_mutex_t g_tb_mutex = PTHREAD_MUTEX_INITIALIZER;
