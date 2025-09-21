#pragma once

#include <dirent.h>
#define TB_IMPL
#include "include/termbox2.h"

typedef const uint16_t color_t;

color_t COLOR_DEFAULT = TB_DEFAULT;
color_t COLOR_REVERSE = TB_REVERSE; // using this in either fg/bg will switch bg and fg
color_t COLOR_UNDERLINE = TB_UNDERLINE;
color_t COLOR_LINENUMBER = TB_YELLOW;
color_t COLOR_PATH_USER_HOST = TB_GREEN | TB_BOLD;
color_t COLOR_PATH_DIR = TB_BLUE | TB_BOLD;
color_t COLOR_PATH_SELECTION = TB_WHITE | TB_BOLD;
color_t COLOR_ERROR_BG = TB_RED;
color_t COLOR_SUCCESS_FG = TB_GREEN;
color_t COLOR_STATUS_PERM = TB_CYAN;
color_t COLOR_FILE_DIR = TB_BLUE | TB_BOLD;
color_t COLOR_FILE_LINK = TB_CYAN;
color_t COLOR_FILE_NORMAL = TB_WHITE;

uint16_t color_file(const unsigned char d_type) {
  switch (d_type) {
  case DT_DIR:
    return COLOR_FILE_DIR;
  case DT_LNK:
    return COLOR_FILE_LINK;
  default:
    return COLOR_FILE_NORMAL;
  }
}
