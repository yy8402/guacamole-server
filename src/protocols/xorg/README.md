# Xorg Protocol Client (Initial)

This protocol module provides an Xorg capture client for guacd. It connects to
an X11 display, uses damage-based updates (with MIT-SHM if available), and
sends updates over the Guacamole protocol at a target FPS.

## Args
- `display`: X11 display string (default: use `DISPLAY` env)
- `width`: capture width (0 uses full screen)
- `height`: capture height (0 uses full screen)
- `fps`: target frames per second (default: 30)

## Env and config fallback
If a connection argument is not provided, the module will fall back to:
1) Environment variables: `GUAC_XORG_DISPLAY`, `GUAC_XORG_WIDTH`,
   `GUAC_XORG_HEIGHT`, `GUAC_XORG_FPS`, `GUAC_XORG_DISABLE_XSHM`
2) Config file: `$GUAC_XORG_CONFIG` or `/etc/guacamole/xorg.conf`

Config file format is simple `key=value` lines, for example:

```ini
display=:0
width=1920
height=1080
fps=30
```

## Build and run (xorg-only)
Build guacd with only this protocol enabled:

```sh
./configure \
  --with-xorg-client \
  --with-vnc=no \
  --with-rdp=no \
  --with-ssh=no \
  --with-telnet=no \
  --with-kubernetes=no \
  --with-xorg=no
make
make install
```

Start guacd:

```sh
guacd -f -L info
```

Set the X display either via the connection parameter or the `DISPLAY` env:
- Connection parameter: `display=:0`
- Environment fallback: `DISPLAY=:0 guacd -f -L info`

## Notes
- MIT-SHM is used when possible; set `GUAC_XORG_DISABLE_XSHM=1` to force
  fallback to `XGetImage` (useful if MIT-SHM is blocked and causes BadAccess).
