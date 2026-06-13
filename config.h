static const char *CMD_PREVIEW = "scope";
static const char *CMD_OPEN = "opener";
static const char *CMD_DELETE = "rm -rf";
static const char *CMD_MOVE = "mv";
static const char *CMD_COPY = "cp";

const unsigned int TAB_WIDTH = 4;

const unsigned int OPT_WRAP_SCROLL = 1;
const unsigned int OPT_WRAP_SCAN = 1;
const unsigned int OPT_NUMBER = 1;
const unsigned int OPT_RELATIVE_NUMBER = 1;
const unsigned int OPT_IGNORE_CASE = 1;

// Run commands in $SHELL with rc files sourced instead of /bin/sh
// Allows the use of aliases, but has a small performance impact
const unsigned int OPT_FULL_SHELL = 1;

// Enable tab completion for commands in command mode (PATH executables + shell aliases)
const unsigned int OPT_CMD_COMPLETE = 1;

// Delay in ms before showing the ">" indicator when running a command
// Prevents flickering for fast commands like :pwd
const unsigned int CMD_INDICATOR_DELAY_MS = 80;
