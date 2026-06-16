# Firmware TODO

Deferred robustness/maintainability items from the wall-install review.
Items are roughly in priority order.

## Needs design decision / larger change

- [ ] **Enable OTA rollback.** OTA download/apply is implemented (standard ZCL
  OTA Upgrade client — see `docs/OTA.md`); what remains is auto-revert of a bad
  image. Turn on `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` and gate the existing
  `esp_ota_mark_app_valid_cancel_rollback()` call (in `ota_client_start`) behind
  a real health check — ideally "marked valid only once the device has rejoined
  the network" — otherwise a device that boots but can't join will roll back.
  - [x] Confirmed module flash is 4MB; set `CONFIG_ESPTOOLPY_FLASHSIZE_4MB`.
  - [x] Dual-OTA partition layout (`ota_0`/`ota_1` 1.5MB each + `otadata`).
  - [x] OTA Upgrade client cluster + image download/apply (`ota_driver.c`).

## Smaller follow-ups

- [ ] **Suppress commands when not joined to a network.** The encoder/button
  paths call `esp_zb_zcl_*_cmd_req` regardless of join state. Cheap to guard
  with a "joined" flag (set on steering success, cleared on leave) — also lets
  the LED show a clear "not joined" state. Deferred because a wrong flag would
  make the device look dead; needs careful join/reboot-state handling.
- [ ] **Derive `sw_build_version` from the build** instead of the hardcoded
  date string in `device_config.c` (easy to forget to bump).
- [ ] **Handle LED-strip init failure more gracefully.** If
  `led_strip_new_rmt_device` fails, `led_task` is never started and nothing
  drains `led_evt_queue`; senders use timeout 0 so it is harmless, but the
  device then runs with no status LED.

## Done (wall-install review, this branch)

- [x] Move commission button off strapping pin GPIO9 → GPIO23 (board rewired + firmware)
- [x] Move relay driver off strapping pin GPIO15 → GPIO2 (board rewired; not yet driven by firmware)
- [x] Lock cross-task Zigbee stack calls (fixes vPortExitCritical freeze)
- [x] Reboot on critical init failure instead of hanging
- [x] Bound the Zigbee stack-lock wait so UI tasks can't wedge
- [x] Enable task-watchdog panic for self-recovery on hang
- [x] Self-heal on network loss (re-steer + LED indication)
- [x] Raise Zigbee task priority/stack
- [x] Keep GPIO ISRs in IRAM (no dropped edges during flash writes)
- [x] Debounce only the active button's interrupt
