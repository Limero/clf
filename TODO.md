# TODO

## Bugs

* Yanking file in one instance doesn't update the others, probably need to poll the clipboard file
* First line of preview is drawn over full command outputs (like `:git log`)
* Gap between middle and right column on "empty" message in empty dir doesn't match indentation of preview
* Indentation of `:ls` is uneven because we use `TAB_WIDTH` instead of fixed 8 spaces

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
* Investigate supporting image previews
* Support truecolor in preview and command output for programs like `bat` (set TB_OPT_ATTR_W)
* Skip passing binary files to previewer like lf (or let previewer script handle that?)
* Consider full mouse support. Currently only scrolling is implemented (if added, update readme)

## General improvements
* Add tests for more core functionality
* Fix flicker in preview window when running commands like `:pwd` and toggling hidden files, while still making sure dirs get updated when you do `:touch` or similar
* When multi-selection is disabled, program still creates memory for the full clipboard
* Performance config flags might have to converted to `#define` to actually be useful
* `-r` is appended to `CMD_COPY`, which might be a problem if it's changed to something different
* Clear command output on enter instead of opening current cursor file
* Cut preview lines like lf or continue wrapping even if it breaks commands like `bat` in smaller windows? `render_ansi_str` has an argument to wrap or not
* Investigate if all current `tb_present` are necessary
* Consider adding previewer fallback to `bat`/`cat` and something for opener
* Add example printscreen to README
* Add example opener/previewer script
