# TODO
* Tab characters in preview isn't working correctly
* Prefix yanked files
* Do more partial clearings instead full tb_clear()
* Use int_digits helper in columns for dynamic line number width
* Current linenumber is padded in lf but not here
* Periodically reload directories
* Monitor filesystem events to update directories
* Different colors for executable files (use access() and string concat for ../ to work in left col)
* Smarter cursor position when toggling hidden files
* Remember position in right column when moving back multiple directories
* Remember position in right column for all directories (it's currently reset when moving up/down)
* Select new file/dir after touch/mkdir/etc.
* Select files and copy/cut multiple
* Show progress for copy/move
* Add tests for more core functionality
* Other TODO comments around the codebase
