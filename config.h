// Command to preview files
static const char *CMD_PREVIEW = "scope";

// Command to open files
static const char *CMD_OPEN = "opener";

// Command used to delete files/folders
static const char *CMD_DELETE = "rm -rf";

// Command used to move and rename files/folders
static const char *CMD_MOVE = "mv";

// Command used to copy files/folders
static const char *CMD_COPY = "cp";

// Number of space characters to replace tab with in preview
const unsigned int TAB_WIDTH = 4;

// Scrolling can wrap around the file list
const unsigned int OPT_WRAP_SCROLL = 1;

// Searching can wrap around the file list
const unsigned int OPT_WRAP_SCAN = 1;

// Show the position number for directory items on the left side of the pane
const unsigned int OPT_NUMBER = 1;

// Show the position number relative to the current line
const unsigned int OPT_RELATIVE_NUMBER = 1;

// Ignore case in search and command tab completion
const unsigned int OPT_IGNORE_CASE = 1;

// Delay in ms before showing the ">" indicator when running a command
// Prevents flickering for fast commands like :pwd
const unsigned int OPT_CMD_INDICATOR_DELAY_MS = 80;

// -- The options below have a performance impact, disable for a minimal build --

// Run commands in $SHELL with rc files sourced instead of /bin/sh
// Allows the use of aliases, but has a small performance impact
const unsigned int OPT_FULL_SHELL = 1;

// Enable tab completion for commands in command mode (PATH executables + shell aliases)
// Will allocate an extra ~4MB static memory
const unsigned int OPT_CMD_COMPLETE = 1;

// Enable multi-selection with space
// Will allocate an extra ~256KB static memory
const unsigned int OPT_MULTISELECT = 1;

// Enable command history (up/down arrows to recall previous commands)
// Will allocate an extra ~25KB static memory
const unsigned int OPT_CMD_HISTORY = 1;
