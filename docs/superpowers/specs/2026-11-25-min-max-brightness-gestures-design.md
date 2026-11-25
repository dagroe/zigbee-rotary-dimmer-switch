# Min / max brightness gestures on the encoder button

## Intent

Walking into a room, get the right light with one gesture:

- cooking in the daytime: double-click, lamp goes to full brightness;
- at night: press and hold, lamp goes to its minimum.

Both gestures act the same whether the lamp is on or off.

## Behaviour

| Gesture | Result |
|---|---|
| Single click | Toggle, sent `BUTTON_DOUBLE_CLICK_MS` (400 ms) after release |
| Double-click | Move to Level with On/Off, level 254; no toggle first |
| Third and later rapid clicks | Ignored |
| Hold 0.5 s | Move to Level with On/Off, level 2, sent while still held |
| Release after a hold | Nothing |
| Press, rotate | Colour temperature; the click is suppressed |

Applies to both encoders (on-board EP1, external EP3). The plain external wall switch (EP4), relay button and commission button are unchanged.

## Design

All in `firmware/main/esp_zb_switch.c`.

- `click_task` (one per encoder) resolves single versus double click. Each short, non-rotated release notifies it; it waits `BUTTON_DOUBLE_CLICK_MS` for a second notification. None: Toggle. One: full brightness, then swallow further clicks until the series pauses.
- `handle_encoder_button` sends the minimum preset on `SWITCH_LONG_PRESS_DETECTED` unless the encoder was already rotated during this press.
- `send_level_preset` sends Move to Level with On/Off through the binding table, 0.5 s transition.

## Trade-offs accepted

- Every single click is 400 ms slower, so that a double-click never toggles the lamp first.
- Push-and-rotate must start within 0.5 s of the press; a later rotation happens after the lamp has already gone to minimum.
- The minimum preset is level 2, not 1: a "with On/Off" move down to level 1 turns the Aqara T2 off (ZCL behaviour for reaching the minimum level).
- "Hold = off" is removed. Off is the single click.
- The switch does not track or restore the previous brightness.

## Verification

No host-side test harness exists. Verified by building for ESP32-C6 and on the bench with an Aqara T2 bulb bound through ZHA: single, double and triple click, hold, and push-and-rotate, from both on and off.
