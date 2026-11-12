#!/usr/bin/env bash
set -euo pipefail
echo "Configuring EASy68K Linux RPM repository for automatic updates..."
cat << REPO_EOF | sudo tee /etc/yum.repos.d/easy68k-linux.repo >/dev/null
[easy68k-linux]
name=EASy68K Linux Native Official Repository
baseurl=https://sebo1411.github.io/EASy68KLinux/rpm
enabled=1
gpgcheck=0
REPO_EOF
if command -v dnf >/dev/null 2>&1; then
    sudo dnf makecache
    sudo dnf install -y easy68k-linux
elif command -v zypper >/dev/null 2>&1; then
    sudo zypper refresh
    sudo zypper install -y easy68k-linux
else
    sudo yum install -y easy68k-linux
fi
echo "EASy68K Linux installed and configured for automatic RPM updates!"
