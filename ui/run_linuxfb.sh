#!/bin/sh
# Run EdgeGuard UI on imx6ull
# Touch handling is built into the app (raw evdev), no tslib needed.

# --- Touch device discovery ---
find_touch_event() {
    if command -v devscan >/dev/null 2>&1; then
        local ev
        ev=$(devscan "goodix-ts" 2>/dev/null)
        [ -z "$ev" ] && ev=$(devscan "Goodix Capacitive TouchScreen" 2>/dev/null)
        [ -z "$ev" ] && ev=$(devscan "iMX6UL Touchscreen Controller" 2>/dev/null)
        [ -n "$ev" ] && echo "$ev" && return 0
    fi

    if [ -r /proc/bus/input/devices ]; then
        grep -A1 -i "goodix\|gt9157\|gt911" /proc/bus/input/devices 2>/dev/null | \
            grep "Handlers=" | grep -o 'event[0-9]*' | head -1
    fi
}

TOUCH_EVENT=$(find_touch_event)
if [ -n "$TOUCH_EVENT" ]; then
    echo "Touch device: /dev/input/$TOUCH_EVENT"
else
    echo "No touch device found"
fi

# --- Qt platform (framebuffer only, no tslib) ---
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
export QT_QPA_PLATFORM_PLUGIN_PATH=/usr/lib/plugins
export QT_QPA_FONTDIR=/usr/lib/fonts

echo "Starting edgeguard-ui..."
if [ -n "$TOUCH_EVENT" ]; then
    exec ./edgeguard-ui --status /tmp/edgeguard_status.json --touch "/dev/input/$TOUCH_EVENT"
else
    exec ./edgeguard-ui --status /tmp/edgeguard_status.json
fi
