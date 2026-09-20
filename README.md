# Corne (nice!nano-compatible Pro Micro nRF52840) — ZMK Config

Split Corne keyboard running [ZMK](https://zmk.dev), targeting `nice_nano_v2` (the
Pro Micro nRF52840 board used here is a nice!nano-pinout-compatible clone).

- `config/west.yml` — ZMK manifest
- `config/corne.keymap` — keymap
- `config/corne.conf` — Kconfig options (RGB underglow enabled)
- `build.yaml` — build matrix (`nice_nano_v2` + `corne_left` / `corne_right`)
- `.zmk/` — local west workspace (gitignored, created by the steps below)

## Option A: GitHub Actions (no local setup)

Push this repo to GitHub. The workflow in `.github/workflows/build.yml` builds
both halves and uploads `.uf2` firmware files as build artifacts — nothing to
install locally.

## Option B: Build locally

These steps set up the native Zephyr/ZMK toolchain on Arch Linux (Omarchy).

### 1. System packages

```sh
sudo pacman -S --needed base-devel cmake ninja gperf ccache dfu-util \
  dtc wget python python-pip git unzip xz
```

### 2. Python virtualenv + west

```sh
cd /home/gpmontt/Documents/pro_micro52840_rev1940_2
python -m venv .zmk/.venv
source .zmk/.venv/bin/activate
pip install -U pip west
```

Activate this venv (`source .zmk/.venv/bin/activate`) in every new shell
before running `west` commands.

### 3. Fetch Zephyr + modules

The `.zmk/` workspace is already initialized (`west init` was run against
`config/west.yml`). Pull in Zephyr itself and all ZMK modules:

```sh
cd .zmk
west update
west zephyr-export
west packages pip --install
```

This downloads several hundred MB and can take a while.

### 4. Install the Zephyr SDK (arm-zephyr-eabi toolchain only)

```sh
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz
tar xf zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz
cd zephyr-sdk-0.16.8
./setup.sh -t arm-zephyr-eabi -h -c
```

`-c` registers udev rules (needed for flashing/debug probes); it will prompt
for `sudo`.

Set the SDK location for future shells (add to `~/.bashrc` / `~/.zshrc`):

```sh
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk-0.16.8"
```

### 5. Build

From the repo root, with the venv activated:

```sh
source .zmk/.venv/bin/activate
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk-0.16.8"
cd .zmk

west build -d build/left -b nice_nano_v2 -- \
  -DSHIELD=corne_left -DZMK_CONFIG="$(pwd)/../config"

west build -d build/right -b nice_nano_v2 -- \
  -DSHIELD=corne_right -DZMK_CONFIG="$(pwd)/../config"
```

Firmware output: `.zmk/build/left/zephyr/zmk.uf2` and
`.zmk/build/right/zephyr/zmk.uf2`.

#### Either half as the USB/host side

By default `corne_left` is central (the half you plug into USB / pair to your
computer) and `corne_right` is peripheral. USB is already ZMK's default
preferred output whenever the central half is plugged in — no extra config
needed for that.

To instead plug the **right** half into USB, build the reversed-role variants
(also produced automatically by `build.yaml` / GitHub Actions):

```sh
west build -d build/left-peripheral -b nice_nano_v2 -- \
  -DSHIELD=corne_left -DZMK_CONFIG="$(pwd)/../config" \
  -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n

west build -d build/right-central -b nice_nano_v2 -- \
  -DSHIELD=corne_right -DZMK_CONFIG="$(pwd)/../config" \
  -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=y
```

Flash `corne_left` + `corne_right` together (left plugged into USB), or
`corne_left_peripheral` + `corne_right_central` together (right plugged into
USB) — never mix a central build from one set with a peripheral build from
the other. After switching which set is flashed, re-pair both halves (flash
`settings_reset` to both first if pairing seems stuck).

To rebuild after keymap/config changes, add `-p` (pristine) if you change
board/shield, otherwise a plain `west build -d build/left` re-run picks up
`config/` edits.

### 6. Flash

1. Plug in one half via USB.
2. Double-tap the reset button on the Pro Micro nRF52840 — it mounts as a USB
   drive (e.g. `NICENANO` or similar).
3. Copy the matching `zmk.uf2` (left → left half, right → right half) onto
   that drive. The board reboots automatically with new firmware.
4. Repeat for the other half.

Pair each half over Bluetooth from your OS once both are flashed; the two
halves talk to each other automatically once paired.

### Resetting settings/bonds

`build.yaml` also builds a `settings_reset` firmware (no keymap, just wipes
stored Bluetooth bonds and persisted settings). Flash it the same way as
above to either half if pairing gets stuck or you need a clean slate, then
reflash that half with its normal `corne_left`/`corne_right` firmware.
