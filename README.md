# clf

clf is a terminal file manager that aims to be a super-minimal and portable version of [LF](https://github.com/gokcehan/lf), [Ranger](https://github.com/ranger/ranger) and [joshuto](https://github.com/kamiyaa/joshuto) written in C99.

It offers the same core functionality and default keybinds as the other file managers but expects more advanced operations to be done by the user through shell commands.

For terminal output it uses [termbox2](https://github.com/termbox/termbox2), a TUI library offering similar functionality to [ncurses](https://en.wikipedia.org/wiki/Ncurses) but more minimalistic.

It supports moving/copying files between multiple instances of the program through named pipes, instead of using a client/server model like LF.

## Keybinds

* VI keys for navigation
    * `h` - navigate to previous directory
    * `j` - select next file (can be repeated by typing a number before)
    * `k` - select previous file (can be repeated by typing a number before)
    * `l` - navigate to next directory or open file
    * `g` - go to top (or to a specific line by typing a number before)
    * `G` - go to bottom
* Regular keys for navigation
    * `Up` - select previous file (can be repeated by typing a number before)
    * `Down` - select next file (can be repeated by typing a number before)
    * `Left` - navigate to previous directory
    * `Right` - navigate to next directory or open file
    * `Home` - go to top (or to a specific line by typing a number before)
    * `End` - go to bottom
    * `Page Up` or `Ctrl+U` - scroll half list up
    * `Page Down` or `Ctrl+D` - scroll half list down
* File/directory operations
    * `D` - delete selected file or directory (with confirmation)
    * `r` - rename selected file or directory
    * `:touch abc` - create file named abc (shell)
    * `:mkdir abc` - create directory named abc (shell)
    * Any other shell commands prefixed with `:`
* Toggle hidden files with `Ctrl-H`
* Refresh directories with `Ctrl-R`
* Open `$SHELL` in the current directory with `S`; return to the file manager with `exit`
* Exit with `Ctrl-C`, `q`, or `:q`

## Preview/open

To show file previews, it calls a script named `scope` in your `$PATH`. The arguments are: `name of the current selection`, `columns of the preview window` and `lines of the preview window`.

For opening files, it will call a script named `opener` in your `$PATH`. The argument is: `name of the current selection`.

## Shell integration

Run shell commands with `:` and navigate command history with `Ctrl+P`/`Ctrl+N` or Up/Down. Clear the current command with `Ctrl+U`; use `Ctrl+A`/`Home` to go to the beginning of the line and `Ctrl+E`/`End` to go to the end. Return to normal mode with `ESC` and execute current command with `Enter`. The executed command will receive the currently selected file in the `$f` environment variable.

Shell integration with `:` in clf works similarly to `%` in LF, where the output is piped back and displayed inside the program.

If the executed shell command changes the working directory, the file manager will move to that directory when the command completes. This can be used for bookmarks.

## Usage

Clone the repository

```
git clone https://github.com/Limero/clf
```

Run `make release` and optionally `make install` to install it to `~/.local/bin`.

For developers, there are additional commands in the [Makefile](Makefile), see `make help`.

## Design decisions

* Follows the [suckless philosophy](https://suckless.org/philosophy/). However, unlike other official suckless software, code is separated into multiple files, there are tests and optional features can be toggled through configuration instead of requiring patches
* Configuration is done at build time, like with other suckless software. See configuration options in [config.h](config.h). To change keybinds, set the `TB_KEY_x` macros; see [termbox2.h](https://github.com/termbox/termbox2/blob/master/termbox2.h) and [nav.c](nav.c)
* No non-core features that would require adding significant extra code or complexity
* Stateless - nothing is saved, so command history is not persistent
* Focuses on POSIX - Linux/BSD/macOS only
* Keyboard navigation only
* Single binary with no runtime dependencies
* Once all core features are added, the program will be considered complete and will focus only on bug fixes and performance/efficiency improvements. See [TODO](TODO.md) for a list of some of the remaining items

## License

[MIT](LICENSE)
