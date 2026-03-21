#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="$(brew --prefix)"
DATA_DIR="$HOME/.influxdb3_data"
LOG_DIR="$HOME/Library/Logs"
PLIST_PATH="$HOME/Library/LaunchAgents/com.koffing.influxdb3.plist"

echo "=== Koffing Server Setup ==="
echo ""

# 1. Install packages (Mosquitto, Telegraf, Grafana start via Brewfile; InfluxDB installed only)
echo "Installing packages..."
brew bundle --file="$SCRIPT_DIR/Brewfile"

# 2. Mosquitto config
echo "Configuring Mosquitto..."
cp "$SCRIPT_DIR/mosquitto/mosquitto.conf" "$PREFIX/etc/mosquitto/mosquitto.conf"
brew services restart mosquitto

# 3. Start InfluxDB 3 via launchd
echo "Configuring InfluxDB 3..."
mkdir -p "$DATA_DIR" "$LOG_DIR"

sed -e "s|INFLUXDB3_BIN_PLACEHOLDER|$PREFIX/bin/influxdb3|g" \
    -e "s|DATA_DIR_PLACEHOLDER|$DATA_DIR|g" \
    -e "s|LOG_DIR_PLACEHOLDER|$LOG_DIR|g" \
    "$SCRIPT_DIR/influxdb3.plist" > "$PLIST_PATH"

launchctl bootout gui/$(id -u) "$PLIST_PATH" 2>/dev/null || true
launchctl bootstrap gui/$(id -u) "$PLIST_PATH"
echo "InfluxDB 3 started (data: $DATA_DIR)"

# Wait for InfluxDB to be ready
echo -n "Waiting for InfluxDB..."
for i in $(seq 1 20); do
  if curl -s http://localhost:8181/health >/dev/null 2>&1; then
    echo " ready."
    break
  fi
  sleep 1
  echo -n "."
done

# 4. Create database
influxdb3 create database koffing 2>/dev/null || echo "Database 'koffing' already exists."

# 5. Create admin token
echo "Creating API token..."
TOKEN_OUTPUT=$(influxdb3 create token --admin 2>&1 || true)
INFLUX_TOKEN=$(echo "$TOKEN_OUTPUT" | sed -n 's/.*Token: *//p' | head -1)

if [ -z "$INFLUX_TOKEN" ]; then
  echo "Could not parse token. Output was:"
  echo "$TOKEN_OUTPUT"
  echo "You may need to set the token manually in telegraf.conf and grafana datasource."
  INFLUX_TOKEN="INFLUX_TOKEN_PLACEHOLDER"
else
  echo "Token created. Save this — it won't be shown again:"
  echo "  $INFLUX_TOKEN"
fi

# 6. Configure Telegraf
echo "Configuring Telegraf..."
sed "s|INFLUX_TOKEN_PLACEHOLDER|$INFLUX_TOKEN|g" \
  "$SCRIPT_DIR/telegraf/telegraf.conf" > "$PREFIX/etc/telegraf.conf"
brew services restart telegraf

# 7. Configure Grafana
echo "Configuring Grafana..."
GRAFANA_PROV="$PREFIX/share/grafana/conf/provisioning"
GRAFANA_DASH="$PREFIX/var/lib/grafana/dashboards"
mkdir -p "$GRAFANA_PROV/datasources" "$GRAFANA_PROV/dashboards" "$GRAFANA_DASH"

sed "s|INFLUX_TOKEN_PLACEHOLDER|$INFLUX_TOKEN|g" \
  "$SCRIPT_DIR/grafana/provisioning/datasources.yaml" > "$GRAFANA_PROV/datasources/koffing.yaml"

sed "s|/var/lib/grafana/dashboards|$GRAFANA_DASH|g" \
  "$SCRIPT_DIR/grafana/provisioning/dashboards.yaml" > "$GRAFANA_PROV/dashboards/koffing.yaml"

cp "$SCRIPT_DIR/grafana/dashboards/koffing.json" "$GRAFANA_DASH/"
brew services restart grafana

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Grafana:  http://localhost:3000  (admin / admin)"
echo "InfluxDB: http://localhost:8181"
echo "MQTT:     localhost:1883"
echo ""
echo "Next: set MQTT_SERVER in secrets.h to this machine's IP or hostname.local"
echo "Test MQTT: mosquitto_sub -t 'koffing/sensors'"
