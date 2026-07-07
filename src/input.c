#include "input.h"
#include <lvgl.h>

lv_obj_t *input_create_button(lv_obj_t *parent, int width, int height, const char *label_text, lv_event_cb_t event_cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, width, height);
    input_style_button(btn, lv_color_black(), LV_OPA_30);
    lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN);
    if (event_cb) {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, label_text);
    lv_obj_center(label);

    return btn;
}

void input_style_button(lv_obj_t *btn, lv_color_t bg_color, lv_opa_t opa)
{
    lv_obj_set_style_bg_color(btn, bg_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, opa, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN);
}
