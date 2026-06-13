# TODO

## Bugs

* Creating new file before the cursor item in list with command will move cursor to different item
* Yanking file in one instance doesn't update the others
* Renaming file so it changes position in list will leave cursor on different file
* Renaming file to the name of different file overwrites it
* With OPT_FULL_SHELL set to 0, long running commands like `:sleep 10` can't be cancelled with CTRL+C. It works with OPT_FULL_SHELL set to 1
* Some commands, like `:vi` (not `:nvim`) will freeze the program and require fully closing the terminal
* Possible to overflow when adding to command history and if the commands are too long

## Features

* Periodically poll directories for changes (`tb_peek_event()` and `scandir()`)
* Monitor filesystem events to update directories. inotify (Linux) and kqueue (macOS/BSD) - [File Watcher in C](https://www.youtube.com/watch?v=07sXGHxjbRI)
* Different colors for executable files (use `access()` and string concat for ../ to work in left col)
* Smarter cursor position when toggling hidden files
* Remember cursor position in right column when moving back multiple directories
* Remember cursor position in right column for all directories (it's currently reset when moving up/down)
* Move cursor to new file/dir after commands, touch/mkdir/etc
* Move cursor to pasted file/dir after copy/move
* Select files and copy/cut multiple
* Show progress for copy/move
* Handle collision with existing files when pasting after copy/move
* Bind commands to keys in config.h (can be used for bookmarks with cd)
* Ability to write to log file what is happening while running for debugging
* Support showing multiline output from running command
* When renaming file with extension, put cursor right before extension instead of end
* Support running commands with fish shell, instead of falling back on bash
* Add OPT_CMD_HISTORY to disable command history completely

## General improvements

* Do more partial clearings instead of full `tb_clear()`. Maybe the draw functions for each column should trigger clear if there are changes.
* Add tests for more core functionality
* Make os_exec_output return bool if screen should be refreshed or not to avoid flickering on commands like `:pwd`. Commands like `:touch` should still update. Probably have to check if files in directory were modified.
* See if more of the current functionality has config options in lf
* Add comments to all config options
