# Over-the-air firmware updates (Zigbee OTA)

The firmware implements the **standard ZCL OTA Upgrade cluster (0x0019)** as a
client. The device advertises a manufacturer code, image type, and the version
it is currently running; the coordinator (zigbee2mqtt, Home Assistant ZHA,
deCONZ, …) acts as the OTA server and pushes a matching `.ota` image. Nothing in
this is vendor-specific — any compliant coordinator can update the device.

## How matching works

A coordinator only offers an image when **all** of these hold:

| Field             | Firmware source                       | Default        |
| ----------------- | ------------------------------------- | -------------- |
| Manufacturer code | `OTA_UPGRADE_MANUFACTURER` (`main/ota_driver.h`) | `0x131B` (4891)|
| Image type        | `OTA_UPGRADE_IMAGE_TYPE` (`main/ota_driver.h`)   | `0x1010` (4112)|
| File version      | `FW_VERSION_*` (`main/version.h`)     | `1.0.x` → `0x010000xx` |

The image's header manufacturer code + image type must equal the firmware's, and
its file version must be **greater** than the running version.

## Releasing an update

The version lives in exactly one place — `main/version.h` — and feeds the OTA
file version, the SW Build ID z2m shows, the ESP-IDF app version, and the `.ota`
the packaging script produces.

1. Bump `FW_VERSION_MAJOR/MINOR/PATCH` in `main/version.h` (e.g. `1.0.3` → `1.0.4`).
2. `idf.py build` — produces `build/light_switch.bin`.
3. Package the `.ota` (version/manufacturer/image-type are read from the headers,
   so no numbers to pass):

   ```
   python tools/make_ota.py --in build/light_switch.bin --out dimmer.ota
   ```

   The script prints the `fileVersion`, `fileSize`, and `sha512` to drop into the
   z2m index below.

## Serving it to zigbee2mqtt

Users install the device + auto-OTA by adding the external converter and the
hosted OTA index — see **[`../z2m/README.md`](../z2m/README.md)**. As a
maintainer you publish a release by pointing `make_ota.py` at `z2m/ota-index.json`
(it writes the image into `z2m/images/` and updates the index with the raw-GitHub
`url`), then commit + push to `main`; every user's z2m then discovers it.

For a quick local test without GitHub, you can instead point z2m's
`ota.zigbee_ota_override_index_location` at a local `ota-index.json` whose `url`
is a path z2m can read (in Docker, the in-container path), or use z2m's manual
"upload firmware" button to push a `.ota` directly.

## Rollback (auto-revert of a bad update)

App-rollback is enabled (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`). A freshly
OTA'd image boots in *pending-verify* state and is only confirmed once the device
**rejoins the network** (`ota_confirm_running_image()` from the steering-success
signal). If the new image instead crashes or wedges before rejoining, the task
watchdog reboots it and the bootloader reverts to the previous working image. So
an update that bricks Zigbee self-heals without a site visit.

Implication for releases: the new firmware must be able to join the network, or
it will be rolled back. A normally-booted (UART-flashed) image is never in
pending-verify, so this is a no-op outside of OTA.

## First-time / verification notes

- On first OTA, watch the `OTA` log tag: `download started`, periodic writes, then
  `OTA complete (... bytes). Rebooting`. After reboot the device rejoins, logs
  `New OTA image confirmed healthy`, and reports the new `fileVersion`.
- To test rollback: build an image that deliberately fails to join, OTA it, and
  confirm the device reverts to the previous version after the watchdog reboot.
- The two app slots are `ota_0`/`ota_1` (1.5 MB each) from the dual-OTA partition
  table; an image must fit in one slot.
