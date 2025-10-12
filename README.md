# runapp

_runapp_ is a Linux desktop application runner that launches the given application in an appropriate
systemd user unit, according to [systemd recommendations](https://systemd.io/DESKTOP_ENVIRONMENTS/).

By running each application in its own systemd service, you avoid the risk of a single memory-hungry
application taking down your entire compositor session, and you gain the
ability to easily inspect applications via e.g.
`systemctl --user status <my-app>`.

A great complement to runapp is [uwsm](https://github.com/Vladimir-csp/uwsm), which provides
an easy way to run your Wayland compositor as well as user services and applications under systemd.

In fact, runapp is inspired by `uwsm app`, which has a similar feature set. However, compared to
it and other alternatives, runapp is very fast (native executable written in C++) and has minimal
dependencies (only systemd, no Python, binary is <200K).

## Installation

- Arch Linux: [Get runapp from the AUR](https://aur.archlinux.org/packages/runapp)
  ([Arch User Repository](https://wiki.archlinux.org/index.php/Arch_User_Repository)).
- NixOS: [runapp is being added to nixpkgs](https://github.com/NixOS/nixpkgs/pull/447721).
- Other: Run `make install`.
  - This requires that you have GCC 15 or later with C++
  compiler and GNU Make.
  - You may be prompted for your `sudo` password.
  - To uninstall again, run `make uninstall`.

## Prerequisites

Your systemd user instance must include some key environment variables needed to run
graphical applications.

To verify this, check that the output of `systemctl --user show-environment` includes
`WAYLAND_DISPLAY` (if using Wayland) or `DISPLAY` (if using Xorg), as well as anything else your
compositor may require (e.g. Sway needs `SWAYSOCK`, i3 needs `I3SOCK`).

If something is missing,
review your compositor setup; I recommend using [uwsm](https://github.com/Vladimir-csp/uwsm) and following its setup instructions, which
will take care of everything.

## Usage

Running an application via runapp is as simple as e.g. `runapp firefox` to run Firefox.

Or if you want to inspect the systemd unit, the `-v`/`--verbose` option is useful:

```
$ runapp -v zathura
Resolved executable zathura to /usr/bin/zathura
Launching app-sway-zathura@7c715bb4dcb4e28d.service: ["zathura"].
Success.

$ systemctl --user status app-sway-zathura@7c715bb4dcb4e28d.service 
● app-sway-zathura@7c715bb4dcb4e28d.service - zathura
     Loaded: loaded (/run/user/1000/systemd/transient/app-sway-zathura@7c715bb4dcb4e28d.service; transient)
  Transient: yes
     Active: active (running) since Sun 2025-10-12 19:46:49 UTC; 1min 27s ago
 Invocation: b39d29d98b684eed8e7a900fdb3fd86f
   Main PID: 49494 (zathura)
      Tasks: 8 (limit: 18644)
     Memory: 26.7M (peak: 32.7M)
        CPU: 334ms
     CGroup: /user.slice/user-1000.slice/user@1000.service/app.slice/app-graphical.slice/app-sway-zathura@7c715bb4dcb4e28d.service
             └─49494 zathura

Oct 12 19:46:49 localhost systemd[655]: Starting zathura...
Oct 12 19:46:49 localhost systemd[655]: Started zathura.
Oct 12 19:46:49 localhost zathura[49494]: info: Opening plain database via sqlite backend.
Oct 12 19:46:49 localhost zathura[49494]: info: No plain database available. Continuing with sqlite database.

$ systemctl --user stop app-sway-zathura@7c715bb4dcb4e28d.service 
```

To see what else you can do, try `runapp --help`:

```
runapp [OPTIONS] COMMAND...
    Run COMMAND as a systemd user unit, in a way suitable for typical applications.
    Options:

    -v, --verbose: Increase output verbosity.
    -o, --scope:   Run command directly, registering it as a systemd scope;
                   the default is to run it as a systemd service.
    -i SLICE, --slice=SLICE:
                   Assign the systemd unit to the given slice (name must include
                   ".slice" suffix); the default is "app-graphical.slice".
    -d DIR, --dir=DIR:
                   Set working directory of command to DIR.
    -e VAR=VALUE, --env=VAR=VALUE:
                   Run command with given environment variable set;
                   may be given multiple times.

runapp --help
    Show this help text.
```

Or you can read the man page via `man runapp`.

## Integrations

Probably you don't always want to type `runapp firefox` to run Firefox (for example).

A simple way to improve on this is to make a shell alias, function, or script with
a short and memorable name that runs `runapp firefox` for you.

In addition, there are some other ways that applications are often launched:

### Application launchers

Tell your launcher to run the selected applications via runapp.

- [Fuzzel](https://codeberg.org/dnkl/fuzzel): `fuzzel --launch-prefix=runapp` or add `launch-prefix=runapp` to the config file.
- [Rofi](https://davatorium.github.io/rofi/): `rofi -show drun -show-icons -run-command 'runapp {cmd}'`.

### File managers

Tell your file manager to open a selected file in an application via runapp.

- [Yazi](https://yazi-rs.github.io/): Configure [openers](https://yazi-rs.github.io/docs/configuration/yazi#opener)
  that run applications via `runapp`, then use them in your
  [`[open]`](https://yazi-rs.github.io/docs/configuration/yazi#open) configuration.
- [LF](https://github.com/gokcehan/lf): Define [`cmd open`](https://github.com/gokcehan/lf/blob/master/doc.md#opening-files)
  to something that launches the application via `runapp`.
- [Ranger](https://ranger.fm/): [Configure rifle](https://github.com/ranger/ranger/blob/master/ranger/config/rifle.conf)
  to run each application via `runapp`.
- [Vifm](https://vifm.info/): Use the [`:filetype`](https://vifm.info/docs/vifm-app.txt#vifm-%3Afiletype) setting.

### File openers

- [Rifle](https://github.com/ranger/ranger/wiki/Official-User-Guide#rifle) (part of Ranger):
  see Ranger above.

Various others, including [`xdg-open`](https://www.freedesktop.org/wiki/Software/xdg-utils/), are
unfortunately not sufficiently customizable to allow interposing `runapp`.

### Hotkeys

- [Sway](https://swaywm.org/): Configure e.g. `bindsym Mod4+Return exec runapp foot` to have it
  launch the _foot_ terminal emulator under runapp on `Super+Return`.
- [Hyprland](https://hypr.land/): Configure e.g. `bind = SUPER, Return, exec, runapp foot` for
  the same thing.

## Features

- Fast: native code (written in C++); talks directly to systemd, via its private socket if available, the same way that `systemd-run` does.
- No dependencies beyond systemd.
- Run app either as systemd [service](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html)
  (recommended, default) or as systemd [scope](https://www.freedesktop.org/software/systemd/man/latest/systemd.scope.html).
    - The latter means `runapp` directly executes the application, after registering it with systemd.
- Run app either under `app-graphical.slice` (recommended for most cases, default) or under any other slice.
- Option to run app in given working directory.
- Option to run app with given environment variables.
- If run from Fuzzel, derive unit name from `.desktop` name, per systemd recommendations.
- On error, if not run from interactive terminal, show desktop notification.

## Non-features

I don't consider these very important to have, but I'm open to discussions (feel free to open an issue!).

- Support custom unit name / description.
- Unit description: derive from `.desktop` file.
  - For Fuzzel, would be made much easier (and more performant) with https://codeberg.org/dnkl/fuzzel/issues/292
- Support being given a Desktop File ID (only).
  - With optional Action ID
- Support running `.desktop` files with args, using field codes.
  - See https://specifications.freedesktop.org/desktop-entry-spec/latest/exec-variables.html
  - Note that `%f` and `%u` entail running multiple app instances
  - Alternatively, this could be done by the launcher, e.g. Fuzzel: https://codeberg.org/dnkl/fuzzel/issues/346
- Alternative ways of accepting Desktop File ID (beyond Fuzzel).
- Support running app connected to terminal, like `systemd-run --pty`.

## Alternatives

- [`uwsm app`](https://github.com/Vladimir-csp/uwsm?tab=readme-ov-file#3-applications-and-slices):
  the original; somewhat slow due to being written in Python.
- [`uwsm-app`](https://github.com/Vladimir-csp/uwsm/blob/master/scripts/uwsm-app.sh):
  shell script that ships with uwsm; spawns a background daemon.
- [`app2unit`](https://github.com/Vladimir-csp/app2unit): self-sufficient and feature-complete
  shell script by the author of uwsm.

## Development

Prerequisites: GCC 15 or later with C++ compiler, and GNU Make.

- `make debug`: create debug build.
- `make compile_commands.json`: generate [`compile_commands.json`](https://clang.llvm.org/docs/JSONCompilationDatabase.html) file,
  useful for language servers like [`clangd`](https://clangd.llvm.org/); requires [`bear`](https://github.com/rizsotto/Bear).
- `make release`: create release build.
- `make clean`: delete all build artefacts.
- `make install`: install release build into `/usr/local` (or override via `prefix` variable).
- `make uninstall`: delete installed release build.
