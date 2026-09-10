# Build & Workflow Guidelines

## Roles
- **miquland**: Wayland window manager.
- **miqutoolkit**: Application development toolkit / shared library.
- **miqulauncher**: Application launcher which also serves as dmenu, workspace, window switcher and binary launcher built with `miqutoolkit`.
- **miqubg**: Independent wallpaper daemon built with `miqutoolkit`.
- **miqudesk**: Interactive desktop canvas & widget layer built with `miqutoolkit`.
- **miqulock**: Session lock utility.
- **miquidle**: Wayland idle management daemon.

## Coding rules
Whenever you code in an higher level e.g. for miqulauncher which uses a lower level toolkit miqutoolkit, we should strictly maintain the boundaries e.g. 
miqulauncher should not be bothered about pango, cairo because it is the job of toolkit. Another example is that if you are working on miqumusic which uses UI components provided by miqutoolkit, it we need a seekbar, we should not make seekbar inside miqumusic, rather we should make seekbar in the toolkit itself and use it in miqumusic. But, whenever you want to edit or create or change something in toolkit in order to fix something in the launcher etc, ask me for permission. But you should know that we are free to edit everywhere, even in the compositor if it is lacking something it should have provided.
## Build and Install Protocol
1. **Never run sudo**: The agent cannot execute commands with `sudo`.
2. **Local Test Builds Only**:
   - For checking compilation and build errors, always build into the local `build/` directory without sudo (`./make.sh`).
   - Binaries and libraries remain solely inside `build/` for validation.
3. **Strictly System-Wide Installation (`/usr`)**:
   - **NEVER** install, copy, or symlink binaries into `~/.local/bin` or user directories.
   - All components (`miquland`, `miqutoolkit`, `miqulauncher`, `miqulock`, `miquidle`) must strictly install system-wide to `/usr` (`/usr/bin`, `/usr/lib`, `/usr/include`) via `sudo ./make.sh`.
4. **Never build `miqulauncher` immediately after `miqutoolkit`**:
   - `miqulauncher` depends on the system-wide installed `miqutoolkit` in `/usr`.
   - After compiling changes in `miqutoolkit`, **STOP** immediately.
   - Prompt the user to install `miqutoolkit` system-wide:
     ```bash
     sudo ./make.sh
     ```
   - Do **NOT** proceed to building or testing `miqulauncher` until the user confirms the system installation has finished.
   - The same is applicable for all other projects like miqudesk which depends on "miqutoolkit".
   5. **Config file edit**
   - Anytime you edit the config file in /assets/***.conf, you must ask me whether to update the usr ~/.config/**/**conf too or not and vice e versa. Goal is to sync but sometimes sync is not necessary, so better ask.
   6. GIT COMMIT AND PUSH should be always prompted if it is git repo. I may deny commit and push sometimes.
   7. Some consequences of code are visual, and may be hard for you to detect e.g. If I ask to make an UI red, you might write the code and verify, but verification for you is tough usually becasue you will be then needed to screenshot and check the color of it. For such kind of harder job, you better just ask me "Is it showing red?". 
## TODO 
- 1. **`wlr_xdg_activation_v1`**: Enables token-based activation/focus passing (e.g. clicking links in external apps).
- 2. **`wlr_cursor_shape_v1`**: Standardized Wayland cursor-shape-v1 support.
- 3. **`wlr_relative_pointer_v1` & `wlr_pointer_constraints_v1`**: Cursor trapping and relative motion for games & 3D apps.
- 4. miqubar, miquidle, miqushot, miqusunset (gamma is working fine though) are the natural next project.
- 5. miqulauncher will ultimately have to look like gnome overview e.g. taking screenshot of workspaces and windows and presenting them in grid. This is distant future.
- 6. miqulauncher immediate implementation is to support images grid. Right now we have not implemented it. Since, it used miqutoolkit which already has imageview, i don't think it will be hard. Moreover, the grid alreasdy supports images when rendering icon as this also uses the same toolkit api of image view.
- 7. Some folders are not git repo yet. It is imminent TODO.

