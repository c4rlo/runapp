# runapp

_runapp_ is an application runner for Linux desktop environments that integrate with systemd.

Its job is simple: it launches the given application in an appropriate systemd user unit, according
to systemd's [idea](https://systemd.io/DESKTOP_ENVIRONMENTS/#xdg-standardization-for-applications)
of how this should be done.

It is useful if you are using a light-weight Linux desktop such as [Sway](https://swaywm.org/). In
such cases, `runapp` works great as a complement to your favourite application launcher, such as
[Fuzzel](https://codeberg.org/dnkl/fuzzel).

`runapp` owes its inspiration to [uwsm](https://github.com/Vladimir-csp/uwsm), a great way to go
all-in on the systemd way of running applications in a Desktop context, and another great complement
to `runapp`. In fact, the `uwsm app` subcommand has exactly the same purpose as `runapp`, and has
several more features; see below [Missing features](#missing-features) section for a comparison.

So why `runapp`? Its advantage over `uwsm app` is that it is a native executable (written in C++),
hence it has significantly lower startup latency and resource usage (`uwsm` is written in Python).
And compared to the `uwsm-app` tool that is shipped with `uwsm`, `runapp` does not require a
permanently-running background daemon.

## Usage

If you use [Sway](https://swaywm.org/) with [Fuzzel](https://codeberg.org/dnkl/fuzzel), you might
include something like the following in your `.config/sway/config` file:

```
bindsym Mod4+d runapp fuzzel --launch-prefix runapp
```

This will launch Fuzzel via `runapp` when the `Super+D` key combination is pressed, and tells Fuzzel
to also run any application it launches via `runapp`.

Variants of the above will apply for other combinations of desktop compositors and application
launchers.

## Installation

- If using Arch Linux, install [runapp from the AUR](https://aur.archlinux.org/packages/runapp)
  ([what's the AUR?](https://wiki.archlinux.org/index.php/Arch_User_Repository)).
- Otherwise, run `make install`. This requires that you have a recent GCC with C++
  compiler and GNU Make. You may be prompted for your `sudo` password.
  To uninstall again, run `make uninstall`.

## Current features

- Fast
- No dependencies beyond systemd
- Run app either as systemd [service](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html)
  (recommended, default) or as systemd [scope](https://www.freedesktop.org/software/systemd/man/latest/systemd.scope.html)
    - The latter means `runapp` directly executes the application, after registering it with systemd
- Run app either under `app-graphical.slice` (recommended for most cases, default) or under any other slice
- If run from Fuzzel, derive unit name from `.desktop` name, per systemd recommendations
- On error, if not run from interactive terminal, show desktop notification

## Missing features

Most of the features on this list are implemented in `uwsm app`.

- Unit description: derive from `.desktop` file
  - For Fuzzel, would be made much easier (and more performant) with https://codeberg.org/dnkl/fuzzel/issues/292
- Support being given a Desktop File ID (only)
  - With optional Action ID
- Support running `.desktop` files with args, using field codes
  - See https://specifications.freedesktop.org/desktop-entry-spec/latest/exec-variables.html
  - Note that `%f` and `%u` entail running multiple app instances
  - Alternatively, this could be done by the launcher, e.g. Fuzzel: https://codeberg.org/dnkl/fuzzel/issues/346
- Support running app under Terminal
- Support custom unit name / description
- Alternative ways of accepting Desktop File ID (beyond Fuzzel)

## Development

Prerequisites: Recent GCC with C++ compiler and GNU Make.

- `make debug`: create debug build.
- `make compile_commands.json`: generate [`compile_commands.json`](https://clang.llvm.org/docs/JSONCompilationDatabase.html) file,
  useful for language servers like [`clangd`](https://clangd.llvm.org/); requires [`bear`](https://github.com/rizsotto/Bear).
- `make release`: create release build.
- `make clean`: delete all build artefacts.
