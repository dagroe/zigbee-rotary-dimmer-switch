# A rotary Zigbee dimmer switch
A Zigbee dimmer switch with a tactile feeling like classic rotary dimmers that seamlessly integrates with your existing switch and socket design lines (e.g. Gira System 55).

|  <img src="img/dimmer_render.png" /> | <img src="img/dimmer_foto.png"  />  |
|---|---|

## Use case, requirements, and features
The goal of this dimmer switch is to
* provide tactile feeling like classic rotary dimmers, i.e. rotate and push one big knob (instead of multiple buttons)
* integrate with existing switch and socket design lines (e.g. Gira System 55), also for multi frames
* offer an optional 230V (or 110V) power supply, no more changing batteries + acts as Zigbee router
* control existing smart lights (e.g. Philips Hue), not making classic bulbs smart (i.e. switching/dimming classic bulbs)

Also nice to have:
* be extendable, e.g. easily add multiple dimmer switches or push buttons in multi frames
* allow for a second rotary input by rotating the knob while pushing it (e.g. to change color temperature or scenes)
* feature an integrated 230V (or 110V) mechanical switch to switch power of the connected lamp socket (e.g. for emergencies or to reset the lamp when replacing classic wall switches)
* be affordable


## Backstory: Why yet _another_ Zigbee dimmer switch?
TL;DR; I couldn't find one to fit my use case, so I decided to build my own.

When I moved into my new home I already had many Philips Hue Bulbs and classic Hue dimmer switches (the on with the four buttons) to control the lights.
However, those didn't integrate well with my Gira System 55 outlets, switches, frames, etc. even when using fancy adapters (https://www.samotech.co.uk/products/philips-hue-dimmer-frame-cover-light-switch/)
And I was also a big fan of using a rotary knob for dimming lights, since it is just more convenient.

So why didn't I use available rotary zigbee dimmers? Basically all dimmers I found are "real" dimmers, in the sense that they expect to control a load (classic dimmable bulb) or simply don't fit into the 55mm Gira system.

Since I though "well, it can't be that hard to hook up a rotary encoder to a Zigbee chip", I set out to build a rotary dimmer switch myself.



## Current state of development
I have built a second prototype using the ESP32-C6 chip. Hardware, firmware, and enclosure design files are available in this repository but I need to update this README at some point ;)

## Help wanted
My prototype is working but I see a lot of potential for improvement. Since I am mostly a software engineer with limited knowledge of electronics, I am looking for other interested in taking this project further. You can find me on discord here: https://discord.gg/CjbDc5nPja or checkout the discussion here: https://community.home-assistant.io/t/diy-zigbee-rotary-wall-dimmer/681751


