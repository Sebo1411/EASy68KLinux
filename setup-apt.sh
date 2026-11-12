#!/usr/bin/env bash
set -euo pipefail
echo "Configuring EASy68K Linux APT repository for automatic updates..."
echo "deb [trusted=yes arch=amd64] https://sebo1411.github.io/EASy68KLinux/apt ./" | sudo tee /etc/apt/sources.list.d/easy68k-linux.list >/dev/null
sudo apt-get update
sudo apt-get install -y easy68k-linux
echo "EASy68K Linux installed and configured for automatic APT updates!"
