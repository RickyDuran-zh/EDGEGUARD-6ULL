#!/bin/sh
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
# If your Qt build supports it and you later enable touch, uncomment one of these:
# export QT_QPA_GENERIC_PLUGINS=evdevtouch:/dev/input/eventX
# export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS=/dev/input/eventX

./edgeguard-ui --status /tmp/edgeguard_status.json
