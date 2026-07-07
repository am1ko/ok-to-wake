#ifndef INPUT_H
#define INPUT_H

#include <lvgl.h>

lv_obj_t *input_create_button(lv_obj_t *parent, int width, int height, const char *label_text, lv_event_cb_t event_cb);
void input_style_button(lv_obj_t *btn, lv_color_t bg_color, lv_opa_t opa);

#endif /* INPUT_H */
