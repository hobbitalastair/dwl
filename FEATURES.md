# dwl scrolling

- Windows are arranged between several logically independent workspaces.
- Each workspace holds windows in a long horizontal line.
- Workspaces are numbered.
- The user's viewport shows a particular part of the line of windows from one workspace.
- Multiple viewports can be used, one per output. No two viewports can display the same workspace at once to avoid displaying the same window two places. If a viewport switches to a workspace already shown on another output, the two outputs swap workspaces.
- Each viewport has a configurable width per workspace; windows are resized to fit n windows onto that viewport.
- Managed "floating" windows are not supported. All normal toplevel windows, including dialog toplevels, are treated as normal windows in the workspace line. Protocol popups remain attached to their parent surface and are not separate windows in the line.
- When an application launches a new window, it is placed to the right of the currently focused window. The first window is focused automatically. Later new windows are focused only if they use a valid activation token proving the user requested activation.
- The viewport follows focus, not window creation. New windows without activation tokens do not move the viewport. When a new or existing window is focused, the viewport is adjusted to include it.

## Movement with keybindings

- Meta+arrow changes the currently focused window, adjusting the viewport so that it is shown if necessary.
  * Up/down moves between workspaces, including empty workspaces.
  * Left/right moves left/right inside the current workspace.
- Meta+shift+arrow moves the currently focused window, adjusting the viewport so that it remains shown if necessary.
  * Up/down moves between workspaces.
  * Left/right moves left/right inside the current workspace.
- Meta+`+` and Meta+`-` changes the viewport width in the current workspace.

Each workspace remembers its own focused position. Moving left/right inside one workspace does not change the position remembered by any other workspace. When moving up/down to a workspace with fewer windows, focus or insertion uses that workspace's remembered position clamped to the nearest valid position. Closing a focused window focuses the window to its left if one exists, otherwise the window that moved into its position.

Assume that `[]` is the current viewport.
Here are some examples of a setup with three workspaces and windows a-f. The focused window uses an upper case letter.

From this starting point:
```
1. a[B]c
2.   de
3.   f
```

Examples of actions and the outcome:

Meta+down:

```
1. abc
2. [D]e
3.  f
```

Meta+right:

```
1. ab[C]
2.    de
3.    f
```

Meta+shift+left:

```
1. [B]ac
2.  de
3.  f
```

Meta+shift+down:

```
1.   ac
2. d[B]e
3.   f
```

New window g added:

```
1. a[B]gc
2.   de
3.   f
```

Window B closes:

```
1. [A]c
2.  de
3.  f
```

## Movement with the mouse

It is possible to click on either a window "." in the display or workspace ID.
This will switch to a different window/workspace.

## Focus highlighting

To highlight which window is focused, pressing Meta shall trigger an 30% opacity black overlay on the surrounding windows.
