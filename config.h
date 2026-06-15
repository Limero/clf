// Normal mode keybinds: "cmd" starting with ":" is a shell command, otherwise a built-in action name
static const struct {
  unsigned char key1;
  unsigned char key2; // 0 for single-key binding
  const char *cmd;    // ":" prefix = shell command, otherwise built-in action
} KEY_COMMANDS[] = {
    // Navigation
    {'k', 0, "nav_up"},
    {'j', 0, "nav_down"},
    {'h', 0, "nav_left"},
    {'l', 0, "nav_right"},
    {'g', 0, "nav_goto"},
    {'G', 0, "nav_bottom"},

    // File operations
    {' ', 0, "multi_select"},
    {'y', 0, "yank_copy"},
    {'d', 0, "yank_cut"},
    {'D', 0, "delete"},
    {'p', 0, "paste"},
    {'r', 0, "rename"},

    // Search
    {'n', 0, "search_next"},
    {'N', 0, "search_prev"},
    {'/', 0, "search"},
    {'?', 0, "search"},
    {'f', 0, "find"},
    {'F', 0, "find_back"},

    // Mode / system
    {':', 0, "command"},
    {'S', 0, "shell"},
    {'q', 0, "quit"},

    // Directory history
    {'-', 0, "nav_cd_prev"},

    // Custom shell commands
    {'c', 'h', ":cd ~"},
    {'c', 't', ":cd /tmp"},
    {'c', 'd', ":cd ~/Downloads"},
};

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

// Searching can wrap around the file list (always enabled for f/F)
const unsigned int OPT_WRAP_SCAN = 1;

// Show the position number for directory items on the left side of the pane
const unsigned int OPT_NUMBER = 1;

// Show the position number relative to the current line
const unsigned int OPT_RELATIVE_NUMBER = 1;

// Ignore case in search and command tab completion (always disabled for f/F)
const unsigned int OPT_IGNORE_CASE = 1;

// Move cursor to results while searching
const unsigned int OPT_INCSEARCH = 1;

// Delay in ms before showing the ">" indicator when running a command
// Prevents flickering for fast commands like :pwd
const unsigned int OPT_CMD_INDICATOR_DELAY_MS = 80;

// -- The options below have a performance impact, disable for a minimal build --

// Run commands in $SHELL with rc files sourced (aliases available, slightly slower startup)
// Set to 0 to use /bin/sh directly (faster, no aliases)
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
