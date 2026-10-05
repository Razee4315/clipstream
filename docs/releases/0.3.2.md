## ClipStream 0.3.2

A speed release, plus the popup now opens where you are typing.

### Faster everywhere
Switching filters, moving through the list, scrolling and searching were sluggish, and the list could briefly show stale clips after a click. On the test machine (125% display scaling) a filter click went from about 75-120 ms to about 10-20 ms with a small history, and from up to 950 ms to about 15-25 ms with megabyte-sized clips and full-screen screenshots in the history.

What changed:
- The popup shadow is drawn once instead of being re-blurred on every repaint.
- Timestamps no longer go through a time-zone conversion for every row.
- The list loads a short preview of each clip. The full clip is fetched when you paste, copy, preview or edit it.
- Screenshot thumbnails are decoded in the background, and captured screenshots are saved in the background.
- The first open after startup is prepared ahead of time.

### Opens at the text cursor
Like the Windows clipboard panel, the popup opens just below the text cursor. If the app does not report a cursor, it opens at the mouse pointer. Prefer the pointer always? Settings > General > Open popup.

Good to know: browsers and other Chromium-based apps only report the text cursor through their accessibility support. ClipStream asks for it and waits at most 70 ms. The first open in a browser window, or a very heavy page, may therefore still open at the mouse pointer.

### Also
- Every open starts on All clips with an empty search.

### Install
Download **ClipStream-Setup-0.3.2.exe** below and run it. Open ClipStream with **Ctrl+Shift+V**. Windows x64; no administrator access required.

Validated with a Windows release build, 16 automated regression tests, and 10 focus-and-paste tests that send real input to a native Windows edit control. Cursor positioning was checked against a native edit control and a Chromium window; other applications were not tested individually.
