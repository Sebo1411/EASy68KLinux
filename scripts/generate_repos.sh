#!/usr/bin/env bash
# =============================================================================
# EASy68K Linux - Automated APT & RPM Repository Generator for GitHub Pages
# Generates static APT (Debian/Ubuntu) and RPM (Fedora/RHEL/openSUSE) repos
# alongside one-line setup scripts (setup-apt.sh, setup-rpm.sh).
# =============================================================================
set -euo pipefail

DIST_DIR="${1:-dist}"
OUT_DIR="${2:-gh-pages-root}"
REPO_URL="${3:-https://sebo1411.github.io/EASy68KLinux}"
GITHUB_REPO="${4:-Sebo1411/EASy68KLinux}"

mkdir -p "${OUT_DIR}/apt" "${OUT_DIR}/rpm"

# Copy packages into repository directories
cp "${DIST_DIR}"/*.deb "${OUT_DIR}/apt/"
cp "${DIST_DIR}"/*.rpm "${OUT_DIR}/rpm/"

# 1. Generate APT Repository Metadata (Packages, Packages.gz, Release)
pushd "${OUT_DIR}/apt" >/dev/null
dpkg-scanpackages --multiversion . /dev/null > Packages
gzip -9c Packages > Packages.gz
if command -v apt-ftparchive >/dev/null 2>&1; then
    apt-ftparchive \
        -o APT::FTPArchive::Release::Origin="EASy68KLinux" \
        -o APT::FTPArchive::Release::Label="EASy68K Linux Native Official Repository" \
        -o APT::FTPArchive::Release::Suite="stable" \
        -o APT::FTPArchive::Release::Codename="stable" \
        -o APT::FTPArchive::Release::Architectures="amd64" \
        -o APT::FTPArchive::Release::Components="main" \
        release . > Release
fi
popd >/dev/null

# 2. Generate RPM Repository Metadata (repodata/repomd.xml)
if command -v createrepo_c >/dev/null 2>&1; then
    createrepo_c "${OUT_DIR}/rpm"
elif command -v createrepo >/dev/null 2>&1; then
    createrepo "${OUT_DIR}/rpm"
fi

# 3. Generate One-Line APT Setup Script (setup-apt.sh)
cat << EOF > "${OUT_DIR}/setup-apt.sh"
#!/usr/bin/env bash
set -euo pipefail
echo "Configuring EASy68K Linux APT repository for automatic updates..."
echo "deb [trusted=yes arch=amd64] ${REPO_URL}/apt ./" | sudo tee /etc/apt/sources.list.d/easy68k-linux.list >/dev/null
sudo apt-get update
sudo apt-get install -y easy68k-linux
echo "EASy68K Linux installed and configured for automatic APT updates!"
EOF
chmod +x "${OUT_DIR}/setup-apt.sh"

# 4. Generate One-Line RPM Setup Script (setup-rpm.sh)
cat << EOF > "${OUT_DIR}/setup-rpm.sh"
#!/usr/bin/env bash
set -euo pipefail
echo "Configuring EASy68K Linux RPM repository for automatic updates..."
cat << 'REPO_EOF' | sudo tee /etc/yum.repos.d/easy68k-linux.repo >/dev/null
[easy68k-linux]
name=EASy68K Linux Native Official Repository
baseurl=${REPO_URL}/rpm
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
EOF
chmod +x "${OUT_DIR}/setup-rpm.sh"

# 5. Generate Landing Page (index.html)
cat << EOF > "${OUT_DIR}/index.html"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>EASy68K Linux Native - Auto-Update Package Repository</title>
  <style>
    body { font-family: system-ui, -apple-system, sans-serif; max-width: 820px; margin: 2rem auto; padding: 0 1rem; background: #121417; color: #e2e8f0; line-height: 1.6; }
    h1, h2 { color: #60a5fa; }
    pre { background: #1e293b; padding: 1rem; border-radius: 6px; overflow-x: auto; border: 1px solid #334155; color: #f8fafc; }
    a { color: #38bdf8; }
  </style>
</head>
<body>
  <h1>EASy68K Linux Native Repository</h1>
  <p>Official automatic update repository for <a href="https://github.com/${GITHUB_REPO}">${GITHUB_REPO}</a>.</p>

  <h2>1. Debian / Ubuntu / Kubuntu / Linux Mint (APT)</h2>
  <pre><code>curl -fsSL ${REPO_URL}/setup-apt.sh | sudo bash</code></pre>

  <h2>2. Fedora / RHEL / openSUSE (DNF / RPM)</h2>
  <pre><code>curl -fsSL ${REPO_URL}/setup-rpm.sh | sudo bash</code></pre>

  <h2>3. AppImage Delta Updates (zsync) &amp; Direct Downloads</h2>
  <p>All <code>.AppImage</code> builds embed <code>AppImageUpdate</code> delta-update metadata and include an integrated GUI Auto-Updater under <b>Help &rarr; Check for Updates...</b>.</p>
  <p><a href="https://github.com/${GITHUB_REPO}/releases/latest">Download Latest Release Packages on GitHub</a></p>
</body>
</html>
EOF

touch "${OUT_DIR}/.nojekyll"
echo "Successfully generated APT and RPM auto-update repositories in ${OUT_DIR}"
