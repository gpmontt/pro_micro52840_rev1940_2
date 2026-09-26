/*
 * Pass-through LED strip (gpmontt,led-strip-indicator) that forwards every
 * update to the real strip, overriding the pixel for the active Bluetooth
 * profile. ZMK's underglow always paints all pixels the same and rewrites
 * the whole strip every 50ms, so a per-key indicator has to be applied on
 * the way to the strip rather than written separately.
 *
 * The active profile's LED (RGB_BT_PROFILE_INDICATOR_LED_<n>) is white at
 * the underglow's current brightness: solid while its host is connected,
 * blinking while it isn't. Profile state only exists on the central half;
 * on a peripheral this is a plain pass-through.
 */

#define DT_DRV_COMPAT gpmontt_led_strip_indicator

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define SHOW_PROFILES                                                                              \
    (IS_ENABLED(CONFIG_ZMK_BLE) &&                                                                 \
     (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)))

#if SHOW_PROFILES
#include <zmk/ble.h>
#endif

#define NUM_PIXELS DT_INST_PROP(0, chain_length)
#define BLINK_MS 500
#define FINDER_STEP_MS 2000

BUILD_ASSERT(NUM_PIXELS == DT_PROP(DT_INST_PHANDLE(0, led_strip), chain_length),
             "led-strip-indicator chain-length must match the real strip's");

static const struct device *const strip = DEVICE_DT_GET(DT_INST_PHANDLE(0, led_strip));

/* ZMK passes its own pixel buffer; edit a copy so its effect state is untouched. */
static struct led_rgb buf[NUM_PIXELS];

#if SHOW_PROFILES && !IS_ENABLED(CONFIG_RGB_BT_PROFILE_INDICATOR_FINDER)
static const int profile_leds[] = {
    CONFIG_RGB_BT_PROFILE_INDICATOR_LED_0,
    CONFIG_RGB_BT_PROFILE_INDICATOR_LED_1,
    CONFIG_RGB_BT_PROFILE_INDICATOR_LED_2,
};

static void show_profile(size_t num_pixels) {
    int profile = zmk_ble_active_profile_index();
    int led = profile >= 0 && profile < ARRAY_SIZE(profile_leds) ? profile_leds[profile] : -1;

    if (led < 0 || led >= num_pixels) {
        return;
    }

    if (!zmk_ble_active_profile_is_connected() && (k_uptime_get() / BLINK_MS) % 2) {
        buf[led] = (struct led_rgb){0};
        return;
    }

    /* Match the brightness the underglow is using for this pixel. */
    uint8_t level = MAX(buf[led].r, MAX(buf[led].g, buf[led].b));

    buf[led] = (struct led_rgb){.r = level, .g = level, .b = level};
}
#endif

#if IS_ENABLED(CONFIG_RGB_BT_PROFILE_INDICATOR_FINDER)
static void show_finder(size_t num_pixels) {
    static int last_index = -1;
    int index = (k_uptime_get() / FINDER_STEP_MS) % num_pixels;

    memset(buf, 0, num_pixels * sizeof(buf[0]));
    buf[index] = (struct led_rgb){.r = 64, .g = 64, .b = 64};

    if (index != last_index) {
        LOG_INF("LED finder: lighting LED %d", index);
        last_index = index;
    }
}
#endif

static int indicator_update_rgb(const struct device *dev, struct led_rgb *pixels,
                                size_t num_pixels) {
    num_pixels = MIN(num_pixels, NUM_PIXELS);
    memcpy(buf, pixels, num_pixels * sizeof(buf[0]));

#if IS_ENABLED(CONFIG_RGB_BT_PROFILE_INDICATOR_FINDER)
    show_finder(num_pixels);
#elif SHOW_PROFILES
    show_profile(num_pixels);
#endif

    return led_strip_update_rgb(strip, buf, num_pixels);
}

static int indicator_update_channels(const struct device *dev, uint8_t *channels,
                                     size_t num_channels) {
    return led_strip_update_channels(strip, channels, num_channels);
}

static const struct led_strip_driver_api indicator_api = {
    .update_rgb = indicator_update_rgb,
    .update_channels = indicator_update_channels,
};

static int indicator_init(const struct device *dev) {
    return device_is_ready(strip) ? 0 : -ENODEV;
}

DEVICE_DT_INST_DEFINE(0, indicator_init, NULL, NULL, NULL, POST_KERNEL,
                      CONFIG_LED_STRIP_INIT_PRIORITY, &indicator_api);
