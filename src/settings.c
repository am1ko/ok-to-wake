#include "settings.h"
#include "graphics.h"
#include "input.h"
#include <lvgl.h>

static lv_obj_t *settings_screen;
static lv_obj_t *settings_tab_btns[2];
static lv_obj_t *settings_page_containers[2];
static lv_obj_t *settings_time_field_btns[3];
static lv_obj_t *settings_time_value_labels[3];
static lv_obj_t *settings_transition_field_btns[4];
static lv_obj_t *settings_transition_value_labels[4];
static lv_obj_t *settings_inc_btn;
static lv_obj_t *settings_dec_btn;
static lv_obj_t *settings_prev_btn;
static lv_obj_t *settings_next_btn;
static lv_obj_t *settings_save_btn;
static lv_obj_t *settings_cancel_btn;

static int settings_hour;
static int settings_minute;
static int settings_second;
static int settings_day_hour = 8;
static int settings_day_minute = 0;
static int settings_night_hour = 21;
static int settings_night_minute = 0;
static int selected_tab;
static int selected_field;

static void update_settings_time_labels(void)
{
    lv_label_set_text_fmt(settings_time_value_labels[0], "%02d", settings_hour);
    lv_label_set_text_fmt(settings_time_value_labels[1], "%02d", settings_minute);
    lv_label_set_text_fmt(settings_time_value_labels[2], "%02d", settings_second);
}

static void update_settings_transition_labels(void)
{
    lv_label_set_text_fmt(settings_transition_value_labels[0], "%02d", settings_day_hour);
    lv_label_set_text_fmt(settings_transition_value_labels[1], "%02d", settings_day_minute);
    lv_label_set_text_fmt(settings_transition_value_labels[2], "%02d", settings_night_hour);
    lv_label_set_text_fmt(settings_transition_value_labels[3], "%02d", settings_night_minute);
}

static void update_settings_field_styles(void)
{
    for (int i = 0; i < 3; i++) {
        if (selected_tab == 0 && i == selected_field) {
            lv_obj_set_style_border_width(settings_time_field_btns[i], 3, LV_PART_MAIN);
            lv_obj_set_style_border_color(settings_time_field_btns[i], lv_color_make(0x33, 0x99, 0xFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(settings_time_field_btns[i], 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(settings_time_field_btns[i], lv_color_white(), LV_PART_MAIN);
        }
    }

    for (int i = 0; i < 4; i++) {
        if (selected_tab == 1 && i == selected_field) {
            lv_obj_set_style_border_width(settings_transition_field_btns[i], 3, LV_PART_MAIN);
            lv_obj_set_style_border_color(settings_transition_field_btns[i], lv_color_make(0x33, 0x99, 0xFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(settings_transition_field_btns[i], 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(settings_transition_field_btns[i], lv_color_white(), LV_PART_MAIN);
        }
    }

    for (int i = 0; i < 2; i++) {
        if (i == selected_tab) {
            lv_obj_set_style_bg_color(settings_tab_btns[i], lv_color_make(0x33, 0x99, 0xFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(settings_tab_btns[i], lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_color(settings_tab_btns[i], lv_color_black(), LV_PART_MAIN);
            lv_obj_set_style_text_color(settings_tab_btns[i], lv_color_white(), 0);
        }
    }
}

static void hide_settings(void)
{
    lv_obj_add_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
}

static void set_active_settings_tab(int tab)
{
    selected_tab = tab;
    lv_obj_add_flag(settings_page_containers[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_page_containers[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(settings_page_containers[tab], LV_OBJ_FLAG_HIDDEN);
    selected_field = 0;
    update_settings_field_styles();
}

static void adjust_settings_field(int delta)
{
    if (selected_tab == 0) {
        if (selected_field == 0) {
            settings_hour = (settings_hour + delta + 24) % 24;
        } else if (selected_field == 1) {
            settings_minute = (settings_minute + delta + 60) % 60;
        } else {
            settings_second = (settings_second + delta + 60) % 60;
        }
        update_settings_time_labels();
    } else {
        if (selected_field == 0) {
            settings_day_hour = (settings_day_hour + delta + 24) % 24;
        } else if (selected_field == 1) {
            settings_day_minute = (settings_day_minute + delta + 60) % 60;
        } else if (selected_field == 2) {
            settings_night_hour = (settings_night_hour + delta + 24) % 24;
        } else {
            settings_night_minute = (settings_night_minute + delta + 60) % 60;
        }
        update_settings_transition_labels();
    }
}

static void select_next_field(void)
{
    int max_field = (selected_tab == 0) ? 3 : 4;
    selected_field = (selected_field + 1) % max_field;
    update_settings_field_styles();
}

static void select_previous_field(void)
{
    int max_field = (selected_tab == 0) ? 3 : 4;
    selected_field = (selected_field + max_field - 1) % max_field;
    update_settings_field_styles();
}

static void settings_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);

    if (target == settings_tab_btns[0]) {
        set_active_settings_tab(0);
        return;
    }
    if (target == settings_tab_btns[1]) {
        set_active_settings_tab(1);
        return;
    }
    if (target == settings_prev_btn) {
        select_previous_field();
        return;
    }
    if (target == settings_next_btn) {
        select_next_field();
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
        graphics_set_transition_times(settings_day_hour, settings_day_minute, settings_night_hour, settings_night_minute);
        hide_settings();
        return;
    }
    if (target == settings_cancel_btn) {
        hide_settings();
        return;
    }

    if (selected_tab == 0) {
        for (int i = 0; i < 3; i++) {
            if (target == settings_time_field_btns[i]) {
                selected_field = i;
                update_settings_field_styles();
                return;
            }
        }
    } else {
        for (int i = 0; i < 4; i++) {
            if (target == settings_transition_field_btns[i]) {
                selected_field = i;
                update_settings_field_styles();
                return;
            }
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
    const int base_x = -field_width - field_spacing;

    lv_obj_t *time_row = lv_obj_create(settings_page_containers[0]);
    lv_obj_set_size(time_row, LV_HOR_RES, 80);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(time_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(time_row, LV_ALIGN_TOP_MID, 0, 10);

    for (int i = 0; i < 3; i++) {
        settings_time_field_btns[i] = create_time_field(time_row, &settings_time_value_labels[i]);
        lv_obj_align(settings_time_field_btns[i], LV_ALIGN_CENTER, base_x + i * (field_width + field_spacing), 0);

        if (i < 2) {
            lv_obj_t *colon = lv_label_create(time_row);
            lv_label_set_text(colon, ":");
            lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(colon, lv_color_white(), 0);
            lv_obj_align(colon, LV_ALIGN_CENTER, base_x + i * (field_width + field_spacing) + field_width / 2 + field_spacing / 2, 0);
        }
    }

    create_setting_box(settings_page_containers[1], "Day", 20, 20);
    for (int i = 0; i < 2; i++) {
        settings_transition_field_btns[i] = create_transition_field(settings_page_containers[1], &settings_transition_value_labels[i]);
        lv_obj_align(settings_transition_field_btns[i], LV_ALIGN_TOP_MID, base_x + i * (field_width + field_spacing), 20);

        if (i == 0) {
            lv_obj_t *colon = lv_label_create(settings_page_containers[1]);
            lv_label_set_text(colon, ":");
            lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(colon, lv_color_white(), 0);
            lv_obj_align(colon, LV_ALIGN_TOP_MID, base_x + field_width + field_spacing / 2, 20);
        }
    }

    create_setting_box(settings_page_containers[1], "Night", 20, 110);
    for (int i = 0; i < 2; i++) {
        settings_transition_field_btns[2 + i] = create_transition_field(settings_page_containers[1], &settings_transition_value_labels[2 + i]);
        lv_obj_align(settings_transition_field_btns[2 + i], LV_ALIGN_TOP_MID, base_x + i * (field_width + field_spacing), 110);

        if (i == 0) {
            lv_obj_t *colon = lv_label_create(settings_page_containers[1]);
            lv_label_set_text(colon, ":");
            lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(colon, lv_color_white(), 0);
            lv_obj_align(colon, LV_ALIGN_TOP_MID, base_x + field_width + field_spacing / 2, 110);
        }
    }
}

static void create_navigation_buttons(void)
{
    settings_prev_btn = input_create_button(settings_screen, 56, 40, "<", settings_event_cb);
    lv_obj_align(settings_prev_btn, LV_ALIGN_BOTTOM_MID, -90, -140);

    settings_next_btn = input_create_button(settings_screen, 56, 40, ">", settings_event_cb);
    lv_obj_align(settings_next_btn, LV_ALIGN_BOTTOM_MID, 90, -140);

    settings_dec_btn = input_create_button(settings_screen, 80, 40, "-", settings_event_cb);
    lv_obj_align(settings_dec_btn, LV_ALIGN_BOTTOM_MID, -90, -90);

    settings_inc_btn = input_create_button(settings_screen, 80, 40, "+", settings_event_cb);
    lv_obj_align(settings_inc_btn, LV_ALIGN_BOTTOM_MID, 90, -90);
}

static void create_save_cancel_buttons(void)
{
    settings_save_btn = input_create_button(settings_screen, 100, 40, "Save", settings_event_cb);
    input_style_button(settings_save_btn, lv_color_make(0x33, 0x99, 0x33), LV_OPA_COVER);
    lv_obj_align(settings_save_btn, LV_ALIGN_BOTTOM_LEFT, 40, -20);

    settings_cancel_btn = input_create_button(settings_screen, 100, 40, "Cancel", settings_event_cb);
    input_style_button(settings_cancel_btn, lv_color_make(0x99, 0x33, 0x33), LV_OPA_COVER);
    lv_obj_align(settings_cancel_btn, LV_ALIGN_BOTTOM_RIGHT, -40, -20);
}

void settings_init(lv_obj_t *screen)
{
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

    settings_page_containers[0] = lv_obj_create(settings_screen);
    lv_obj_set_size(settings_page_containers[0], LV_HOR_RES, LV_VER_RES - 100);
    lv_obj_set_style_bg_opa(settings_page_containers[0], LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(settings_page_containers[0], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(settings_page_containers[0], LV_ALIGN_TOP_MID, 0, 70);

    settings_page_containers[1] = lv_obj_create(settings_screen);
    lv_obj_set_size(settings_page_containers[1], LV_HOR_RES, LV_VER_RES - 100);
    lv_obj_set_style_bg_opa(settings_page_containers[1], LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(settings_page_containers[1], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(settings_page_containers[1], LV_ALIGN_TOP_MID, 0, 70);

    create_settings_content();
    create_navigation_buttons();
    create_save_cancel_buttons();
    create_gear_button(screen);

    set_active_settings_tab(0);
}

void settings_show(void)
{
    graphics_get_time(&settings_hour, &settings_minute, &settings_second);
    graphics_get_transition_times(&settings_day_hour, &settings_day_minute, &settings_night_hour, &settings_night_minute);
    update_settings_time_labels();
    update_settings_transition_labels();
    set_active_settings_tab(0);
    lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settings_screen);
}
