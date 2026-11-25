# Min / max brightness gestures on the encoder button

## Intent

Walking into a room with the lamp off, get the right light with one gesture:

- cooking in the daytime: double-click, lamp comes on at full brightness;
- at night: press and hold, lamp comes on at its minimum.

## Goals

- Double-click sends "full brightness, turn on if off".
- Hold (0.5 s or longer) then release sends "minimum brightness, turn on if off".
- A plain single click stays instant (toggle, as today).
- Push-and-rotate (colour temperature) keeps working unchanged.
- Both encoders (on-board EP1, external EP3) get the gestures; they share `handle_encoder_button`.

## Non-goals

- Restoring the previous brightness after max/min (the bulb remembers its last level; the switch does not track it).
- Making the gestures configurable from Zigbee/Home Assistant.
- Gestures for the plain external wall switch (EP4), the relay button, or the commission button.

## Anti-goals

- Any added delay on the ordinary on/off click.
- A slow press-then-rotate dropping the lamp to minimum.

## Approaches considered

**Hold timing**

- *On release (chosen).* After 0.5 s the status LED blinks as a "you can let go" cue; releasing sends minimum, unless the encoder was rotated during the hold. Keeps push-and-rotate intact.
- *At 0.5 s while still held.* Lamp reacts without releasing, but a rotate that starts later than 0.5 s after the press would first drop the lamp to minimum, and the old level cannot be restored. Rejected.

**Double-click detection**

- *No wait (chosen).* The first click toggles immediately, as today. If a second short click is released within the double-click window, send full brightness. From off: on, then full. From on: the lamp starts fading off and is then driven to full, a brief dip. No change to `switch_driver`.
- *Wait for a possible second click.* Clean when the lamp is on, but every ordinary click becomes ~0.3 s slower. Rejected: the main use is from off.

## Design

All changes are in `firmware/main/esp_zb_switch.c`, in `handle_encoder_button` and `encoder_ctrl_t`.

- New constants: `BUTTON_DOUBLE_CLICK_MS 400`, `LEVEL_MAX 254`, `LEVEL_MIN 1`, `LEVEL_PRESET_TRANSITION_DS 5` (0.5 s).
- New field `last_click_tick` in `encoder_ctrl_t`.
- `SWITCH_RELEASE_DETECTED` (short click, not rotated): if within `BUTTON_DOUBLE_CLICK_MS` of the previous short-click release, send Move to Level with On/Off (`LEVEL_MAX`) and clear `last_click_tick` so a third click is a plain toggle; otherwise send Toggle and record the tick.
- `SWITCH_LONG_PRESS_DETECTED` (0.5 s, still held, not rotated): blink the LED once as the release cue.
- `SWITCH_LONG_RELEASE_DETECTED` / `SWITCH_HOLD_RELEASE_DETECTED` (not rotated): send Move to Level with On/Off (`LEVEL_MIN`). This replaces the current "hold = Off".
- Commands go out through the binding table with `ZB_SEND_CMD`, like the existing ones.

## Behaviour change to note

"Hold = off" is removed. Off is the single click.

## Left to the builder

- Exact LED colour for the release cue (reuse an existing `led_state_t`).
- The esp-zigbee-sdk 1.6.8 request type for Move to Level with On/Off.

## Verification

Build for ESP32-C6, flash, and check on the bench with the Aqara T2 bulb:
from off and from on — single click, double-click, hold-release, push-and-rotate, triple click.
