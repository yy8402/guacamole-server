# Xorg Client Design

## Goals
- Minimize bandwidth/CPU by capturing only damaged regions.
- Keep latency low with damage coalescing and frame pacing.
- Keep changes isolated to the xorg protocol client.
- Maintain compatibility with guacd and the Guacamole protocol.

## Architecture Overview
The xorg client is a pull-based capture loop with damage-driven inputs. It uses
XDamage events to track changes, optional XShm to speed up capture, and XFixes
for cursor updates. All Xlib calls stay within the xorg client code path.

Core modules:
- capture.c/h: XDamage + XShm capture and fallbacks.
- display.c/h: Orchestrates the event loop, pacing, and display updates.
- scale.c/h: Scales and converts captured pixels into the Guac display buffer.
- cursor.c/h: Updates the Guac cursor layer from XFixes.
- input.c/h: Injects mouse/keyboard via XTest, with scaling.

## Data Flow
1) Initialize X11 display and root window.
2) Enable XDamage (if available) and XFixes (if available).
3) Main loop:
   - Drain X events (damage + cursor notify).
   - Update capture/output dimensions (root window size).
   - Coalesce damage for a short window.
   - Capture damaged region (XShm or XGetImage).
   - Scale/convert into Guac display buffer.
   - Update cursor if needed.
   - Flush the Guac display frame.

## Damage Handling
- XDamageReportNonEmpty is used to get bounding rectangles of change.
- Damages are unioned into a single rectangle per frame.
- A short coalescing window (default 12ms) batches rapid changes.
- When XDamage is unavailable, the client falls back to full-frame capture.

## Capture Paths
- XShm fast-path (MIT-SHM): XShmCreateImage + XShmGetImage.
- Fallback: XGetImage.
- XDamageSubtract is called before capture to avoid dropping new events during
  a slow capture.

## Scaling and Pixel Conversion
- Output size is the client-requested width/height.
- Capture size always tracks the root window size.
- Scaling uses precomputed x/y maps to avoid per-pixel division.
- Fast path: direct row copy when image format matches Guac buffer and no
  scaling is needed.

## Cursor Handling
- XFixes cursor events update the Guac cursor layer independently from the
  framebuffer updates.
- Cursor hotspot is preserved.
- If XFixes is unavailable, cursor updates are disabled (framebuffer still
  updates normally).

## Input Injection
- XTest is used for mouse and keyboard.
- Coordinates are scaled from output size to capture size.
- If XTest is unavailable, input is disabled for safety.

## Threading and Xlib Safety
- Capture runs in a single thread in display.c.
- XLockDisplay/XUnlockDisplay is used for all Xlib calls.
- Input handlers also use XLockDisplay/XUnlockDisplay.

## Frame Pacing and Backpressure
- FPS is configurable via args/env; default is 30 FPS.
- Frames are sent only when there is damage.
- Coalescing window reduces micro-updates; stale updates are skipped by
  capturing only the latest unioned rectangle.

## Configuration
- Connection args: display, width, height, fps.
- Env overrides: GUAC_XORG_DISPLAY, GUAC_XORG_WIDTH, GUAC_XORG_HEIGHT,
  GUAC_XORG_FPS, GUAC_XORG_CONFIG.
- Config file: /etc/guacamole/xorg.conf (key=value pairs).

## Compatibility and Scope
- All changes are isolated to src/protocols/xorg.
- No changes to shared libguac display internals.
- Other protocol agents (VNC/RDP/SSH/etc.) are unaffected.

## Known Limitations
- Scaling is nearest-neighbor (fast but low quality).
- No XInput2 support; XTest only.
- Capture is CPU-based; no GPU readback path yet.
