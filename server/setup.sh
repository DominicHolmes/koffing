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

# 4. Create or load admin token (must happen before database creation so we have auth)
TOKEN_FILE="$SCRIPT_DIR/.influx_token"
if [ -f "$TOKEN_FILE" ]; then
  INFLUX_TOKEN=$(cat "$TOKEN_FILE")
  # Verify the token still works; if not, delete it and fall through to recreate
  if ! influxdb3 show tokens --token "$INFLUX_TOKEN" >/dev/null 2>&1; then
    echo "Saved token is no longer valid, creating a new one..."
    rm -f "$TOKEN_FILE"
    INFLUX_TOKEN=""
  else
    echo "Using existing API token from $TOKEN_FILE"
  fi
fi

if [ -z "${INFLUX_TOKEN:-}" ]; then
  echo "Creating API token..."
  TOKEN_OUTPUT=$(influxdb3 create token --admin 2>&1 || true)
  INFLUX_TOKEN=$(echo "$TOKEN_OUTPUT" | sed -n 's/.*Token: *//p' | head -1 | sed 's/\x1b\[[0-9;]*m//g' | tr -d '[:cntrl:]')

  if [ -z "$INFLUX_TOKEN" ]; then
    echo "Could not parse token. Output was:"
    echo "$TOKEN_OUTPUT"
    echo "You may need to set the token manually in telegraf.conf and grafana datasource."
    INFLUX_TOKEN="INFLUX_TOKEN_PLACEHOLDER"
  else
    echo "$INFLUX_TOKEN" > "$TOKEN_FILE"
    echo "Token created and saved to $TOKEN_FILE"
  fi
fi

# 5. Create database
influxdb3 create database koffing --token "$INFLUX_TOKEN" 2>/dev/null || echo "Database 'koffing' already exists."

# 6. Configure Telegraf
echo "Configuring Telegraf..."
sed "s|INFLUX_TOKEN_PLACEHOLDER|$INFLUX_TOKEN|g" \
  "$SCRIPT_DIR/telegraf/telegraf.conf" > "$PREFIX/etc/telegraf.conf"
brew services restart telegraf

# 7. Configure Grafana
echo "Configuring Grafana..."
GRAFANA_CONF="$PREFIX/etc/grafana/grafana.ini"
GRAFANA_PROV="$PREFIX/share/grafana/conf/provisioning"
GRAFANA_DASH="$PREFIX/var/lib/grafana/dashboards"
mkdir -p "$GRAFANA_PROV/datasources" "$GRAFANA_PROV/dashboards" "$GRAFANA_DASH"

# Append anonymous auth config (skip login)
if ! grep -q "auth.anonymous" "$GRAFANA_CONF" 2>/dev/null | grep -q "enabled = true"; then
  cat "$SCRIPT_DIR/grafana/grafana.ini" >> "$GRAFANA_CONF"
fi

sed "s|INFLUX_TOKEN_PLACEHOLDER|$INFLUX_TOKEN|g" \
  "$SCRIPT_DIR/grafana/provisioning/datasources.yaml" > "$GRAFANA_PROV/datasources/koffing.yaml"

sed "s|/var/lib/grafana/dashboards|$GRAFANA_DASH|g" \
  "$SCRIPT_DIR/grafana/provisioning/dashboards.yaml" > "$GRAFANA_PROV/dashboards/koffing.yaml"

cp "$SCRIPT_DIR/grafana/dashboards/koffing.json" "$GRAFANA_DASH/"
brew services restart grafana

echo ""
echo "=== Setup Complete ==="
echo ""
HOSTNAME=$(hostname)
echo "Grafana:  http://localhost:3000 or http://$HOSTNAME:3000"
echo "InfluxDB: http://localhost:8181 or http://$HOSTNAME:8181"
echo "MQTT:     localhost:1883 or $HOSTNAME:1883"
echo ""
echo "Next: set MQTT_SERVER in secrets.h to this machine's IP or hostname.local"
echo "Test MQTT: mosquitto_sub -t 'koffing/sensors'"
echo ""
echo "Tailscale (one-time setup for remote access):"
echo "  sudo tailscaled install-system-daemon"
echo "  tailscale up"
echo "  tailscale ip -4   # use this IP to access Grafana remotely"
