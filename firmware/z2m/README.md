# Using the dimmer with zigbee2mqtt (incl. OTA updates)

This folder is the zigbee2mqtt integration for the **DG Electronics ESP32-C6
rotary dimmer** (`ESP_DIMMER_1`):

| File | Purpose |
| ---- | ------- |
| `ESP_DIMMER_1.js` | External converter — makes z2m recognize the device and enables OTA. |
| `ota-index.json` | OTA index served to z2m so it auto-discovers firmware updates. |
| `images/*.ota` | Published firmware images referenced by the index. |

The device speaks the **standard ZCL OTA Upgrade cluster**, so this also works
with ZHA/deCONZ — they just consume `ota-index.json` differently.

## Install (zigbee2mqtt users)

1. **Add the external converter.** Download
   [`ESP_DIMMER_1.js`](https://raw.githubusercontent.com/dagroe/zigbee-rotary-dimmer-switch/main/firmware/z2m/ESP_DIMMER_1.js)
   into your z2m **data directory** (next to `configuration.yaml`), and reference it:

   ```yaml
   external_converters:
     - ESP_DIMMER_1.js
   ```

2. **Enable OTA from this repo.** Point z2m at the hosted index — no download
   needed, z2m fetches it:

   ```yaml
   ota:
     zigbee_ota_override_index_location: https://raw.githubusercontent.com/dagroe/zigbee-rotary-dimmer-switch/main/firmware/z2m/ota-index.json
   ```

3. **Restart z2m.** The device now shows as *DG Electronics / ESP_DIMMER_1*, and
   the device's **OTA** tab will offer an update whenever a newer image is
   published here. (z2m checks on a schedule and the device also asks hourly;
   you can also click **Check for update** to force it.)

That's it — future firmware updates arrive over the air, no reflashing.

## Releasing a new firmware (maintainers)

The version lives only in `firmware/main/version.h`. To cut a release:

```bash
cd firmware

# 1. bump FW_VERSION_* in main/version.h, then build
idf.py build

# 2. package the image AND update the index in one step
python tools/make_ota.py \
    --in build/light_switch.bin \
    --out z2m/images/dimmer-<MAJOR.MINOR.PATCH>.ota \
    --index z2m/ota-index.json

# 3. commit the new image + index and push to main
git add z2m/images/dimmer-<MAJOR.MINOR.PATCH>.ota z2m/ota-index.json
git commit -m "OTA: release firmware <MAJOR.MINOR.PATCH>"
git push origin main
```

`make_ota.py` reads the version/manufacturer/image-type from the firmware
headers, computes `fileSize`/`sha512`, and rewrites the matching `ota-index.json`
entry with the raw-GitHub `url` for the new image. Once pushed to `main`, every
user's z2m discovers it automatically.

> The raw URLs above point at the **`main`** branch, so the integration only
> works once these files are merged to `main`.

See [`../docs/OTA.md`](../docs/OTA.md) for how the OTA client, partitioning, and
rollback work on the firmware side.
