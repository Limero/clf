#pragma once

#include <assert.h>
#include <dirent.h>
#define TB_IMPL
#include "include/termbox2.h"

typedef const uint16_t color_t;

color_t COLOR_DEFAULT = TB_DEFAULT;
color_t COLOR_REVERSE = TB_REVERSE; // using this in either fg/bg will switch bg and fg
color_t COLOR_UNDERLINE = TB_UNDERLINE;
color_t COLOR_LINENUMBER = TB_YELLOW;
color_t COLOR_SELECT = TB_MAGENTA;
color_t COLOR_YANK_COPY = TB_YELLOW;
color_t COLOR_YANK_MOVE = TB_RED;
color_t COLOR_PATH_USER_HOST = TB_GREEN | TB_BOLD;
color_t COLOR_PATH_DIR = TB_BLUE | TB_BOLD;
color_t COLOR_PATH_CURSOR = TB_WHITE | TB_BOLD;
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

// --- ANSI SGR parser ---

typedef struct {
  uint16_t fg;
  uint16_t bg;
} ansi_state_t;

static inline void ansi_state_reset(ansi_state_t *state) {
  state->fg = TB_DEFAULT;
  state->bg = TB_DEFAULT;
}

// Mask to clear basic color (0x00-0x08) and TB_BRIGHT when setting a new foreground/background color
#define TB_COLOR_CLR 0x400fu

// Map ANSI color code to termbox2 attribute.
// code: 0-7 for basic colors (30-37), same 0-7 for bright colors (90-97, caller adds TB_BRIGHT).
static inline uint16_t ansi_idx_to_tb(const int idx) {
  static const uint16_t map[8] = {TB_BLACK, TB_RED, TB_GREEN, TB_YELLOW, TB_BLUE, TB_MAGENTA, TB_CYAN, TB_WHITE};
  return map[idx];
}

// Parse one CSI sequence starting at s (must point to '\033').
// If it is an SGR (Select Graphic Rendition) sequence (final byte 'm'),
// update state accordingly. Non-SGR CSI sequences are silently consumed.
// Returns pointer to the first byte after the entire CSI sequence.
static const char *ansi_parse_csi(const char *s, ansi_state_t *state) {
  ++s; // skip ESC
  if (*s != '[')
    return s;
  ++s; // skip '['

  int params[16];
  int nparams = 0;
  int current = -1;

  for (;;) {
    if (*s == '\0')
      return s;
    if (*s >= '0' && *s <= '9') {
      if (current == -1)
        current = 0;
      current = current * 10 + (*s - '0');
      ++s;
    } else if (*s == ';') {
      if (current != -1) {
        if (nparams < 16)
          params[nparams++] = current;
        current = -1;
      } else {
        // empty parameter means 0
        if (nparams < 16)
          params[nparams++] = 0;
      }
      ++s;
    } else if (*s >= '@' && *s <= '~') {
      // final byte
      if (current != -1 && nparams < 16)
        params[nparams++] = current;

      const char final = *s;
      if (final == 'm') {
        if (nparams == 0) {
          ansi_state_reset(state);
        }
        for (int i = 0; i < nparams; i++) {
          switch (params[i]) {
          case 0:
            ansi_state_reset(state);
            break;
          case 1:
            state->fg |= TB_BOLD;
            break;
          case 4:
            state->fg |= TB_UNDERLINE;
            break;
          case 7:
            state->fg |= TB_REVERSE;
            break;
          case 22:
            state->fg &= ~TB_BOLD;
            break;
          case 24:
            state->fg &= ~TB_UNDERLINE;
            break;
          case 27:
            state->fg &= ~TB_REVERSE;
            break;
          case 30:
          case 31:
          case 32:
          case 33:
          case 34:
          case 35:
          case 36:
          case 37:
            state->fg = (state->fg & ~TB_COLOR_CLR) | ansi_idx_to_tb(params[i] - 30);
            break;
          case 38:
            // extended fg: 38;5;n (256-color) or 38;2;r;g;b (truecolor)
            // We can't represent these in TB_OUTPUT_NORMAL, so just skip sub-params.
            if (i + 1 < nparams && params[i + 1] == 5) {
              i += 2; // skip ;5;n
            } else if (i + 1 < nparams && params[i + 1] == 2) {
              i += 4; // skip ;2;r;g;b
            }
            break;
          case 39:
            state->fg &= ~TB_COLOR_CLR;
            break;
          case 40:
          case 41:
          case 42:
          case 43:
          case 44:
          case 45:
          case 46:
          case 47:
            state->bg = (state->bg & ~TB_COLOR_CLR) | ansi_idx_to_tb(params[i] - 40);
            break;
          case 48:
            // extended bg
            if (i + 1 < nparams && params[i + 1] == 5) {
              i += 2;
            } else if (i + 1 < nparams && params[i + 1] == 2) {
              i += 4;
            }
            break;
          case 49:
            state->bg &= ~TB_COLOR_CLR;
            break;
          case 90:
          case 91:
          case 92:
          case 93:
          case 94:
          case 95:
          case 96:
          case 97:
            state->fg = (state->fg & ~TB_COLOR_CLR) | ansi_idx_to_tb(params[i] - 90) | TB_BRIGHT;
            break;
          case 100:
          case 101:
          case 102:
          case 103:
          case 104:
          case 105:
          case 106:
          case 107:
            state->bg = (state->bg & ~TB_COLOR_CLR) | ansi_idx_to_tb(params[i] - 100) | TB_BRIGHT;
            break;
          }
        }
      }

      return s + 1;
    } else {
      ++s;
    }
  }

  assert(!"unreachable");
  return s;
}

// Count number of UTF-8 code points in the first len bytes
static int utf8_cell_count(const char *s, const int len) {
  int count = 0;
  for (int i = 0; i < len; i++)
    if (((unsigned char)s[i] & 0xC0) != 0x80)
      count++;
  return count;
}

// Skips any ANSI escape sequence starting at p (must point to '\033').
// Updates ANSI state for SGR sequences via ansi_parse_csi.
// Handles CSI, OSC/DCS/SOS/PM/APC, and simple escape sequences.
// Returns pointer to first byte after the escape sequence.
static const char *ansi_skip_escape(const char *p, const char *end, ansi_state_t *state) {
  if (*p != '\033')
    return p;
  if (p + 1 >= end)
    return p + 1;
  const char c = *(p + 1);
  if (c == '[') {
    return ansi_parse_csi(p, state);
  }
  if (c == ']' || c == 'P' || c == 'X' || c == '^' || c == '_') {
    p += 2;
    while (p < end && *p != '\033' && *p != '\a')
      p++;
    if (p < end && *p == '\a')
      p++;
    else if (p < end && *p == '\033')
      p += 2;
    return p;
  }
  return p + 2;
}

// Expands a tab at position x to the next tab stop.
// Writes spaces with given fg/bg to termbox.
// Returns the new x position after expansion.
static inline int ansi_expand_tab(const int x, const int y, const int max_x, const int tab_width, const uint16_t fg,
                                  const uint16_t bg) {
  int stop = ((x / tab_width) + 1) * tab_width;
  if (stop > max_x)
    stop = max_x;
  int nx = x;
  for (; nx < stop; nx++)
    tb_set_cell(nx, y, ' ', fg, bg);
  return nx;
}

// Finds the end of the next text segment (stops at '\033').
// Returns pointer to the found byte, or end if neither was found.
static inline const char *ansi_find_seg_end(const char *p, const char *end) {
  while (p < end && *p != '\033' && *p != '\t')
    p++;
  return p;
}
