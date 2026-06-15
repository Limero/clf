# TODO

## Bugs

* Yanking file in one instance doesn't update the others
* Possible to overflow when adding to command history and if the commands are too long

## Features

* Periodically poll directories for changes (`tb_peek_event()` and `scandir()`)
* Monitor filesystem events to update directories. inotify (Linux) and kqueue (macOS/BSD) - [File Watcher in C](https://www.youtube.com/watch?v=07sXGHxjbRI)
* Different colors for executable files (use `access()` and string concat for ../ to work in left col)
* Smarter cursor position when toggling hidden files
* Remember cursor position in right column when moving back multiple directories
* Remember cursor position in right column for all directories (it's currently reset when moving up/down)
* Move cursor to new file/dir after commands, touch/mkdir/etc
* Show progress for copy/move
* Handle collision with existing files when pasting after copy/move
* Ability to write to log file what is happening while running for debugging
* When renaming file with extension, put cursor right before extension instead of end
* Support running commands with fish shell, instead of falling back on bash
* 'smartcase' config option (if pattern has uppercase, treat as case-sensitive)
* 'timefmt' config option to change status bar date/time format
* Consider implementing scrollable pager for long command output instead of capping it
* Show command output while its running, not just on finish
* Support colors in preview
* Investigate supporting image previews

## General improvements
* Do more partial clearings instead of full `tb_clear()`. Maybe the draw functions for each column should trigger clear if there are changes.
* Add tests for more core functionality
* Make os_exec_output return bool if screen should be refreshed or not to avoid flickering on commands like `:pwd`. Commands like `:touch` should still update. Probably have to check if files in directory were modified.
* When multi-selection is disabled, program still creates memory for the full clipboard
* Performance config flags might have to converted to `#define` to actually be useful
* `-r` is appended to `CMD_COPY`, which might be a problem if it's changed to something different
* Clear command output on enter instead of opening current cursor file
