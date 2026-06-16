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
3. Wrap that `.bin` in a Zigbee `.ota` container whose header carries the **same**
   manufacturer code / image type and the **new** version. Use the esp-zigbee-sdk
   image builder (run with `--help` to confirm flags for your version):

   ```
   python <esp-zigbee-sdk>/tools/image_builder/image_builder_tool.py \
       --create light_switch.ota \
       --manuf-id 0x131B \
       --image-type 0x1010 \
       --version 0x01000001 \
       --tag-id 0x0000 \
       build/light_switch.bin
   ```

   The header version here MUST match the `OTA_UPGRADE_FILE_VERSION` you compiled
   into that same build.

## Serving it from zigbee2mqtt

z2m needs a local OTA index that points at your `.ota`. The exact config key has
changed across z2m versions — check your version's OTA docs — but the shape is:

`configuration.yaml`:
```yaml
ota:
  zigbee_ota_override_index_location: my_ota_index.json
```

`my_ota_index.json` (numbers are decimal):
```json
[
  {
    "fileVersion": 16777217,
    "manufacturerCode": 4891,
    "imageType": 4112,
    "url": "file:///opt/zigbee2mqtt/data/ota/light_switch.ota"
  }
]
```

- `4891` = `0x131B`, `4112` = `0x1010`, `16777217` = `0x01000001`.
- Put `light_switch.ota` at the `url` path.
- Restart z2m, then in the device's page use **OTA → Check for update / Update**.
  (ZHA: enable the OTA provider / advanced OTA and trigger an update similarly.)

## First-time / verification notes

- The running image is marked valid on boot (`esp_ota_mark_app_valid_cancel_rollback`).
  App-rollback is **not yet enabled** in sdkconfig — see `firmware/TODO.md`. Enable
  it only together with a health check, or every boot will roll back.
- On first OTA, watch the `OTA` log tag: `download started`, periodic writes, then
  `OTA complete (... bytes). Rebooting`. After reboot the device rejoins and
  reports the new `fileVersion`.
- The two app slots are `ota_0`/`ota_1` (1.5 MB each) from the dual-OTA partition
  table; an image must fit in one slot.
