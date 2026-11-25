# Min / Max Brightness Gestures Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Double-click on the encoder button turns the bound lamp on at full brightness; hold-then-release turns it on at minimum.

**Architecture:** Both gestures are decided inside the existing `handle_encoder_button()` from the events the button driver already emits. Double-click is detected without a wait: the first click toggles at once, a second short release inside the window sends the preset. No change to `switch_driver`.

**Tech Stack:** ESP-IDF 6.1.1, esp-zigbee-lib 1.6.8, ESP32-C6, C.

**Spec:** `docs/superpowers/specs/2026-11-25-min-max-brightness-gestures-design.md`

## Global Constraints

- Only `firmware/main/esp_zb_switch.c` changes.
- No added delay on the ordinary single click.
- Push-and-rotate (colour temperature) must behave exactly as before: any rotation during a press suppresses the release action.
- Commands are sent through the binding table (`ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT`) with the existing `ZB_SEND_CMD` macro.
- `DEBUG_ENABLED` in `device_config.h` stays commented out in commits (it is enabled only in the local working tree).
- The project has no host-side test harness; verification is a clean build plus the bench checklist in Task 1.

## Review Focus

- Triple click: third click must be a plain toggle, not a second "max".
- Two single clicks more than 400 ms apart: two toggles, no "max".
- Hold with rotation started after 0.5 s: no "min" on release, no toggle.
- Hold longer than 5 s without rotation: "min" on release.
- Tick wrap-around / first click after boot: `last_click_tick == 0` must not be read as "a click just happened".

---

### Task 1: Gestures in `handle_encoder_button`

**Files:**
- Modify: `firmware/main/esp_zb_switch.c` (`encoder_ctrl_t`, the tuning `#define` block, `handle_encoder_button`)

**Interfaces:**
- Consumes: `esp_zb_zcl_move_to_level_cmd_t { zcl_basic_cmd, address_mode, uint8_t level, uint16_t transition_time }` and `uint8_t esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(esp_zb_zcl_move_to_level_cmd_t *)` from esp-zigbee-lib; `ZB_SEND_CMD(stmt)`; `led_evt_queue`; `LED_COLOR_STATE_BLINK_ONCE_WHITE`.
- Produces: nothing used elsewhere.

- [ ] **Step 1: Branch**

`git checkout -b min-max-gestures` from `main`.

- [ ] **Step 2: Add constants next to the other tuning defines**

```c
#define BUTTON_DOUBLE_CLICK_MS 400      /* second click released within this of the first = full brightness */
#define LEVEL_PRESET_MAX 254
#define LEVEL_PRESET_MIN 1              /* bulb clamps to its own minimum */
#define LEVEL_PRESET_TRANSITION_DS 5    /* 0.5 s */
```

- [ ] **Step 3: Add `TickType_t last_click_tick;` to `encoder_ctrl_t`**

Value 0 means "no recent click". Only the button task reads and writes it, so it needs no mux.

- [ ] **Step 4: Add `static void send_level_preset(encoder_ctrl_t *enc, uint8_t level)`**

Builds an `esp_zb_zcl_move_to_level_cmd_t` (source endpoint `enc->src_endpoint`, binding-table address mode, `transition_time = LEVEL_PRESET_TRANSITION_DS`) and sends it with `ZB_SEND_CMD(esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(&cmd))`. Place it above `handle_encoder_button`, below the `TAG` definition.

- [ ] **Step 5: `SWITCH_RELEASE_DETECTED`, not rotated**

If `last_click_tick != 0` and `now - last_click_tick <= pdMS_TO_TICKS(BUTTON_DOUBLE_CLICK_MS)`: call `send_level_preset(enc, LEVEL_PRESET_MAX)` and set `last_click_tick = 0`. Otherwise send Toggle as today and set `last_click_tick = now`, substituting 1 if `now` is 0. A rotated release leaves `last_click_tick` at 0.

- [ ] **Step 6: `SWITCH_LONG_PRESS_DETECTED`**

If `rotated_while_pressed` is false (read under `enc->mux`), queue `LED_COLOR_STATE_BLINK_ONCE_WHITE` to `led_evt_queue` as the release cue. Set `last_click_tick = 0`.

- [ ] **Step 7: `SWITCH_LONG_RELEASE_DETECTED` / `SWITCH_HOLD_RELEASE_DETECTED`, not rotated**

Replace the Off command with `send_level_preset(enc, LEVEL_PRESET_MIN)`. Update the comments and debug log strings that say "off".

- [ ] **Step 8: Update the comment above `handle_encoder_button`**

It currently says "short press = toggle, hold = off"; make it: short press = toggle, double-click = full brightness, hold then release = minimum, all suppressed by rotation.

- [ ] **Step 9: Build**

Run: `cd firmware && source ~/.espressif/tools/activate_idf_v6.1.1.sh && idf.py build`
Expected: `Project build complete`, no new warnings in `esp_zb_switch.c`.

- [ ] **Step 10: Bench check (needs the person at the bench)**

Flash with `idf.py -p <port> flash monitor`, then with the Aqara T2 bound:

| Start state | Gesture | Expected |
|---|---|---|
| off | single click | on at last level, instantly |
| on | single click | off |
| off | double-click | on, then full brightness |
| on, dim | double-click | brief dip, then full brightness |
| off | hold 1 s, release | LED blinks at 0.5 s; on at minimum after release |
| on | hold 1 s, release | fades to minimum |
| on | press, wait 1 s, rotate, release | colour temperature changes; brightness unchanged; no toggle |
| off | triple click | on, full, then off |
| on | hold 6 s, release | minimum |

- [ ] **Step 11: Commit**

Stage only `firmware/main/esp_zb_switch.c` and the two docs under `docs/superpowers/`. Message: `Add double-click = full and hold = minimum brightness gestures`.
