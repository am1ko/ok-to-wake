#ifndef SETTINGS_H
#define SETTINGS_H

#include <lvgl.h>
#include <zephyr/device.h>

void settings_ui_init(lv_obj_t *screen, const struct device *display_dev);
void settings_show(void);

#endif /* SETTINGS_H */
