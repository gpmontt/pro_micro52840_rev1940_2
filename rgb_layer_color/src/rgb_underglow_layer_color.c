/*
 * Overrides the RGB underglow color while a layer with a configured hue
 * is the highest active layer, restoring the previous color once no such
 * layer is active. See Kconfig (RGB_UNDERGLOW_LAYER_COLOR*) to configure
 * the colors.
 *
 * The color is applied by invoking the &rgb_ug behavior rather than
 * calling zmk_rgb_underglow_set_hsb() directly: &rgb_ug has global
 * locality, so on a split keyboard the central also forwards it to the
 * peripheral half. A direct API call would only recolor the central.
 */

#include <zephyr/kernel.h>

#include <dt-bindings/zmk/rgb.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>

/* Indexed by layer; -1 means "no override" (layer 0 never overrides). */
static const int layer_hues[] = {
    -1,
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L1_HUE,
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L2_HUE,
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L3_HUE,
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L4_HUE,
    CONFIG_RGB_UNDERGLOW_LAYER_COLOR_L5_HUE,
};

static struct zmk_led_hsb saved_color;
static bool color_saved;

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

static int rgb_underglow_layer_color_listener(const zmk_event_t *eh) {
    uint8_t layer = zmk_keymap_highest_layer_active();
    int hue = layer < ARRAY_SIZE(layer_hues) ? layer_hues[layer] : -1;

    if (hue >= 0) {
        if (!color_saved) {
            /* direction 0 leaves the color untouched; used here just to read it. */
            saved_color = zmk_rgb_underglow_calc_hue(0);
            color_saved = true;
        }

        apply_color((struct zmk_led_hsb){
            .h = hue,
            .s = CONFIG_RGB_UNDERGLOW_LAYER_COLOR_SAT,
            .b = CONFIG_RGB_UNDERGLOW_LAYER_COLOR_BRT,
        });
    } else if (color_saved) {
        apply_color(saved_color);
        color_saved = false;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_underglow_layer_color, rgb_underglow_layer_color_listener);
ZMK_SUBSCRIPTION(rgb_underglow_layer_color, zmk_layer_state_changed);
