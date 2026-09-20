/*
 * Overrides the RGB underglow color while a specific layer is the
 * highest active layer, restoring the previous color once it
 * deactivates. See Kconfig (RGB_UNDERGLOW_LAYER_COLOR*) to configure
 * which layer and which color.
 */

#include <zephyr/kernel.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>

static struct zmk_led_hsb saved_color;
static bool color_saved;

static int rgb_underglow_layer_color_listener(const zmk_event_t *eh) {
    if (zmk_keymap_highest_layer_active() == CONFIG_RGB_UNDERGLOW_LAYER_COLOR_LAYER) {
        if (!color_saved) {
            /* direction 0 leaves the color untouched; used here just to read it. */
            saved_color = zmk_rgb_underglow_calc_hue(0);
            color_saved = true;
        }

        zmk_rgb_underglow_set_hsb((struct zmk_led_hsb){
            .h = CONFIG_RGB_UNDERGLOW_LAYER_COLOR_HUE,
            .s = CONFIG_RGB_UNDERGLOW_LAYER_COLOR_SAT,
            .b = CONFIG_RGB_UNDERGLOW_LAYER_COLOR_BRT,
        });
    } else if (color_saved) {
        zmk_rgb_underglow_set_hsb(saved_color);
        color_saved = false;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_underglow_layer_color, rgb_underglow_layer_color_listener);
ZMK_SUBSCRIPTION(rgb_underglow_layer_color, zmk_layer_state_changed);
