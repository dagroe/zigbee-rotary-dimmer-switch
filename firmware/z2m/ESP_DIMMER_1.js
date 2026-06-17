// External zigbee2mqtt converter for the DG Electronics ESP32-C6 rotary dimmer.
//
// Why this is needed: z2m only exposes OTA controls for devices it recognizes.
// This definition makes z2m identify the device (by its Zigbee model id) and
// enables OTA so it will offer firmware updates from the OTA override index.
//
// Install (z2m 2.x), in configuration.yaml:
//   external_converters:
//     - ESP_DIMMER_1.js
// (place this file in the z2m data dir, next to configuration.yaml, then restart z2m).
//
// The device is a controller (it sends on/off, level and color commands to a
// bound target), so there are no exposes to control here -- this definition is
// for identification + OTA. Bind it to your light via the device's Bind tab.
const {commandsOnOff, commandsLevelCtrl, commandsColorCtrl, identify} = require('zigbee-herdsman-converters/lib/modernExtend');

module.exports = [
    {
        zigbeeModel: ['ESP_DIMMER_1'],
        model: 'ESP_DIMMER_1',
        vendor: 'DG Electronics',
        description: 'ESP32-C6 Zigbee rotary dimmer switch',
        extend: [identify(),commandsOnOff(), commandsLevelCtrl(), commandsColorCtrl()],
        // z2m 2.x: `ota: true` enables OTA using the (override) index.
        // z2m 1.x: replace the line below with:
        //   const ota = require('zigbee-herdsman-converters/lib/ota');
        //   ... ota: ota.zigbeeOTA,
        ota: true,
    },
];
