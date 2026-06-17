# Firmware TODO

Deferred robustness/maintainability items from the wall-install review.
Items are roughly in priority order.

## Smaller follow-ups

- [ ] **Re-add network-loss self-heal, safely.** A first attempt (re-steer on
  ESP_ZB_ZDO_SIGNAL_LEAVE / NO_ACTIVE_LINKS_LEFT) was reverted: it raced the
  stack's own commissioning/leave state machine — it broke initial joins
  (leave-with-rejoin during commissioning) and asserted in `zdo_app.c` during a
  coordinator-forced leave-reset while steering was retrying. The stack already
  auto-rejoins routers on transient loss. If re-added, prefer a clean
  `esp_restart()` on a RESET-type leave (let the device come up factory-new)
  rather than calling commissioning APIs from the leave handler, and test
  against a coordinator that actively removes the device.

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
- [x] Raise Zigbee task priority/stack
- [x] Keep GPIO ISRs in IRAM (no dropped edges during flash writes)
- [x] Debounce only the active button's interrupt
- [x] 4MB flash + dual-OTA partition layout
- [x] Standard Zigbee OTA Upgrade client (download/apply; see `docs/OTA.md`)
- [x] OTA rollback: image confirmed only after rejoining the network, else
      auto-reverts to the previous image
- [x] Single-source firmware version (`main/version.h`) feeding OTA version,
      SW Build ID, app version, and the .ota packaging tool
- [x] Suppress outbound commands when not joined; LED feedback on every command
      (white = sent, red = dropped/not joined)
