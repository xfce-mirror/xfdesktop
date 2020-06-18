## What is it?

xfdesktop is a desktop manager for the Xfce Desktop Environment. Desktop
in this respect means the root window (or, rather, a window that sits on top
of the root window). The manager handles the following tasks:
* background image / color
* root menu, window list
* minimized app icons
* file icons on the desktop (using Thunar libs)


## Minimum Requirements

* intltool 0.31
* GTK+ 3.22.0
* libxfce4util 4.13
* libxfce4ui 4.13
* libwnck 3.14
* libexo 0.11
* xfconf 4.12.1
* garcon 0.6.0 (optional; required for apps menu)
* thunar 1.7.0 (optional; required for file icons)
* tumbler 1.6 (optional; enables thumbnail previews for file icons)
* cairo 1.12


## Installation

The file [`INSTALL`](INSTALL) contains generic installation instructions.


## Debugging Support

xfdesktop currently supports three different levels of debugging support,
which can be setup using the configure flag `--enable-debug` (check the output
of `configure --help`):

| Argument  | Description |
| -------   | ----------- |
| `yes`     | This is the default for Git snapshot builds. It adds all kinds of checks to the code, and is therefore likely to run slower. Use this for development of xfdesktop and locating bugs in xfdesktop. |
| `minimum` | This is the default for release builds. **This is the recommended behaviour.** |
| `no`      | Disables all sanity checks. Don't use this unless you know exactly what you do. |


## How to report bugs?

Bugs should be reported to [Xfce's GitLab](https://gitlab.xfce.org/xfce/xfdesktop). You will need to create an account for yourself.

