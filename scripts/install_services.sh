#!/bin/sh
# Install EdgeGuard systemd services on the imx6ull board
# Run as root on the target board
#
# The script expects to be run from /imx6ull/scripts/ or any directory
# containing edgeguard.service and edgeguard-ui.service.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
echo "Installing EdgeGuard systemd services from: $SCRIPT_DIR"

# --- Copy service files ---
cp "$SCRIPT_DIR/edgeguard.service" /etc/systemd/system/
cp "$SCRIPT_DIR/edgeguard-ui.service" /etc/systemd/system/

# --- Create default config if missing ---
# (sensor_hubd also auto-creates this on first start, but we do it here
#  so the config is ready before the first launch.)
if [ ! -f /etc/edgeguard/config.json ]; then
    echo "Creating /etc/edgeguard/config.json ..."
    mkdir -p /etc/edgeguard
    cat > /etc/edgeguard/config.json <<'CEOF'
{
  "sample_interval_ms": 500,
  "als_low_threshold": 80,
  "ps_warning_threshold": 120,
  "ps_alarm_threshold": 220,
  "motion_warning_threshold": 8000,
  "motion_alarm_threshold": 15000,
  "buzzer_enable": true,
  "led_enable": true,
  "log_enable": true
}
CEOF
fi

# --- Reload systemd, enable and start services ---
systemctl daemon-reload
systemctl enable edgeguard.service
systemctl enable edgeguard-ui.service
systemctl restart edgeguard.service
sleep 1
systemctl restart edgeguard-ui.service

echo ""
echo "Done. Check status with:"
echo "  systemctl status edgeguard"
echo "  systemctl status edgeguard-ui"
echo "  journalctl -u edgeguard -f"
echo "  tail -f /var/log/edgeguard/alarm.log"
