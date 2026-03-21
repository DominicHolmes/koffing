#!/bin/bash
set -euo pipefail

PREFIX="$(brew --prefix)"
PLIST_PATH="$HOME/Library/LaunchAgents/com.koffing.influxdb3.plist"

echo "=== Koffing Server Teardown ==="
echo ""

# 1. Stop brew services
echo "Stopping services..."
brew services stop mosquitto 2>/dev/null || true
brew services stop telegraf 2>/dev/null || true
brew services stop grafana 2>/dev/null || true

# 2. Stop InfluxDB 3 (launchd)
if [ -f "$PLIST_PATH" ]; then
  launchctl bootout gui/$(id -u) "$PLIST_PATH" 2>/dev/null || true
  rm -f "$PLIST_PATH"
  echo "InfluxDB 3 stopped and plist removed."
else
  echo "InfluxDB 3 plist not found, skipping."
fi

# 3. Remove configs we copied into Homebrew directories
echo "Removing configs..."
rm -f "$PREFIX/share/grafana/conf/provisioning/datasources/koffing.yaml"
rm -f "$PREFIX/share/grafana/conf/provisioning/dashboards/koffing.yaml"
rm -f "$PREFIX/var/lib/grafana/dashboards/koffing.json"

echo ""
echo "=== Services stopped, configs removed ==="
echo ""
echo "Data is preserved in these locations:"
echo "  InfluxDB: ~/.influxdb3_data/"
echo "  Grafana:  $PREFIX/var/lib/grafana/"
echo ""
echo "To also delete all stored data:"
echo "  rm -rf ~/.influxdb3_data"
echo "  rm -f $PREFIX/var/lib/grafana/grafana.db"
echo ""
echo "To uninstall the packages entirely:"
echo "  brew uninstall mosquitto telegraf influxdb grafana"
