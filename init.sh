#!/usr/bin/env bash

set -e

gcc -std=c11 -O2 -o pomo pomo.c

PREFIX="/opt/homebrew/bin"
sudo mkdir -p "$PREFIX"
sudo mv pomo "$PREFIX/pomo"
sudo chmod +x "$PREFIX/pomo"

CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/pomo"
CONFIG_FILE="$CONFIG_DIR/config"
mkdir -p "$CONFIG_DIR"
if [[ ! -f "$CONFIG_FILE" ]]; then
  cat > "$CONFIG_FILE" <<EOF
COLOR1=#FFFFFF
COLOR2=#00CCFF
EOF
  echo "Created default config at $CONFIG_FILE"
fi

echo "Installed ‘pomo’ to $PREFIX/pomo"
