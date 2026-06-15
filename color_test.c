#include "color.c"
#include "test.c"

static void test_ansi_basic_fg(void) {
  ansi_state_t s;
  const char *p;

  ansi_state_reset(&s);
  p = ansi_parse_csi("\033[30m", &s);
  assert_int(s.fg, ==, TB_BLACK);
  assert_int(s.bg, ==, TB_DEFAULT);
  assert_string_equal(p, "");

  ansi_state_reset(&s);
  ansi_parse_csi("\033[31m", &s);
  assert_int(s.fg, ==, TB_RED);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[32m", &s);
  assert_int(s.fg, ==, TB_GREEN);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[33m", &s);
  assert_int(s.fg, ==, TB_YELLOW);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[34m", &s);
  assert_int(s.fg, ==, TB_BLUE);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[35m", &s);
  assert_int(s.fg, ==, TB_MAGENTA);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[36m", &s);
  assert_int(s.fg, ==, TB_CYAN);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[37m", &s);
  assert_int(s.fg, ==, TB_WHITE);
}

static void test_ansi_bright_fg(void) {
  ansi_state_t s;

  ansi_state_reset(&s);
  ansi_parse_csi("\033[90m", &s);
  assert_int(s.fg, ==, TB_BLACK | TB_BRIGHT);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[91m", &s);
  assert_int(s.fg, ==, TB_RED | TB_BRIGHT);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[97m", &s);
  assert_int(s.fg, ==, TB_WHITE | TB_BRIGHT);
}

static void test_ansi_basic_bg(void) {
  ansi_state_t s;

  ansi_state_reset(&s);
  ansi_parse_csi("\033[40m", &s);
  assert_int(s.fg, ==, TB_DEFAULT);
  assert_int(s.bg, ==, TB_BLACK);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[41m", &s);
  assert_int(s.bg, ==, TB_RED);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[47m", &s);
  assert_int(s.bg, ==, TB_WHITE);
}

static void test_ansi_bright_bg(void) {
  ansi_state_t s;

  ansi_state_reset(&s);
  ansi_parse_csi("\033[100m", &s);
  assert_int(s.bg, ==, TB_BLACK | TB_BRIGHT);

  ansi_state_reset(&s);
  ansi_parse_csi("\033[101m", &s);
  assert_int(s.bg, ==, TB_RED | TB_BRIGHT);
}

static void test_ansi_reset(void) {
  ansi_state_t s;

  // Reset with 0
  s.fg = TB_RED;
  s.bg = TB_GREEN;
  ansi_parse_csi("\033[0m", &s);
  assert_int(s.fg, ==, TB_DEFAULT);
  assert_int(s.bg, ==, TB_DEFAULT);

  // Empty SGR (ESC[m) also resets
  s.fg = TB_BLUE;
  s.bg = TB_YELLOW;
  ansi_parse_csi("\033[m", &s);
  assert_int(s.fg, ==, TB_DEFAULT);
  assert_int(s.bg, ==, TB_DEFAULT);
}

static void test_ansi_bold(void) {
  ansi_state_t s;

  ansi_state_reset(&s);
  ansi_parse_csi("\033[1m", &s);
  assert_int(s.fg & TB_BOLD, ==, TB_BOLD);

  // Bold + color
  ansi_state_reset(&s);
  ansi_parse_csi("\033[1;31m", &s);
  assert_int(s.fg, ==, TB_RED | TB_BOLD);

  // Color + bold
  ansi_state_reset(&s);
  ansi_parse_csi("\033[31;1m", &s);
  assert_int(s.fg, ==, TB_RED | TB_BOLD);

  // Bold off
  ansi_parse_csi("\033[22m", &s);
  assert_int(s.fg, ==, TB_RED);
}

static void test_ansi_underline(void) {
  ansi_state_t s;

  ansi_state_reset(&s);
  ansi_parse_csi("\033[4m", &s);
  assert_int(s.fg & TB_UNDERLINE, ==, TB_UNDERLINE);

  // Underline + color
  ansi_state_reset(&s);
  ansi_parse_csi("\033[4;32m", &s);
  assert_int(s.fg, ==, TB_GREEN | TB_UNDERLINE);

  // Underline off
  ansi_parse_csi("\033[24m", &s);
  assert_int(s.fg, ==, TB_GREEN);
}

static void test_ansi_reverse(void) {
  ansi_state_t s;

  ansi_state_reset(&s);
  ansi_parse_csi("\033[7m", &s);
  assert_int(s.fg & TB_REVERSE, ==, TB_REVERSE);

  // Reverse off
  ansi_parse_csi("\033[27m", &s);
  assert_int(s.fg & TB_REVERSE, ==, 0);
}

static void test_ansi_multi_param(void) {
  ansi_state_t s;

  // bold + red + green bg
  ansi_state_reset(&s);
  ansi_parse_csi("\033[1;31;42m", &s);
  assert_int(s.fg, ==, TB_RED | TB_BOLD);
  assert_int(s.bg, ==, TB_GREEN);

  // red, then reset, then blue
  ansi_state_reset(&s);
  ansi_parse_csi("\033[31;0;34m", &s);
  assert_int(s.fg, ==, TB_BLUE);
  assert_int(s.bg, ==, TB_DEFAULT);
}

static void test_ansi_default_fg_bg(void) {
  ansi_state_t s;

  ansi_state_reset(&s);
  s.fg = TB_RED;
  s.bg = TB_GREEN;
  ansi_parse_csi("\033[39m", &s);
  assert_int(s.fg, ==, TB_DEFAULT);
  assert_int(s.bg, ==, TB_GREEN);

  ansi_parse_csi("\033[49m", &s);
  assert_int(s.bg, ==, TB_DEFAULT);
}

static void test_ansi_extended_256_skipped(void) {
  ansi_state_t s;

  // 256-color fg: should be ignored (keep previous/use default)
  ansi_state_reset(&s);
  s.fg = TB_RED;
  ansi_parse_csi("\033[38;5;208m", &s);
  // extended codes are not representable in 16-bit, so fg should be unchanged
  // Actually, we don't change fg at all for extended codes
  assert_int(s.fg, ==, TB_RED);

  // 256-color bg
  s.bg = TB_BLUE;
  ansi_parse_csi("\033[48;5;100m", &s);
  assert_int(s.bg, ==, TB_BLUE);
}

static void test_ansi_truecolor_skipped(void) {
  ansi_state_t s;

  ansi_state_reset(&s);
  s.fg = TB_GREEN;
  ansi_parse_csi("\033[38;2;255;100;0m", &s);
  assert_int(s.fg, ==, TB_GREEN);
}

static void test_ansi_non_sgr_skipped(void) {
  ansi_state_t s;

  ansi_state_reset(&s);
  s.fg = TB_RED;
  // Cursor up (not SGR) should be skipped without affecting state
  ansi_parse_csi("\033[2A", &s);
  assert_int(s.fg, ==, TB_RED);
  assert_int(s.bg, ==, TB_DEFAULT);

  // Cursor forward
  ansi_parse_csi("\033[5C", &s);
  assert_int(s.fg, ==, TB_RED);
}

static void test_ansi_reset_preserves_extended_color(void) {
  // Extended colors set then reset should go back to default
  ansi_state_t s;

  ansi_state_reset(&s);
  ansi_parse_csi("\033[38;5;208m", &s);
  // Extended is skipped, so state should not have changed
  assert_int(s.fg, ==, TB_DEFAULT);

  ansi_parse_csi("\033[0m", &s);
  assert_int(s.fg, ==, TB_DEFAULT);
}

static void test_ansi_extended_with_bold(void) {
  ansi_state_t s;

  ansi_state_reset(&s);
  // bold + extended fg
  ansi_parse_csi("\033[1;38;5;208m", &s);
  assert_int(s.fg, ==, TB_BOLD); // extended ignored, bold stays

  // Extended bg + underline
  ansi_state_reset(&s);
  ansi_parse_csi("\033[4;48;5;100m", &s);
  assert_int(s.fg, ==, TB_UNDERLINE);
}

Test color_tests[] = {
    {"/ansi/basic_fg", test_ansi_basic_fg},
    {"/ansi/bright_fg", test_ansi_bright_fg},
    {"/ansi/basic_bg", test_ansi_basic_bg},
    {"/ansi/bright_bg", test_ansi_bright_bg},
    {"/ansi/reset", test_ansi_reset},
    {"/ansi/bold", test_ansi_bold},
    {"/ansi/underline", test_ansi_underline},
    {"/ansi/reverse", test_ansi_reverse},
    {"/ansi/multi_param", test_ansi_multi_param},
    {"/ansi/default_fg_bg", test_ansi_default_fg_bg},
    {"/ansi/extended_256_skipped", test_ansi_extended_256_skipped},
    {"/ansi/truecolor_skipped", test_ansi_truecolor_skipped},
    {"/ansi/non_sgr_skipped", test_ansi_non_sgr_skipped},
    {"/ansi/reset_preserves_extended", test_ansi_reset_preserves_extended_color},
    {"/ansi/extended_with_bold", test_ansi_extended_with_bold},
    {NULL, NULL},
};
