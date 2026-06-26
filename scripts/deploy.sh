#!/bin/sh
# deploy.sh — EdgeGuard one-click deploy on the imx6ull board
# Usage:  sudo sh deploy.sh [service ...]
#
#   No args        → deploy all
#   hubd           → sensor_hubd only
#   httpd          → edgeguard_httpd only
#   mqttd          → edgeguard_mqttd only
#   visiond        → edgeguard_visiond + edgeguard_visiond_face
#   ui             → edgeguard-ui only
#
#   Multiple OK:   sudo sh deploy.sh httpd ui
#
# Run on the target board after syncing from the VM:
#   Windows → sync_to_vm.sh → VM → make → cp to sharedir → board: sudo sh deploy.sh

SHAREDIR="/mnt"
APP_DIR="/imx6ull/app"
UI_DIR="/imx6ull/ui"

# Internal: define all known targets
# Format:  tag|svc_name|binary1 [binary2 ...]
ALL_TARGETS="
hubd|edgeguard|sensor_hubd
httpd|edgeguard-httpd|edgeguard_httpd
mqttd|edgeguard-mqttd|edgeguard_mqttd
visiond|edgeguard-visiond|edgeguard_visiond edgeguard_visiond_face
ui|edgeguard-ui|edgeguard-ui
"

# ---- resolve args → list of tags ----
if [ $# -eq 0 ]; then
    TAGS="hubd httpd mqttd visiond ui"
else
    TAGS=""
    for arg in "$@"; do
        case "$arg" in
            hubd|httpd|mqttd|visiond|ui) TAGS="$TAGS $arg" ;;
            *) echo "WARNING: unknown target '$arg' — skipped" ;;
        esac
    done
fi

if [ -z "$TAGS" ]; then
    echo "No valid targets specified."
    echo "Usage: sudo sh deploy.sh [hubd|httpd|mqttd|visiond|ui] ..."
    exit 1
fi

echo "============================================"
echo " EdgeGuard Deploy"
echo " Targets: $TAGS"
echo "============================================"
echo ""

# ---- Step 1: stop selected services ----
echo "[1/4] Stopping services..."
for tag in $TAGS; do
    svc=$(echo "$ALL_TARGETS" | awk -v t="$tag" -F'|' '$1==t{print $2}')
    if systemctl is-active --quiet "$svc" 2>/dev/null; then
        echo "       stopping $svc ..."
        sudo systemctl stop "$svc" || echo "       (ignored)"
    else
        echo "       $svc already stopped"
    fi
done
echo "       done."
echo ""

# ---- Step 2: verify sharedir ----
echo "[2/4] Checking sharedir..."
if [ ! -d "$SHAREDIR" ]; then
    echo "ERROR: $SHAREDIR not found — is the VM shared folder mounted?"
    exit 1
fi
echo "       $SHAREDIR OK"
echo ""

# ---- Step 3: install binaries ----
echo "[3/4] Installing binaries..."
sudo mkdir -p "$APP_DIR"
sudo mkdir -p "$UI_DIR"

for tag in $TAGS; do
    line=$(echo "$ALL_TARGETS" | awk -v t="$tag" -F'|' '$1==t{print $0}')
    svc=$(echo "$line" | cut -d'|' -f2)
    bins=$(echo "$line" | cut -d'|' -f3-)

    for bin in $bins; do
        src="$SHAREDIR/$bin"
        if [ "$tag" = "ui" ]; then
            dst="$UI_DIR/$bin"
        else
            dst="$APP_DIR/$bin"
        fi
        if [ -f "$src" ]; then
            sudo cp "$src" "$dst"
            echo "       $bin → $dst"
        else
            echo "       SKIP $bin (not found in sharedir)"
        fi
    done
done
echo "       done."
echo ""

# ---- Step 4: start services + cleanup ----
echo "[4/4] Restarting services + cleanup..."
sudo systemctl daemon-reload

for tag in $TAGS; do
    svc=$(echo "$ALL_TARGETS" | awk -v t="$tag" -F'|' '$1==t{print $2}')
    bins=$(echo "$ALL_TARGETS" | awk -v t="$tag" -F'|' '$1==t{print $3}')
    echo "       starting $svc ..."
    sudo systemctl restart "$svc" || echo "       WARNING: $svc failed to start"
    sleep 1
    # cleanup sharedir for this target
    for bin in $bins; do
        [ -f "$SHAREDIR/$bin" ] && sudo rm -f "$SHAREDIR/$bin" && echo "       cleaned $bin"
    done
done
echo "       done."
echo ""

# ---- Status summary ----
echo "============================================"
echo " Service Status"
echo "============================================"
for tag in $TAGS; do
    svc=$(echo "$ALL_TARGETS" | awk -v t="$tag" -F'|' '$1==t{print $2}')
    if systemctl is-active --quiet "$svc" 2>/dev/null; then
        echo "  ✅ $svc — active"
    else
        echo "  ❌ $svc — NOT active"
    fi
done
echo ""
echo "Deploy finished. Check logs with:"
echo "  journalctl -u edgeguard -f"
echo "  journalctl -u edgeguard-httpd -f"
echo "  journalctl -u edgeguard-visiond -f"
