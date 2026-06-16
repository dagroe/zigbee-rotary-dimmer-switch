# Over-the-air firmware updates (Zigbee OTA)

The firmware implements the **standard ZCL OTA Upgrade cluster (0x0019)** as a
client. The device advertises a manufacturer code, image type, and the version
it is currently running; the coordinator (zigbee2mqtt, Home Assistant ZHA,
deCONZ, …) acts as the OTA server and pushes a matching `.ota` image. Nothing in
this is vendor-specific — any compliant coordinator can update the device.

## How matching works

A coordinator only offers an image when **all** of these hold:

| Field             | Firmware source (`main/ota_driver.h`) | Default        |
| ----------------- | ------------------------------------- | -------------- |
| Manufacturer code | `OTA_UPGRADE_MANUFACTURER`            | `0x131B` (4891)|
| Image type        | `OTA_UPGRADE_IMAGE_TYPE`              | `0x1010` (4112)|
| File version      | `OTA_UPGRADE_FILE_VERSION`            | `0x01000000`   |

The image's header manufacturer code + image type must equal the firmware's, and
its file version must be **greater** than the running `OTA_UPGRADE_FILE_VERSION`.

## Releasing an update

1. Bump `OTA_UPGRADE_FILE_VERSION` in `main/ota_driver.h` (e.g. `0x01000000` →
   `0x01000001`).
2. Build normally: `idf.py build`. This produces `build/light_switch.bin`.
3. Wrap that `.bin` in a Zigbee `.ota` container with `tools/make_ota.py`:

   ```
   python tools/make_ota.py --in build/light_switch.bin --out dimmer_v2.ota \
       --manuf 0x131B --image-type 0x1010 --version 0x01000001
   ```

   The `--manuf`/`--image-type`/`--version` MUST match the `OTA_UPGRADE_*` defines
   you compiled into that same build. The script prints the `fileVersion`,
   `fileSize`, and `sha512` to drop into the z2m index below.

## Serving it from zigbee2mqtt

z2m needs a local OTA index that points at your `.ota`. The exact config key has
changed across z2m versions — check your version's OTA docs — but the shape is:

`configuration.yaml`:
```yaml
ota:
  zigbee_ota_override_index_location: my_ota_index.json
```

`my_ota_index.json` (decimal; use the values `make_ota.py` printed):
```json
[
  {
    "fileVersion": 16777217,
    "manufacturerCode": 4891,
    "imageType": 4112,
    "fileSize": 1100000,
    "sha512": "<from make_ota.py>",
    "url": "/opt/zigbee2mqtt/data/ota/dimmer_v2.ota"
  }
]
```

- `4891` = `0x131B`, `4112` = `0x1010`, `16777217` = `0x01000001`.
- Put `dimmer_v2.ota` at the `url` path; set `fileSize`/`sha512` from the script.
- Restart z2m, then in the device's page use **OTA → Check for update / Update**.
  (ZHA: enable the OTA provider / advanced OTA and trigger an update similarly.)

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
