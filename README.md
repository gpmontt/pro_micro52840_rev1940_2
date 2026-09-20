# Corne (nice!nano-compatible Pro Micro nRF52840) — ZMK Config

Split Corne keyboard running [ZMK](https://zmk.dev), targeting `nice_nano_v2` (the
Pro Micro nRF52840 board used here is a nice!nano-pinout-compatible clone).

- `config/west.yml` — ZMK manifest
- `config/corne.keymap` — keymap
- `config/corne.conf` — Kconfig options (RGB underglow enabled)
- `build.yaml` — build matrix (`nice_nano_v2` + `corne_left` / `corne_right`)
- `zephyr/module.yml` — declares this repo as a Zephyr module (custom
  `boards/`, plus the `rgb_layer_color` module below)
- `rgb_layer_color/` — custom module: overrides the underglow color while a
  configured layer is active (central-only, see gotcha below)
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
pip install -U pip west "setuptools<81"
```

(`setuptools<81` is required for ZMK Studio builds only: nanopb's protobuf
code generator imports `pkg_resources`, which newer `setuptools` releases
dropped. Building without Studio doesn't need this pin.)

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

This downloads several hundred MB and can take a while. It also fetches
`nanopb` and `zmk-studio-messages` (needed for ZMK Studio, see below) — if a
prior `west update` ever ran with `manifest.project-filter` excluding them
(check `west config manifest.project-filter`), clear it first with
`west config --delete manifest.project-filter` and re-run `west update`.

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
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
cd .zmk

west build -s zmk/app -d build/left -b nice_nano_v2 -S studio-rpc-usb-uart -- \
  -DSHIELD=corne_left -DZMK_CONFIG="$(pwd)/../config" \
  -DZMK_EXTRA_MODULES="$(pwd)/.." -DCONFIG_ZMK_STUDIO=y

west build -s zmk/app -d build/right -b nice_nano_v2 -- \
  -DSHIELD=corne_right -DZMK_CONFIG="$(pwd)/../config" \
  -DZMK_EXTRA_MODULES="$(pwd)/.."
```

The `-S studio-rpc-usb-uart` snippet and `-DCONFIG_ZMK_STUDIO=y` enable [ZMK
Studio](https://zmk.dev/docs/features/studio) — only needed on the **central**
build (`corne_left` by default; see below for the reversed-role case).

`ZMK_EXTRA_MODULES` points at the repo root, which has a `zephyr/module.yml`
pulling in `rgb_layer_color/` (the RGB-underglow-by-layer module). GitHub
Actions detects and adds this automatically; local builds need the flag
explicitly.

#### Known ZMK gotchas (already handled here, kept for reference)

- **Peripheral halves don't have keymap/layer state.** Layer tracking
  (`zmk_keymap_highest_layer_active()`, the `zmk_layer_state_changed` event)
  only exists in a split build's **central** role — `app/CMakeLists.txt`
  compiles that code exclusively for the central side. Any module that reads
  layer state (like `rgb_layer_color`) must gate itself with
  `depends on (!ZMK_SPLIT || (ZMK_SPLIT && ZMK_SPLIT_ROLE_CENTRAL))` in its
  Kconfig, the same guard ZMK core uses for `CONFIG_ZMK_USB`, or the
  peripheral build fails to link with undefined references. This is a
  recurring class of bug for split-aware modules — see
  [zmkfirmware/zmk#3456](https://github.com/zmkfirmware/zmk/pull/3456) for
  the same issue hitting HID-indicator/endpoint code.
- **A local module needs explicit wiring, even in the same repo.** Just
  having a module's own `zephyr/module.yml` inside a subdirectory doesn't
  make west/CMake discover it. The supported pattern for a zmk-config repo
  that also ships a local module is a `zephyr/module.yml` at the **repo
  root** (pointing at the module's `cmake`/`kconfig` paths) plus
  `-DZMK_EXTRA_MODULES=<repo root>` on the build command — GitHub Actions'
  `build-user-config.yml` auto-detects and adds this for you, but local
  builds must pass it explicitly (see step 5 above). Also give the module an
  explicit `name:` in `module.yml` — a relative extra-modules path (e.g.
  `$(pwd)/..`) otherwise gets used as the module name verbatim, and
  `build/<dir>/modules/..` filesystem-normalizes back to `build/<dir>`,
  causing a CMake binary-directory collision.

(`ZEPHYR_TOOLCHAIN_VARIANT` and `-s zmk/app` are both required — without them
CMake fails to find the SDK / the app source respectively.)

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
west build -s zmk/app -d build/left-peripheral -b nice_nano_v2 -- \
  -DSHIELD=corne_left -DZMK_CONFIG="$(pwd)/../config" \
  -DZMK_EXTRA_MODULES="$(pwd)/.." \
  -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n

west build -s zmk/app -d build/right-central -b nice_nano_v2 -S studio-rpc-usb-uart -- \
  -DSHIELD=corne_right -DZMK_CONFIG="$(pwd)/../config" \
  -DZMK_EXTRA_MODULES="$(pwd)/.." \
  -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=y -DCONFIG_ZMK_STUDIO=y
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

## ZMK Studio

[ZMK Studio](https://zmk.dev/docs/features/studio) lets you edit the keymap
at runtime (over USB) without reflashing. The `corne` shield already ships an
in-tree physical layout, so no board/shield changes were needed — only the
central build needs the `studio-rpc-usb-uart` snippet and
`CONFIG_ZMK_STUDIO=y` (see `build.yaml` / step 5 above).

- The keymap has a `&studio_unlock` binding on `lower_layer`, bottom-right key
  (under `'`) — hold `lower` and press it to unlock the keyboard for Studio
  edits.
- Connect at <https://zmk.studio/> (Chrome/Edge) or the [native
  app](https://zmk.studio/download), over USB, to the **central** half (the
  one plugged in). On Linux you may need to be in the `dialout` or `uucp`
  group to access the USB serial port.
- Once you've made changes in Studio, don't hand-edit `config/corne.keymap`
  again unless you first do "Restore Stock Settings" from the Studio UI —
  local keymap edits are ignored after Studio has taken over, with the
  exception of adding new empty (`status = "reserved";`) layers for Studio to
  use.
