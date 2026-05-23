# EdgeGuard UI Qt Demo

This is a Qt Widgets multi-page UI for the EdgeGuard-6ULL project.
It is designed for an 800x480 RGB LCD running through Linux framebuffer (`/dev/fb0`).

## Pages

1. Dashboard
2. Sensor Data
3. Alarm Center
4. Settings Preview
5. Network

By default, the Dashboard page is shown. Without touch input, you can still test other pages using `--page N` or a keyboard with keys 1-5.

## Build on the board

```sh
qmake EdgeGuardUI.pro
make
```

## Build with cross Qt

Use the qmake from your ARM Qt toolchain, for example:

```sh
/path/to/arm-qt/bin/qmake EdgeGuardUI.pro
make
```

## Run on framebuffer

```sh
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
./edgeguard-ui --demo
```

Or read real status from `sensor_hubd`:

```sh
cp status_sample.json /tmp/edgeguard_status.json
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
./edgeguard-ui --status /tmp/edgeguard_status.json
```

## Page test without touch

```sh
./edgeguard-ui --demo --page 0   # Dashboard
./edgeguard-ui --demo --page 1   # Sensors
./edgeguard-ui --demo --page 2   # Alarms
./edgeguard-ui --demo --page 3   # Settings
./edgeguard-ui --demo --page 4   # Network
```

If a keyboard is available, press 1-5 to switch pages and Esc to exit.
If running from SSH on linuxfb, stop it from another SSH terminal using `pkill edgeguard-ui`.

## Expected JSON status format

See `status_sample.json`.
