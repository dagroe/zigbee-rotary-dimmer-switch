// External zigbee2mqtt converter for the DG Electronics ESP32-C6 rotary dimmer.
//
// Why this is needed: z2m only recognizes (and offers OTA for) devices it has a
// definition for. This makes z2m identify the device by its Zigbee model id,
// expose its two endpoints, and enable OTA from the override index.
//
// Install (z2m 2.x), in configuration.yaml:
//   external_converters:
//     - ESP_DIMMER_1.js
// (place this file in the z2m data dir, next to configuration.yaml, then restart z2m).
//
// Endpoints:
//   1 = the on-board rotary/button controller. Sends on/off, level and color
//       commands to a bound target -- bind it to your light via the Bind tab.
//   2 = the on-board 230V relay (switchable on/off output for the lamp socket).
//   3 = external encoder daughterboard (a second dimmer). Same on/off/level/color
//       controller as endpoint 1, bound independently to its own light.
//   4 = external wall switch (a plain on/off controller), bound independently.
// Endpoints 3 and 4 are always present; if no daughterboard is wired in they
// simply never emit anything, so just leave them unbound.
const {
    deviceEndpoints, identify, onOff,
    commandsOnOff, commandsLevelCtrl, commandsColorCtrl,
} = require('zigbee-herdsman-converters/lib/modernExtend');

module.exports = [
    {
        zigbeeModel: ['ESP_DIMMER_1'],
        model: 'ESP_DIMMER_1',
        vendor: 'DG Electronics',
        description: 'ESP32-C6 Zigbee rotary dimmer switch',
        extend: [
            deviceEndpoints({endpoints: {controller: 1, relay: 2, ext_encoder: 3, ext_switch: 4}}),
            identify(),
            // On-board encoder/button controller (endpoint 1).
            commandsOnOff({endpointNames: ['controller']}),
            commandsLevelCtrl({endpointNames: ['controller']}),
            commandsColorCtrl({endpointNames: ['controller']}),
            // External encoder daughterboard -- second dimmer (endpoint 3).
            commandsOnOff({endpointNames: ['ext_encoder']}),
            commandsLevelCtrl({endpointNames: ['ext_encoder']}),
            commandsColorCtrl({endpointNames: ['ext_encoder']}),
            // External wall switch -- plain on/off controller (endpoint 4).
            commandsOnOff({endpointNames: ['ext_switch']}),
            // On-board 230V relay (on/off output) on endpoint 2.
            onOff({endpointNames: ['relay']}),
        ],
        // z2m 2.x: `ota: true` enables OTA using the (override) index.
        // z2m 1.x: replace the line below with:
        //   const ota = require('zigbee-herdsman-converters/lib/ota');
        //   ... ota: ota.zigbeeOTA,
        ota: true,
    },
];
