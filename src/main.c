/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <stdio.h>

LOG_MODULE_REGISTER(kids_alarm_clock);

#define INITIAL_HOUR 8
#define INITIAL_MINUTE 0
#define INITIAL_SECOND 0

static lv_obj_t *clock_label;
static lv_obj_t *seconds_arc;
static int hour = INITIAL_HOUR;
static int minute = INITIAL_MINUTE;
static int second = INITIAL_SECOND;

static void clock_timer_cb(lv_timer_t *timer)
{
    char time_str[16];
    ARG_UNUSED(timer);

    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hour, minute, second);
    lv_label_set_text(clock_label, time_str);
    lv_arc_set_value(seconds_arc, second);

    second++;
    if (second >= 60) {
        second = 0;
        minute++;
        if (minute >= 60) {
            minute = 0;
            hour = (hour + 1) % 24;
        }
    }
}

void main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    int ret;

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device is not ready");
        return;
    }

    lv_obj_t *screen = lv_scr_act();

    lv_obj_t *background = lv_obj_create(screen);
    lv_obj_set_size(background, 320, 240);
    lv_obj_center(background);
    lv_obj_set_style_bg_color(background, lv_color_make(0x10, 0x24, 0x44), 0);
    lv_obj_set_style_bg_opa(background, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(background, 22, 0);
    lv_obj_set_style_border_color(background, lv_color_make(0xFF, 0xC1, 0x07), 0);
    lv_obj_set_style_border_width(background, 4, 0);

    lv_obj_t *title_label = lv_label_create(background);
    lv_label_set_text(title_label, "Kids Alarm Clock");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    seconds_arc = lv_arc_create(screen);
    lv_obj_set_size(seconds_arc, 280, 280);
    lv_obj_center(seconds_arc);
    lv_arc_set_range(seconds_arc, 0, 59);
    lv_arc_set_value(seconds_arc, second);
    lv_arc_set_rotation(seconds_arc, 270);
    lv_arc_set_bg_angles(seconds_arc, 0, 360);
    lv_obj_remove_style(seconds_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(seconds_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(seconds_arc, lv_color_make(0xFF, 0xC1, 0x07), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(seconds_arc, lv_color_make(0x20, 0x34, 0x6B), LV_PART_MAIN);

    clock_label = lv_label_create(background);
    lv_label_set_text(clock_label, "00:00:00");
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_28, 0);
    lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t *date_label = lv_label_create(background);
    lv_label_set_text(date_label, "Mon Jan 01");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_14, 0);
    lv_obj_align(date_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_timer_create(clock_timer_cb, 1000, NULL);

    ret = display_blanking_off(display_dev);
    if (ret < 0 && ret != -ENOSYS) {
        LOG_ERR("Failed to turn display blanking off: %d", ret);
        return;
    }

    while (1) {
        lv_timer_handler();
        k_sleep(K_MSEC(20));
    }
}
