This file is inspired by Jasper's HACKING file for xfce4-panel, and gives a
decent idea of the coding style I prefer for xfdesktop, as well as random
hacking guidelines, and a brief (if incomplete) tour of how the code is
organized.  If you submit a patch, please be sure to conform to these
'rules'.  If anything isn't clear, just ask (#xfce-dev on LiberaChat
IRC, or the xfce-dev mailing list).

last modified: 2022/12/01

## Coding style

- Line length: I try to keep to 120 characters or so, though longer is
  sometimes fine.  Break up function calls on parameter boundaries if
  possible, by lining up the parameters on the next line with the first
  parameter on the previous line.
- Braces: on the same line as the if/while/etc. statement.  Braces
  should NOT be omitted for one-line blocks.  As an exception, the
  opening brace after a function declaration goes on its own line.
- Parentheses: Always put a space before the opening parentheses for
  if/while/etc. statements, but never for function calls.  No space on
  the inside of the parentheses either.  No space between the
  closing parenthesis of a type cast and the variable name.
- Function declarations: return type goes on the first line, function
  name and first parameter on the second line.  Second and further
  parameters each get their own line, and they should be aligned with
  the start of the first parameter.  Please do not add spaces between
  the type and variable name to line up the variable names.  As
  specified before, the opening brace also gets its own line.
- Indentation: four spaces.  No tab characters.
- Variable declarations: don't line up the variable names by padding
  shorter type names with spaces.  Feel free to combine multiple
  same-type declarations to the same line.
- Conditionals: even though C has rules for what is "truthy" and
  "falsey", please for the most part use comparison operators.  For
  example, if you have a pointer that you want to check is non-null,
  instead of writing `if (ptr)`, write `if (ptr != NULL)`.

NB: Some parts of the code don't conform to the above style.  Xfdesktop has
changed hands maintainership-wise several times, and sometimes coding style
guidelines haven't been followed.  Do try to use the above style for any new
code, or lines that you modify.

### Other stuff

- Regardless of what gcc supports, all variable declarations should
  appear at the top of the function, without any normal statements in
  between (though it's ok to initialise variables where they're
  declared).
- I generally try to declare variables in the rough order they'll be
  used in the function.  This makes it easier if I have to go back later
  and figure out what's going on and what's getting used where.
- I won't check in code that doesn't compile when you pass
  `--enable-debug=full` to configure.  This means it must not cause
  compiler warnings.
- Comments are useful to explain stuff that's complicated or to
  enumerate why the various branches of an 'if' tree are being taken if
  the logic is somewhat complex.  Otherwise I tend not to comment my
  code all that much (not saying this is good or bad; just fact).
- ChangeLog/NEWS: don't bother modifying the toplevel ChangeLog or NEWS
  files.  They get automatically generated by scripts during the release
  process.
- Other committers with access to the xfdesktop tree should feel free to
  make small commits related to minor one-line bugfixes and fixes for
  compiler warnings.  The release manager may make whatever changes
  needed during the release process to make xfdesktop release-ready.  I
  do appreciate a quick note via email or on IRC if you make any
  changes, though I'll probably notice it via the xfce4-commits
  mailing list.  The po/ and po-doc/ directories are under control
  of the xfce-i18n team.
    
### Code sample

```
static void
example_foo_function(FooObject *cheese,
                     gboolean bar,
                     gint baz)
{
    gint i, j;
    BarObject *monkey = BAR_OBJECT(monkey);
    const gchar *happy_string;
    
    if (bar || baz == 0)
        do_something();
    else {
        really_long_function_call(cheese,
                                  "I like to eat cheese all the time",
                                  baz,
                                  monkey);
        do_something_else();
    }
    
    /* ... */
}
```

## Code layout

- /common/
  + Stuff that's used in multiple places.
- /settings/
  + This implements `xfdesktop-settings`, a stand-alone program, as well
    as a plugin for the xfce4-settings-manager.
- /src/main.c
  + This basically just the main function, it then starts
    `XfdesktopApplication`.
- /src/xfdesktop-application.c
  + Handles the startup and shutdown of xfdesktop. It uses
    `GApplication` to handle message passing, process uniqueness, parses
    the command line, and other related tasks.
- /src/menu.c
  + This actually contains very little menu code.  It just accepts the
    mouse button press and pops up the menu, along with some other
    random stuff.
- /src/windowlist.c
  + This guy assembles a list of open windows on each workspace and
    constructs a nifty menu to display them all.
- /src/xfce-backdrop.c
  + This `XfceBackdrop` represents a backdrop image.  It handles scaling
    and compositing as well and has code to handle rotating the backdrop
    image.
- /src/xfce-desktop.c
  + This is a `GtkWindow` subclass which is the actual desktop window.
    xfdesktop displays one `XfceDesktop` window (if you use an old-style
    setup with multiple X11 screens, you'll want to run one instance of
    xfdesktop per screen -- note that this is *not* XRandR-based
    multi-monitor, which xfdesktop handles natively in a single
    instance), and may have several `XfceWorkspace` objects associated
    with it based on the number of workspaces configured.
- /src/xfce-workspace.c
  + This represents a workspace on the system and contains a list of
    XfceBackdrops used to represent that workspace, usually one per
    monitor. It also sets the properties of the `XfceBackdrop` and
    handles the migration from the pre4.11 xfconf settings to the modern
    ones.
- /src/xfdesktop-icon-view.c
  + `XfdesktopIconView` is a relatively simple (at least it started out
    that way) icon view widget for the desktop.  It only supports a
    fixed grid of icons, which can be rearranged via drag-and-drop.  It
    does not support arbitrary positioning.  The grid resizes
    automatically if the screen size changes or if the icon/font size
    changes.  There are a bunch of annoying things in here to support
    arbitrary desktop icon types.  You should *never* assume that the
    user is viewing normal file icons here.  So the icon view can't know
    about files, or about dropping things onto icons, or what happens
    when a DnD event occurs from another application, etc.  The icon
    view doesn't even know where the icons come from or what they
    represent.  `XfdesktopIconView` implements `GtkCellLayout`, though
    in normal cases you don't need to touch that, as it will
    automatically add a pixbuf and text renderer to itself.  It figures
    out the icons to display based on a `GtkTreeModel`.
- /src/xfdesktop-icon.c
  + `XfdesktopIcon` is an abstract icon object.  You can add them to or
    remove them from an `XfdesktopIconView`.  You can set their
    positions, and figure out the icon's rectangular bounding box after
    it's been placed in the icon view.  The icon also reports what kinds
    of drag and drop actions it supports, and if it can display a
    tooltip.  It can also have its own special popup menu.  Note that
    this is no longer used for the window icon implementation, only for
    file icons.
- /src/xfdesktop-icon-view-manager.c
  + `XfdesktopIconViewManager` is an abstract controller for the icon
    view. It's responsible for creating the XfdesktopIcon objects and
    adding them to the XfdesktopIconView.  It handles drag and drop
    events for certain situations (like dropping data from another app
    onto an empty area of the desktop).  Most of the heavy lifting
    should go here.
- /src/xfdesktop-{file,window}-icon-model.c
  + These implement the `GtkTreeModel` classes used to back the
    `XfdesktopIconView`.  They provide icon pixbufs, label text,
    tooltips, and other things.  Since a lot of the code in the two is
    the same, they share a base class called `XfdesktopIconViewModel`.

TODO: document the actual window icon and file icon implementations.