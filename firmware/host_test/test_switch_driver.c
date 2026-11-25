// Host test of the button state machine in main/switch_driver.c. Run: sh run.sh
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static int64_t g_us;
static int g_level[64];
int gpio_get_level(int pin) { return g_level[pin]; }
int64_t esp_timer_get_time(void) { return g_us; }

#include "switch_driver.c"

static char g_log[256];
static void cb(switch_func_pair_t *p, switch_state_t s)
{
    char buf[16];
    snprintf(buf, sizeof buf, "%u:%d ", (unsigned)p->pin, (int)s);
    strcat(g_log, buf);
}

static switch_func_pair_t pairs[] = { {1, SWITCH_ONOFF_TOGGLE_CONTROL}, {2, SWITCH_EXT_SWITCH_CONTROL} };

static void run_ms(int ms)
{
    for (int t = 0; t < ms; t += 10) {
        for (int i = 0; i < 2; ++i) button_poll(&pairs[i], &button_ctx[i]);
        g_us += 10000;
    }
}

static void expect(const char *name, const char *want)
{
    if (strcmp(g_log, want)) {
        printf("FAIL %s: got '%s' want '%s'\n", name, g_log, want);
        assert(0);
    }
    g_log[0] = 0;
}

static void start(int64_t t0_ms, int l1, int l2)
{
    g_us = t0_ms * 1000; g_level[1] = l1; g_level[2] = l2; g_log[0] = 0;
    assert(switch_driver_init(pairs, 2, cb));
}

int main(void)
{
    assert(SWITCH_PRESS_DETECTED == 1 && SWITCH_LONG_PRESS_DETECTED == 2 && SWITCH_HOLD_DETECTED == 3
           && SWITCH_RELEASE_DETECTED == 4 && SWITCH_LONG_RELEASE_DETECTED == 5
           && SWITCH_HOLD_RELEASE_DETECTED == 6);

    start(1000, 1, 1);
    g_level[1] = 0; run_ms(100); expect("tap press", "1:1 ");
    g_level[1] = 1; run_ms(100); expect("tap release", "1:4 ");

    g_level[1] = 0; run_ms(20); g_level[1] = 1; run_ms(100); expect("bounce ignored", "");

    g_level[1] = 0; run_ms(600); expect("long press", "1:1 1:2 ");
    run_ms(4500); expect("hold", "1:3 ");
    run_ms(3000); expect("hold reported once", "");
    g_level[1] = 1; run_ms(100); expect("hold release", "1:6 ");

    g_level[1] = 0; run_ms(1000); g_level[1] = 1; run_ms(100); expect("long release", "1:1 1:2 1:5 ");

    g_level[2] = 0; run_ms(10000); expect("latched contact", "2:1 2:2 2:3 ");
    g_level[1] = 0; run_ms(100); g_level[1] = 1; run_ms(100); expect("tap while other latched", "1:1 1:4 ");
    g_level[2] = 1; run_ms(100); expect("latched contact opens", "2:6 ");

    g_level[1] = 0; run_ms(100); g_level[2] = 0; run_ms(100);
    g_level[2] = 1; run_ms(100); g_level[1] = 1; run_ms(100);
    expect("overlapping presses", "1:1 2:1 2:4 1:4 ");

    start(1000, 1, 0);
    run_ms(300); expect("closed at boot: no press", "");
    g_level[2] = 1; run_ms(100); expect("closed at boot: release", "2:4 ");

    start(1000, 0, 1);
    run_ms(6000); expect("held from boot reaches hold", "1:2 1:3 ");

    start(4294967296LL - 200, 1, 1);
    g_level[1] = 0; run_ms(300); expect("press across ms counter wrap", "1:1 ");
    g_level[1] = 1; run_ms(100); expect("release after wrap", "1:4 ");

    puts("all button state machine checks passed");
    return 0;
}
