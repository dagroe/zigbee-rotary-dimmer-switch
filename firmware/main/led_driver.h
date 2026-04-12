#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enum representing color states.
 */
typedef enum
{
    LED_COLOR_STATE_OFF = 0,
    LED_COLOR_STATE_WARN_RED,
    LED_COLOR_STATE_OK_GREEN,
    LED_COLOR_STATE_FEEDBACK_WHITE_ONE_PULSE,
    LED_COLOR_STATE_WAITING_YELLOW_BLINK,

    // New: continuous blink colors
    LED_COLOR_STATE_BLINK_RED,
    LED_COLOR_STATE_BLINK_GREEN,
    LED_COLOR_STATE_BLINK_BLUE,
    LED_COLOR_STATE_BLINK_YELLOW,
    LED_COLOR_STATE_BLINK_WHITE,

    // New: one-shot blink colors
    LED_COLOR_STATE_BLINK_ONCE_RED,
    LED_COLOR_STATE_BLINK_ONCE_GREEN,
    LED_COLOR_STATE_BLINK_ONCE_BLUE,
    LED_COLOR_STATE_BLINK_ONCE_YELLOW,
    LED_COLOR_STATE_BLINK_ONCE_WHITE,
} led_state_t;

void setup_led_strip(QueueHandle_t led_evt_queue_p);
void set_led_rgb(uint8_t r, uint8_t g, uint8_t b);


#ifdef __cplusplus
} // extern "C"
#endif