# TODO
* Do more partial clearings instead full tb_clear()
* Yanking file in one instance doesn't update the others
* Periodically reload directories
* Monitor filesystem events to update directories
* Different colors for executable files (use access() and string concat for ../ to work in left col)
* Smarter cursor position when toggling hidden files
* Remember position in right column when moving back multiple directories
* Remember position in right column for all directories (it's currently reset when moving up/down)
* Select new file/dir after touch/mkdir/etc.
* Select files and copy/cut multiple
* Show progress for copy/move
* Tab completion for commands
* Add tests for more core functionality
* Handle collision with existing files when pasting after copy/move
* Having last file in folder selected and removing it with command should move up cursor
* Create new file before selected item in list with command will move cursor to different item
* Make os_exec_output return bool if screen should be refreshed or not to avoid flickering on commands like `pwd`
* Bind commands to keys in config.h (can be used for bookmarks with cd)
