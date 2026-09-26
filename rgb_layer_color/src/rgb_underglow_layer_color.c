/*
 * Sets the RGB underglow hue from the highest active layer, using the
 * per-layer hues in Kconfig (RGB_UNDERGLOW_LAYER_COLOR_L<n>_HUE). A layer
 * with hue -1 leaves the color as it is. The current brightness is kept,
 * so the brightness keys still work.
 *
 * The color is applied by invoking the &rgb_ug behavior rather than
 * calling zmk_rgb_underglow_set_hsb() directly: &rgb_ug has global
 * locality, so on a split keyboard the central also forwards it to the
 * peripheral half. A direct API call would only recolor the central.
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <dt-bindings/zmk/rgb.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>

/* Indexed by layer; -1 means "leave the color unchanged". */
static const int layer_hues[] = {
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L0_HUE,
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L1_HUE,
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L2_HUE,
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L3_HUE,
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L4_HUE,
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L5_HUE,
};

static void apply_color(struct zmk_led_hsb color) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = "rgb_ug",
        .param1 = RGB_COLOR_HSB_CMD,
        .param2 = RGB_COLOR_HSB_VAL(color.h, color.s, color.b),
    };
    struct zmk_behavior_binding_event event = {
        .layer = zmk_keymap_highest_layer_active(),
        .position = 0,
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    zmk_behavior_invoke_binding(&binding, event, true);
}

static void apply_layer_color(void) {
    uint8_t layer = zmk_keymap_highest_layer_active();
    int hue = layer < ARRAY_SIZE(layer_hues) ? layer_hues[layer] : -1;

    if (hue < 0) {
        return;
    }

    /* direction 0 leaves the color untouched; used here just to read it. */
    struct zmk_led_hsb current = zmk_rgb_underglow_calc_hue(0);

    apply_color((struct zmk_led_hsb){
        .h = hue,
        .s = CONFIG_RGB_UNDERGLOW_LAYER_COLOR_SAT,
        .b = current.b,
    });
}

static int rgb_underglow_layer_color_listener(const zmk_event_t *eh) {
    apply_layer_color();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_underglow_layer_color, rgb_underglow_layer_color_listener);
ZMK_SUBSCRIPTION(rgb_underglow_layer_color, zmk_layer_state_changed);

/*
 * No layer event fires at boot, so apply the base layer color once after
 * startup. The delay gives the peripheral half time to connect so it
 * receives the color too.
 */
static void initial_color_work_handler(struct k_work *work) { apply_layer_color(); }

static K_WORK_DELAYABLE_DEFINE(initial_color_work, initial_color_work_handler);

static int rgb_underglow_layer_color_init(void) {
    k_work_schedule(&initial_color_work, K_SECONDS(5));
    return 0;
}

SYS_INIT(rgb_underglow_layer_color_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
