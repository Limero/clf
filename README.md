# clf

clf is a terminal file manager that aims to be a super-minimal and portable version of [LF](https://github.com/gokcehan/lf), [Ranger](https://github.com/ranger/ranger) and [joshuto](https://github.com/kamiyaa/joshuto) written in C99.

It offers the same core functionality and default keybinds as the other file managers but expects more advanced operations to be done by the user through shell commands.

For terminal output it uses [termbox2](https://github.com/termbox/termbox2), a TUI library offering similar functionality to [ncurses](https://en.wikipedia.org/wiki/Ncurses) but more minimalistic.

It supports moving/copying files between multiple instances of the program through a shared temporary file, instead of using a client/server model like LF.

## Keybinds

All character keybinds below are configurable in `KEY_COMMANDS[]` in [config.h](config.h).

* VI navigation
    * `h` - parent directory
    * `j` - next file (repeatable with a number prefix, e.g. `5j`)
    * `k` - previous file (repeatable)
    * `l` - enter directory or open file
    * `g` - top (or line N with number prefix)
    * `G` - bottom
* Regular navigation
    * `Up` / `Down` - previous / next file
    * `Left` / `Right` or `Enter` - parent / enter directory
    * `Home` / `End` - top / bottom
    * `PgUp` / `Ctrl+U` and `PgDn` / `Ctrl+D` - scroll half page
* File operations
    * `Space` - toggle multi-select
    * `y` - yank (copy) selection
    * `d` - yank (cut) selection
    * `p` - paste yanked files
    * `D` - delete (with confirmation)
    * `r` - rename
* Search
    * `/` - search forward
    * `?` - search backward
    * `n` / `N` - next / previous search result
    * `f` / `F` - find by first character forward / backward
* Shell
    * `:` - enter command mode (type a command and press Enter; runs via `$SHELL -c`)
    * `S` - spawn `$SHELL` interactively; return with `exit`
    * `ch` - `cd ~`
    * `ct` - `cd /tmp`
    * `cd` - `cd ~/Downloads`
* Directory history
    * `-` or `:cd -` - go back to previous directory (toggles between current and previous)
* `Ctrl+H` - toggle hidden files
* `Ctrl+R` - refresh directories
* `q`, `Ctrl+C`, or `:q` - exit

## Preview/open

To show file previews, it calls a script named `scope` in your `$PATH`. The arguments are: `name of the current file`, `columns of the preview window` and `lines of the preview window`.

For opening files, it will call a script named `opener` in your `$PATH`. The argument is: `name of the current file`.

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
* Configuration is done at build time, like with other suckless software. See configuration options in [config.h](config.h). Character keybinds are configured through the `KEY_COMMANDS[]` array in config.h. Named-key binds (arrows, enter, etc.) use `TB_KEY_x` macros in [nav.c](nav.c).
* No non-core features that would require adding significant extra code or complexity
* Stateless - nothing is saved, so command history is not persistent
* Focuses on POSIX - Linux/BSD/macOS only
* Keyboard navigation only
* Single binary with no runtime dependencies
* Once all core features are added, the program will be considered complete and will focus only on bug fixes and performance/efficiency improvements. See [TODO](TODO.md) for a list of some of the remaining items

## License

[MIT](LICENSE)
