# Firmware TODO

Deferred robustness/maintainability items from the wall-install review.
Items are roughly in priority order.

## Hardware-blocked (needs next PCB revision)

- [ ] **Move the commission button off GPIO9.** GPIO9 is an ESP32-C6
  strapping/boot pin: if it is held low at power-up the chip enters serial
  download mode and the application never starts. After a power outage a
  stuck/pressed commission button would leave the dimmer dark. Kept on GPIO9
  for now because boards are already fabricated; reassign to a non-strapping
  GPIO in the next hardware revision. (`device_config.h: GPIO_INPUT_COMMISSION_SWITCH`)

## Needs design decision / larger change

- [ ] **Add OTA firmware update.** The partition table has a single `factory`
  app and no OTA slots, and flash is configured as 2MB
  (`CONFIG_ESPTOOLPY_FLASHSIZE_2MB`). Once installed in a wall the only way to
  update is to physically extract the unit and reflash over UART.
  - First confirm the actual module flash size (C6 modules are commonly 4MB —
    fix the flash-size config if so).
  - Move to a dual-OTA partition layout (`ota_0`/`ota_1` + `otadata`).
  - Add an update path (Zigbee OTA cluster preferred).
  - Enable `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` so a bad image auto-reverts.

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

- [x] Lock cross-task Zigbee stack calls (fixes vPortExitCritical freeze)
- [x] Reboot on critical init failure instead of hanging
- [x] Bound the Zigbee stack-lock wait so UI tasks can't wedge
- [x] Enable task-watchdog panic for self-recovery on hang
- [x] Self-heal on network loss (re-steer + LED indication)
- [x] Raise Zigbee task priority/stack
- [x] Keep GPIO ISRs in IRAM (no dropped edges during flash writes)
- [x] Debounce only the active button's interrupt
