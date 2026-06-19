# Build Environment — ZAKO Distribution

Host machine setup for building a ZAKO distribution on AOSP. These requirements are the same regardless of target device.

---

## Host Requirements

**Recommended host:** Ubuntu 22.04 LTS or Ubuntu 20.04 LTS (x86-64)

Minimum hardware:
- CPU: 8 cores (16+ recommended for acceptable build times)
- RAM: 32GB (16GB absolute minimum — expect swap thrashing)
- Disk: 300GB free (AOSP + build artifacts + repo history)
- Network: Broadband for initial sync (~60–80GB download)

macOS users: AOSP builds require Linux. Options:
- Linux VM (Parallels/VMware Fusion) — allocate 16–32GB RAM, 300GB disk
- Dedicated Linux machine
- Remote Linux build server with SSHFS mount for local editing

---

## Package Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
  git-core gnupg flex bison build-essential zip curl \
  zlib1g-dev libc6-dev-i386 lib32z1-dev libgl1-mesa-dev \
  libxml2-utils xsltproc unzip fontconfig squashfs-tools \
  openjdk-11-jdk python3 python3-pip ninja-build \
  libssl-dev libelf-dev bc \
  android-tools-adb android-tools-fastboot \
  device-tree-compiler \
  ccache
```

Enable ccache to dramatically speed up incremental builds:
```bash
export USE_CCACHE=1
export CCACHE_EXEC=/usr/bin/ccache
ccache -M 50G    # 50GB cache — adjust to available disk
```

Add to `~/.bashrc` to persist across sessions.

---

## repo Tool

```bash
mkdir -p ~/.bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/.bin/repo
chmod a+x ~/.bin/repo
echo 'export PATH=$HOME/.bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

---

## Git Configuration

```bash
git config --global user.email "builder@[your-domain]"
git config --global user.name "ZAKO Build"
git config --global color.ui false   # disable color in build logs
```

---

## ADB/Fastboot Udev Rules (Linux)

Required so ADB and fastboot can access Android devices without sudo:

```bash
# Generic Android device rule:
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="18d1", MODE="0666", GROUP="plugdev"' \
  | sudo tee /etc/udev/rules.d/51-android.rules

# Add your user to plugdev:
sudo usermod -aG plugdev $USER

# Reload rules (or reboot):
sudo udevadm control --reload-rules
```

Log out and back in for group membership to take effect. Verify with:
```bash
adb devices
```

---

## Syncing AOSP

```bash
mkdir ~/aosp && cd ~/aosp

# Initialize with the correct branch for this distribution:
repo init \
  -u https://android.googlesource.com/platform/manifest \
  -b [AOSP_BRANCH] \
  --depth=1

# Sync (--no-tags reduces download size, -c fetches current branch only):
repo sync -qcj8 --no-tags
```

This takes 1–3 hours depending on network speed. The `--depth=1` flag fetches shallow clones (sufficient for building; use without `--depth=1` if you need full git history for bisect).

---

## Adding the Distribution Local Manifest

After AOSP sync, add the distribution-specific repositories:

```bash
mkdir -p .repo/local_manifests
```

Create `.repo/local_manifests/[distro_codename].xml`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <!-- Device tree -->
  <project name="[org]/android_device_[DEVICE_VENDOR]_[DEVICE_CODENAME]"
           path="device/[DEVICE_VENDOR]/[DEVICE_CODENAME]"
           remote="github"
           revision="zako-[android_version]" />

  <!-- Kernel -->
  <project name="[org]/android_kernel_[DEVICE_VENDOR]_[DEVICE_CODENAME]"
           path="kernel/[DEVICE_VENDOR]/[DEVICE_CODENAME]"
           remote="github"
           revision="zako-[kernel_version]" />
</manifest>
```

Then sync the new repos:
```bash
repo sync -qcj8 device/[DEVICE_VENDOR]/[DEVICE_CODENAME] \
                kernel/[DEVICE_VENDOR]/[DEVICE_CODENAME]
```

---

## First Build (Smoke Test)

```bash
cd ~/aosp
source build/envsetup.sh
lunch [DISTRO_CODENAME]_[DEVICE_CODENAME]-userdebug

# Quick smoke test — kernel only:
m bootimage -j$(nproc)

# Full build:
m -j$(nproc) 2>&1 | tee build.log
```

A full build takes 2–6 hours on first run; incremental builds with ccache are 15–45 minutes depending on what changed.

Build output lives at:
```
out/target/product/[DEVICE_CODENAME]/
├── boot.img
├── system.img
├── vendor.img
├── product.img
├── recovery.img
├── dtbo.img
└── vbmeta.img
```

---

## Docker Build Environment (Alternative)

For reproducible builds across machines, use the `docker-aosp` container:

```bash
# If docker-aosp is present in repos/tools/:
cd MobileOS/[DistroFolder]/repos/tools/docker-aosp

# Build the container:
docker build -t zako-aosp-builder .

# Mount AOSP source and run build:
docker run --rm \
  -v ~/aosp:/aosp \
  -v ~/.ccache:/ccache \
  -e USE_CCACHE=1 \
  -e CCACHE_DIR=/ccache \
  zako-aosp-builder \
  bash -c "cd /aosp && source build/envsetup.sh && \
           lunch [DISTRO_CODENAME]_[DEVICE_CODENAME]-user && \
           m -j$(nproc)"
```

Docker builds are slower on first run but guarantee a consistent environment. Recommended for release builds.

---

## Common Build Failures

| Error | Cause | Fix |
|---|---|---|
| `JAVA_HOME not set` | Java not installed or wrong version | `sudo apt-get install openjdk-11-jdk` |
| `Out of memory` | <16GB RAM or no swap | Add swap: `fallocate -l 32G /swapfile && mkswap /swapfile && swapon /swapfile` |
| `No space left on device` | Build artifacts fill disk | Free space; or move build dir to larger volume |
| `repo: command not found` | PATH not updated | `source ~/.bashrc` or open new terminal |
| `fatal: manifest does not have a project` | Local manifest path wrong | Check `.repo/local_manifests/` XML project paths |
