/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include "graphics.h"
#include "settings.h"

LOG_MODULE_REGISTER(kids_alarm_clock);

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    int ret;

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device is not ready");
        return -ENODEV;
    }

    lv_obj_t *screen = lv_scr_act();

    graphics_init(screen);
    settings_init(screen);

    lv_timer_create(graphics_tick, 1000, NULL);

    ret = display_blanking_off(display_dev);
    if (ret < 0 && ret != -ENOSYS) {
        LOG_ERR("Failed to turn display blanking off: %d", ret);
        return ret;
    }

    while (1) {
        lv_timer_handler();
        k_sleep(K_MSEC(20));
    }
}
