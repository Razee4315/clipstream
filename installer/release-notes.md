## ClipStream 0.3.1

A smaller popup that stays out of the way, and pasting that goes where you expect.

### What’s new
- **Paste into what you had selected.** Ctrl+Shift+V opens the popup without taking focus, so your app and selection stay put. Click a clip once, or press Enter, to replace the selection.
- **Compact popup.** 420 × 480 instead of 560 × 640, with tighter rows. Prefer the roomier layout? Choose Comfortable in Settings.
- **Clearer Settings.** General, Privacy, and History tabs. Changes save automatically.
- **Clear unpinned history** keeps your pinned clips and snippets. Deleting everything is a separate action.
- Clips that password managers mark as private are never saved.

### Fixes
- **No more wrong pastes.** If another program had the clipboard open at the wrong moment, ClipStream could paste whatever was on the clipboard before instead of the clip you picked. It now confirms the clip is on the clipboard first, retries briefly, and pastes nothing if it cannot.
- Searching or opening Settings no longer loses the paste destination.
- Paste waits until Ctrl/Shift/Alt are released and never sends to a different window. If it cannot paste, the clip stays copied and a tray notice tells you to press Ctrl+V.
- History limits now apply as clips arrive, not only at startup.
- A second copy of ClipStream can no longer start alongside the first.

### Good to know
- Typing goes to your app until you press Ctrl+F or click the search box.
- Windows does not let a normal app paste into one running as administrator. ClipStream leaves the clip copied and tells you.

### Install
Download **ClipStream-Setup-0.3.1.exe** below and run it. Open ClipStream with **Ctrl+Shift+V**. Windows x64; no administrator access required.

Validated with a Windows release build, 13 automated regression tests, and 9 focus-and-paste tests that send real input to a native Windows edit control. Other applications, including browsers, Office, and administrator windows, were not tested individually.
