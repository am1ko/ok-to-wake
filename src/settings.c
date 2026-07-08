#include "settings.h"
#include "graphics.h"
#include "input.h"
#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_DECLARE(kids_alarm_clock);

static int settings_hour;
static int settings_minute;
static int settings_second;
static int settings_day_hour = 8;
static int settings_day_minute = 0;
static int settings_night_hour = 21;
static int settings_night_minute = 0;
static int settings_weekend_day_hour = 9;
static int settings_weekend_day_minute = 0;
static int settings_weekend_night_hour = 22;
static int settings_weekend_night_minute = 0;
static int settings_day_of_week = 1;
static int selected_transition_mode;
static int settings_brightness = 100;
static bool settings_rotate_180;
static int settings_time_trim_ppm;
static const struct device *settings_display_dev;
static int selected_tab;
static int selected_field;

static void apply_display_settings(void);

#define SETTINGS_SUBTREE "kids_alarm_clock"
#define SETTINGS_KEY "config"

struct persisted_settings {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day_hour;
    uint8_t day_minute;
    uint8_t night_hour;
    uint8_t night_minute;
    uint8_t weekend_day_hour;
    uint8_t weekend_day_minute;
    uint8_t weekend_night_hour;
    uint8_t weekend_night_minute;
    uint8_t day_of_week;
    uint8_t brightness;
    uint8_t rotate_180;
    int16_t time_trim_ppm;
} __packed;

struct legacy_persisted_settings {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day_hour;
    uint8_t day_minute;
    uint8_t night_hour;
    uint8_t night_minute;
    uint8_t brightness;
    uint8_t rotate_180;
    int16_t time_trim_ppm;
} __packed;

static int persist_settings_set(const char *key, size_t len, settings_read_cb read_cb,
                               void *cb_arg)
{
    struct persisted_settings stored;

    if (strcmp(key, SETTINGS_KEY) != 0) {
        return -ENOENT;
    }

    if (len == sizeof(struct legacy_persisted_settings)) {
        struct legacy_persisted_settings legacy;
        if (read_cb(cb_arg, &legacy, len) < 0) {
            LOG_ERR("Failed to read persisted settings");
            return -EIO;
        }
        settings_hour = legacy.hour;
        settings_minute = legacy.minute;
        settings_second = legacy.second;
        settings_day_hour = legacy.day_hour;
        settings_day_minute = legacy.day_minute;
        settings_night_hour = legacy.night_hour;
        settings_night_minute = legacy.night_minute;
        settings_brightness = legacy.brightness;
        settings_rotate_180 = legacy.rotate_180;
        settings_time_trim_ppm = legacy.time_trim_ppm;
    } else if (len == sizeof(stored)) {
        if (read_cb(cb_arg, &stored, len) < 0) {
            LOG_ERR("Failed to read persisted settings");
            return -EIO;
        }
        settings_hour = stored.hour;
        settings_minute = stored.minute;
        settings_second = stored.second;
        settings_day_hour = stored.day_hour;
        settings_day_minute = stored.day_minute;
        settings_night_hour = stored.night_hour;
        settings_night_minute = stored.night_minute;
        settings_weekend_day_hour = stored.weekend_day_hour;
        settings_weekend_day_minute = stored.weekend_day_minute;
        settings_weekend_night_hour = stored.weekend_night_hour;
        settings_weekend_night_minute = stored.weekend_night_minute;
        settings_day_of_week = stored.day_of_week;
        settings_brightness = stored.brightness;
        settings_rotate_180 = stored.rotate_180;
        settings_time_trim_ppm = stored.time_trim_ppm;
    } else {
        LOG_ERR("Unexpected settings length: %zu", len);
        return -EINVAL;
    }

    graphics_set_time(settings_hour, settings_minute, settings_second);
    graphics_set_time_trim(settings_time_trim_ppm);
    graphics_set_transition_times(settings_day_hour, settings_day_minute,
                                  settings_night_hour, settings_night_minute);
    graphics_set_weekend_transition_times(settings_weekend_day_hour,
                                          settings_weekend_day_minute,
                                          settings_weekend_night_hour,
                                          settings_weekend_night_minute);
    graphics_set_day_of_week(settings_day_of_week);
    apply_display_settings();

    return 0;
}

static int persist_settings_commit(void)
{
    return 0;
}

static struct settings_handler persist_settings_handler = {
    .name = SETTINGS_SUBTREE,
    .h_set = persist_settings_set,
    .h_commit = persist_settings_commit,
};

static void save_persistent_settings(void)
{
    struct persisted_settings stored = {
        .hour = (uint8_t)settings_hour,
        .minute = (uint8_t)settings_minute,
        .second = (uint8_t)settings_second,
        .day_hour = (uint8_t)settings_day_hour,
        .day_minute = (uint8_t)settings_day_minute,
        .night_hour = (uint8_t)settings_night_hour,
        .night_minute = (uint8_t)settings_night_minute,
        .weekend_day_hour = (uint8_t)settings_weekend_day_hour,
        .weekend_day_minute = (uint8_t)settings_weekend_day_minute,
        .weekend_night_hour = (uint8_t)settings_weekend_night_hour,
        .weekend_night_minute = (uint8_t)settings_weekend_night_minute,
        .day_of_week = (uint8_t)settings_day_of_week,
        .brightness = (uint8_t)settings_brightness,
        .rotate_180 = (uint8_t)settings_rotate_180,
        .time_trim_ppm = (int16_t)settings_time_trim_ppm,
    };

    int ret = settings_save_one(SETTINGS_SUBTREE "/" SETTINGS_KEY, &stored, sizeof(stored));
    if (ret < 0) {
        LOG_ERR("Failed to save settings: %d", ret);
    }
}

static void load_persistent_settings(void)
{
    int ret = settings_subsys_init();
    if (ret < 0) {
        LOG_ERR("settings_subsys_init failed: %d", ret);
        return;
    }

    ret = settings_register(&persist_settings_handler);
    if (ret < 0) {
        LOG_ERR("settings_register failed: %d", ret);
        return;
    }

    ret = settings_load_subtree(SETTINGS_SUBTREE);
    if (ret < 0) {
        LOG_ERR("settings_load_subtree failed: %d", ret);
    }
}

static lv_obj_t *settings_screen;
static lv_obj_t *settings_tab_btns[3];
static lv_obj_t *settings_page_containers[3];
static lv_obj_t *settings_time_field_btns[4];
static lv_obj_t *settings_time_value_labels[4];
static lv_obj_t *settings_transition_field_btns[4];
static lv_obj_t *settings_transition_value_labels[4];
static lv_obj_t *settings_display_trim_btn;
static lv_obj_t *settings_display_trim_label;
static lv_obj_t *settings_inc_btn;
static lv_obj_t *settings_dec_btn;
static lv_obj_t *settings_save_btn;
static lv_obj_t *settings_cancel_btn;
static lv_obj_t *settings_weekday_btn;
static lv_obj_t *settings_weekend_btn;

static const char *day_names[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static void update_settings_time_labels(void)
{
    lv_label_set_text_fmt(settings_time_value_labels[0], "%02d", settings_hour);
    lv_label_set_text_fmt(settings_time_value_labels[1], "%02d", settings_minute);
    lv_label_set_text_fmt(settings_time_value_labels[2], "%02d", settings_second);
    lv_label_set_text(settings_time_value_labels[3], day_names[settings_day_of_week]);
}

static void update_settings_transition_labels(void)
{
    if (selected_transition_mode == 0) {
        lv_label_set_text_fmt(settings_transition_value_labels[0], "%02d", settings_day_hour);
        lv_label_set_text_fmt(settings_transition_value_labels[1], "%02d", settings_day_minute);
        lv_label_set_text_fmt(settings_transition_value_labels[2], "%02d", settings_night_hour);
        lv_label_set_text_fmt(settings_transition_value_labels[3], "%02d", settings_night_minute);
    } else {
        lv_label_set_text_fmt(settings_transition_value_labels[0], "%02d", settings_weekend_day_hour);
        lv_label_set_text_fmt(settings_transition_value_labels[1], "%02d", settings_weekend_day_minute);
        lv_label_set_text_fmt(settings_transition_value_labels[2], "%02d", settings_weekend_night_hour);
        lv_label_set_text_fmt(settings_transition_value_labels[3], "%02d", settings_weekend_night_minute);
    }
}

static void update_settings_field_styles(void)
{
    for (int i = 0; i < 4; i++) {
        if (selected_tab == 0 && i == selected_field) {
            lv_obj_set_style_border_width(settings_time_field_btns[i], 3, LV_PART_MAIN);
            lv_obj_set_style_border_color(settings_time_field_btns[i], lv_color_make(0x33, 0x99, 0xFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(settings_time_field_btns[i], 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(settings_time_field_btns[i], lv_color_white(), LV_PART_MAIN);
        }
    }

    for (int i = 0; i < 3; i++) {
        if (i == selected_tab) {
            lv_obj_set_style_bg_color(settings_tab_btns[i], lv_color_make(0x33, 0x99, 0xFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(settings_tab_btns[i], lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_color(settings_tab_btns[i], lv_color_black(), LV_PART_MAIN);
            lv_obj_set_style_text_color(settings_tab_btns[i], lv_color_white(), 0);
        }
    }

    if (selected_tab == 2) {
        lv_obj_set_style_border_width(settings_display_trim_btn,
                                      selected_field == 0 ? 3 : 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(settings_display_trim_btn,
                                      selected_field == 0 ? lv_color_make(0x33, 0x99, 0xFF)
                                                         : lv_color_white(), LV_PART_MAIN);
    } else {
        lv_obj_set_style_border_width(settings_display_trim_btn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(settings_display_trim_btn, lv_color_white(), LV_PART_MAIN);
    }
}

static void hide_settings(void)
{
    lv_obj_add_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
}

static void apply_display_brightness(void)
{
    if (!settings_display_dev) {
        return;
    }

    int ret = display_set_brightness(settings_display_dev, settings_brightness);
    if (ret < 0 && ret != -ENOTSUP && ret != -ENOSYS) {
        LOG_ERR("Failed to set brightness: %d", ret);
    }
}

static void apply_display_orientation(void)
{
    if (!settings_display_dev) {
        return;
    }

    enum display_orientation orientation = settings_rotate_180 ? DISPLAY_ORIENTATION_ROTATED_180 : DISPLAY_ORIENTATION_NORMAL;
    int ret = display_set_orientation(settings_display_dev, orientation);
    if (ret < 0 && ret != -ENOTSUP) {
        LOG_ERR("Failed to set orientation: %d", ret);
    }
}

static void apply_display_settings(void)
{
    apply_display_brightness();
    apply_display_orientation();
}

static void set_active_settings_tab(int tab)
{
    selected_tab = tab;
    lv_obj_add_flag(settings_page_containers[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_page_containers[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_page_containers[2], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(settings_page_containers[tab], LV_OBJ_FLAG_HIDDEN);
    if (settings_weekday_btn && settings_weekend_btn) {
        lv_obj_set_style_bg_color(settings_weekday_btn,
                                  selected_transition_mode == 0 ? lv_color_make(0x33, 0x99, 0xFF) : lv_color_black(),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_color(settings_weekend_btn,
                                  selected_transition_mode == 1 ? lv_color_make(0x33, 0x99, 0xFF) : lv_color_black(),
                                  LV_PART_MAIN);
    }
    selected_field = 0;
    update_settings_field_styles();
}

static void update_settings_display_labels(void)
{
    lv_label_set_text_fmt(settings_display_trim_label, "%+d ppm", settings_time_trim_ppm);
}

static void adjust_settings_field(int delta)
{
    if (selected_tab == 0) {
        if (selected_field == 0) {
            settings_hour = (settings_hour + delta + 24) % 24;
        } else if (selected_field == 1) {
            settings_minute = (settings_minute + delta + 60) % 60;
        } else if (selected_field == 2) {
            settings_second = (settings_second + delta + 60) % 60;
        } else {
            settings_day_of_week = (settings_day_of_week + delta + 7) % 7;
        }
        update_settings_time_labels();
    } else if (selected_tab == 1) {
        if (selected_transition_mode == 0) {
            if (selected_field == 0) {
                settings_day_hour = (settings_day_hour + delta + 24) % 24;
            } else if (selected_field == 1) {
                settings_day_minute = (settings_day_minute + delta + 60) % 60;
            } else if (selected_field == 2) {
                settings_night_hour = (settings_night_hour + delta + 24) % 24;
            } else {
                settings_night_minute = (settings_night_minute + delta + 60) % 60;
            }
        } else {
            if (selected_field == 0) {
                settings_weekend_day_hour = (settings_weekend_day_hour + delta + 24) % 24;
            } else if (selected_field == 1) {
                settings_weekend_day_minute = (settings_weekend_day_minute + delta + 60) % 60;
            } else if (selected_field == 2) {
                settings_weekend_night_hour = (settings_weekend_night_hour + delta + 24) % 24;
            } else {
                settings_weekend_night_minute = (settings_weekend_night_minute + delta + 60) % 60;
            }
        }
        update_settings_transition_labels();
    } else {
        settings_time_trim_ppm += delta * 10;
        if (settings_time_trim_ppm < -5000) {
            settings_time_trim_ppm = -5000;
        } else if (settings_time_trim_ppm > 5000) {
            settings_time_trim_ppm = 5000;
        }
        update_settings_display_labels();
    }
}

static void settings_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);

    if (target == settings_tab_btns[0]) {
        selected_transition_mode = 0;
    selected_transition_mode = 0;
    set_active_settings_tab(0);
        return;
    }
    if (target == settings_tab_btns[1]) {
        set_active_settings_tab(1);
        return;
    }
    if (target == settings_tab_btns[2]) {
        set_active_settings_tab(2);
        return;
    }
    if (target == settings_inc_btn) {
        adjust_settings_field(1);
        return;
    }
    if (target == settings_dec_btn) {
        adjust_settings_field(-1);
        return;
    }
    if (target == settings_save_btn) {
        graphics_set_time(settings_hour, settings_minute, settings_second);
        graphics_set_time_trim(settings_time_trim_ppm);
        graphics_set_transition_times(settings_day_hour, settings_day_minute, settings_night_hour, settings_night_minute);
        graphics_set_weekend_transition_times(settings_weekend_day_hour,
                                              settings_weekend_day_minute,
                                              settings_weekend_night_hour,
                                              settings_weekend_night_minute);
        graphics_set_day_of_week(settings_day_of_week);
        apply_display_settings();
        save_persistent_settings();
        hide_settings();
        return;
    }
    if (target == settings_cancel_btn) {
        hide_settings();
        return;
    }
    if (target == settings_weekday_btn) {
        selected_transition_mode = 0;
        lv_obj_set_style_bg_color(settings_weekday_btn, lv_color_make(0x33, 0x99, 0xFF), LV_PART_MAIN);
        lv_obj_set_style_bg_color(settings_weekend_btn, lv_color_black(), LV_PART_MAIN);
        update_settings_transition_labels();
        update_settings_field_styles();
        return;
    }
    if (target == settings_weekend_btn) {
        selected_transition_mode = 1;
        lv_obj_set_style_bg_color(settings_weekend_btn, lv_color_make(0x33, 0x99, 0xFF), LV_PART_MAIN);
        lv_obj_set_style_bg_color(settings_weekday_btn, lv_color_black(), LV_PART_MAIN);
        update_settings_transition_labels();
        update_settings_field_styles();
        return;
    }

    if (selected_tab == 0) {
        for (int i = 0; i < 4; i++) {
            if (target == settings_time_field_btns[i]) {
                selected_field = i;
                update_settings_field_styles();
                return;
            }
        }
    } else if (selected_tab == 1) {
        for (int i = 0; i < 4; i++) {
            if (target == settings_transition_field_btns[i]) {
                selected_field = i;
                update_settings_field_styles();
                return;
            }
        }
    } else {
        if (target == settings_display_trim_btn) {
            selected_field = 0;
            update_settings_field_styles();
            return;
        }
    }
}

static void gear_button_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(settings_screen);
        settings_show();
    }
}

static lv_obj_t *create_setting_box(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
    return label;
}

static lv_obj_t *create_time_field(lv_obj_t *parent, lv_obj_t **label)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 80, 60);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_white(), 0);
    lv_obj_add_event_cb(btn, settings_event_cb, LV_EVENT_CLICKED, NULL);

    *label = lv_label_create(btn);
    lv_label_set_text(*label, "00");
    lv_obj_set_style_text_font(*label, &lv_font_montserrat_28, 0);
    lv_obj_center(*label);
    return btn;
}

static lv_obj_t *create_transition_field(lv_obj_t *parent, lv_obj_t **label)
{
    return create_time_field(parent, label);
}

static void create_gear_button(lv_obj_t *screen);

void settings_show(void);

static void create_gear_button(lv_obj_t *screen)
{
    lv_obj_t *gear_btn = lv_btn_create(screen);
    lv_obj_set_size(gear_btn, 40, 40);
    lv_obj_set_style_radius(gear_btn, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(gear_btn, lv_color_make(0x22, 0x22, 0x22), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(gear_btn, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(gear_btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(gear_btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(gear_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_event_cb(gear_btn, gear_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(gear_btn);
#ifndef LV_SYMBOL_SETTINGS
#define LV_SYMBOL_SETTINGS "⚙"
#endif
    lv_label_set_text(label, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
}

static void create_settings_content(void)
{
    const int field_width = 80;
    const int field_spacing = 12;
    const int time_base_x = -((4 - 1) * (field_width + field_spacing)) / 2;
    const int transition_base_x = -((2 - 1) * (field_width + field_spacing)) / 2;

    lv_obj_t *time_row = lv_obj_create(settings_page_containers[0]);
    lv_obj_set_size(time_row, LV_HOR_RES, 80);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(time_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(time_row, LV_ALIGN_TOP_MID, 0, 10);

    for (int i = 0; i < 4; i++) {
        settings_time_field_btns[i] = create_time_field(time_row, &settings_time_value_labels[i]);
        lv_obj_set_size(settings_time_field_btns[i], i == 3 ? 90 : 80, 60);
        lv_obj_align(settings_time_field_btns[i], LV_ALIGN_CENTER,
                     time_base_x + i * (field_width + field_spacing), 0);

        if (i < 2) {
            lv_obj_t *colon = lv_label_create(time_row);
            lv_label_set_text(colon, ":");
            lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(colon, lv_color_white(), 0);
            lv_obj_align(colon, LV_ALIGN_CENTER, time_base_x + i * (field_width + field_spacing) + (field_width + field_spacing) / 2, 0);
        }
    }

    const int day_label_y = 4;
    const int day_row_y = 4;
    const int night_label_y = 52;
    const int night_row_y = 52;

    for (int i = 0; i < 2; i++) {
        settings_transition_field_btns[i] = create_transition_field(settings_page_containers[1], &settings_transition_value_labels[i]);
        lv_obj_set_size(settings_transition_field_btns[i], 80, 44);
        lv_obj_align(settings_transition_field_btns[i], LV_ALIGN_TOP_MID, transition_base_x + i * (field_width + field_spacing), day_row_y);

        if (i == 0) {
            lv_obj_t *colon = lv_label_create(settings_page_containers[1]);
            lv_label_set_text(colon, ":");
            lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(colon, lv_color_white(), 0);
            lv_obj_align(colon, LV_ALIGN_TOP_MID, 0, day_row_y);
        }
    }

    for (int i = 0; i < 2; i++) {
        settings_transition_field_btns[2 + i] = create_transition_field(settings_page_containers[1], &settings_transition_value_labels[2 + i]);
        lv_obj_set_size(settings_transition_field_btns[2 + i], 80, 44);
        lv_obj_align(settings_transition_field_btns[2 + i], LV_ALIGN_TOP_MID, transition_base_x + i * (field_width + field_spacing), night_row_y);

        if (i == 0) {
            lv_obj_t *colon = lv_label_create(settings_page_containers[1]);
            lv_label_set_text(colon, ":");
            lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(colon, lv_color_white(), 0);
            lv_obj_align(colon, LV_ALIGN_TOP_MID, 0, night_row_y);
        }
    }

    create_setting_box(settings_page_containers[2], "Clock trim", 20, 20);
    settings_display_trim_btn = create_time_field(settings_page_containers[2], &settings_display_trim_label);
    lv_obj_set_size(settings_display_trim_btn, 180, 44);
    lv_obj_align(settings_display_trim_btn, LV_ALIGN_TOP_MID, 0, 48);

    settings_weekday_btn = input_create_button(settings_page_containers[1], 100, 44,
                                                "Weekday", settings_event_cb);
    lv_obj_align(settings_weekday_btn, LV_ALIGN_TOP_LEFT, 12, 0);

    settings_weekend_btn = input_create_button(settings_page_containers[1], 100, 44,
                                                "Weekend", settings_event_cb);
    lv_obj_align(settings_weekend_btn, LV_ALIGN_TOP_RIGHT, -12, 0);
}

static void create_navigation_buttons(void)
{
    settings_dec_btn = input_create_button(settings_screen, 80, 40, "-", settings_event_cb);
    lv_obj_align(settings_dec_btn, LV_ALIGN_BOTTOM_LEFT, 120, -10);

    settings_inc_btn = input_create_button(settings_screen, 80, 40, "+", settings_event_cb);
    lv_obj_align(settings_inc_btn, LV_ALIGN_BOTTOM_RIGHT, -120, -10);
}

static void create_save_cancel_buttons(void)
{
    settings_save_btn = input_create_button(settings_screen, 100, 40, "Save", settings_event_cb);
    input_style_button(settings_save_btn, lv_color_make(0x33, 0x99, 0x33), LV_OPA_COVER);
    lv_obj_align(settings_save_btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    settings_cancel_btn = input_create_button(settings_screen, 100, 40, "Cancel", settings_event_cb);
    input_style_button(settings_cancel_btn, lv_color_make(0x99, 0x33, 0x33), LV_OPA_COVER);
    lv_obj_align(settings_cancel_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
}

void settings_ui_init(lv_obj_t *screen, const struct device *display_dev)
{
    settings_display_dev = display_dev;
    load_persistent_settings();
    settings_screen = lv_obj_create(screen);
    lv_obj_set_size(settings_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(settings_screen, lv_color_make(0x11, 0x11, 0x11), 0);
    lv_obj_set_style_bg_opa(settings_screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(settings_screen);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    const int tab_width = 100;

    settings_tab_btns[0] = input_create_button(settings_screen, tab_width, 32, "Time", settings_event_cb);
    lv_obj_align(settings_tab_btns[0], LV_ALIGN_TOP_LEFT, 20, 36);

    settings_tab_btns[1] = input_create_button(settings_screen, tab_width, 32, "Transition", settings_event_cb);
    lv_obj_align(settings_tab_btns[1], LV_ALIGN_TOP_LEFT, 140, 36);

    settings_tab_btns[2] = input_create_button(settings_screen, tab_width, 32, "Trim", settings_event_cb);
    lv_obj_align(settings_tab_btns[2], LV_ALIGN_TOP_LEFT, 260, 36);

    settings_page_containers[0] = lv_obj_create(settings_screen);
    lv_obj_set_size(settings_page_containers[0], LV_HOR_RES, LV_VER_RES - 70);
    lv_obj_set_style_bg_opa(settings_page_containers[0], LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(settings_page_containers[0], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(settings_page_containers[0], LV_ALIGN_TOP_MID, 0, 70);

    settings_page_containers[1] = lv_obj_create(settings_screen);
    lv_obj_set_size(settings_page_containers[1], LV_HOR_RES, LV_VER_RES - 70);
    lv_obj_set_style_bg_opa(settings_page_containers[1], LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(settings_page_containers[1], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(settings_page_containers[1], LV_ALIGN_TOP_MID, 0, 70);

    settings_page_containers[2] = lv_obj_create(settings_screen);
    lv_obj_set_size(settings_page_containers[2], LV_HOR_RES, LV_VER_RES - 70);
    lv_obj_set_style_bg_opa(settings_page_containers[2], LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(settings_page_containers[2], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(settings_page_containers[2], LV_ALIGN_TOP_MID, 0, 70);

    create_settings_content();
    create_navigation_buttons();
    create_save_cancel_buttons();
    create_gear_button(screen);

    set_active_settings_tab(0);
}

void settings_show(void)
{
    graphics_get_time(&settings_hour, &settings_minute, &settings_second);
    graphics_get_time_trim(&settings_time_trim_ppm);
    graphics_get_transition_times(&settings_day_hour, &settings_day_minute, &settings_night_hour, &settings_night_minute);
    graphics_get_weekend_transition_times(&settings_weekend_day_hour,
                                          &settings_weekend_day_minute,
                                          &settings_weekend_night_hour,
                                          &settings_weekend_night_minute);
    graphics_get_day_of_week(&settings_day_of_week);
    update_settings_time_labels();
    update_settings_transition_labels();
    update_settings_display_labels();
    set_active_settings_tab(0);
    lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settings_screen);
}
