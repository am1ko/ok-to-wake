/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LOG_MODULE_REGISTER(kids_alarm_clock);

#define INITIAL_HOUR 8
#define INITIAL_MINUTE 0
#define INITIAL_SECOND 0

#define HOUR_MARKER_LENGTH 14
#define HOUR_MARKER_WIDTH 6
#define HOUR_HAND_WIDTH 8
#define MINUTE_HAND_WIDTH 6
#define SECOND_HAND_WIDTH 3

static lv_obj_t *background_obj;
static lv_obj_t *dial_outline;
static lv_obj_t *hour_hand;
static lv_obj_t *minute_hand;
static lv_obj_t *second_hand;
static lv_obj_t *hour_markers[12];
static lv_obj_t *hour_numbers[12];
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

static lv_point_precise_t hour_hand_points[2];
static lv_point_precise_t minute_hand_points[2];
static lv_point_precise_t second_hand_points[2];
static lv_point_precise_t hour_marker_points[12][2];

static int hour = INITIAL_HOUR;
static int minute = INITIAL_MINUTE;
static int second = INITIAL_SECOND;
static int settings_hour;
static int settings_minute;
static int settings_second;
static int settings_day_hour = 8;
static int settings_day_minute = 0;
static int settings_night_hour = 21;
static int settings_night_minute = 0;

static int selected_tab;
static int selected_field;

static inline int min_int(int a, int b)
{
    return a < b ? a : b;
}

static bool time_in_interval(int current, int start, int end)
{
    if (start <= end) {
        return current >= start && current <= end;
    }

    return current >= start || current <= end;
}

static lv_color_t get_background_color(int current_hour, int current_minute)
{
    int current_minutes = current_hour * 60 + current_minute;
    int day_transition = settings_day_hour * 60 + settings_day_minute;
    int night_transition = settings_night_hour * 60 + settings_night_minute;
    int day_end = (night_transition + 24 * 60 - 1) % (24 * 60);
    int night_end = (day_transition + 24 * 60 - 1) % (24 * 60);

    if (time_in_interval(current_minutes, day_transition, day_end)) {
        return lv_color_make(0xCC, 0xEE, 0xCC);
    }

    if (time_in_interval(current_minutes, night_transition, night_end)) {
        return lv_color_make(0xFF, 0xCC, 0xCC);
    }

    return lv_color_make(0xDD, 0xDD, 0xDD);
}

static void set_line_points(lv_obj_t *obj, lv_point_precise_t points[2], int x0, int y0, int x1, int y1)
{
    points[0].x = x0;
    points[0].y = y0;
    points[1].x = x1;
    points[1].y = y1;
    lv_line_set_points(obj, points, 2);
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
}

static void update_hand(lv_obj_t *hand, lv_point_precise_t points[2], int32_t angle_deg, int length)
{
    const int center = lv_obj_get_width(hand) / 2;
    const double rad = (90 - angle_deg) * M_PI / 180.0;
    const int x_end = center + (int)(cos(rad) * length);
    const int y_end = center - (int)(sin(rad) * length);

    set_line_points(hand, points, center, center, x_end, y_end);
}

static void update_background_color(void)
{
    lv_obj_set_style_bg_color(background_obj, get_background_color(hour, minute), 0);
}

static void update_clock_hands(void);

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

static void show_settings(void)
{
    settings_hour = hour;
    settings_minute = minute;
    settings_second = second;
    update_settings_time_labels();
    update_settings_transition_labels();
    set_active_settings_tab(0);
    lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settings_screen);
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
        int max_field = (selected_tab == 0) ? 3 : 4;
        selected_field = (selected_field + max_field - 1) % max_field;
        update_settings_field_styles();
        return;
    }
    if (target == settings_next_btn) {
        int max_field = (selected_tab == 0) ? 3 : 4;
        selected_field = (selected_field + 1) % max_field;
        update_settings_field_styles();
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
        hour = settings_hour;
        minute = settings_minute;
        second = settings_second;
        update_background_color();
        update_clock_hands();
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
        show_settings();
    }
}

static void create_settings_menu(lv_obj_t *screen)
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
    settings_tab_btns[0] = lv_btn_create(settings_screen);
    lv_obj_set_size(settings_tab_btns[0], tab_width, 32);
    lv_obj_set_style_bg_color(settings_tab_btns[0], lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_tab_btns[0], LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_border_color(settings_tab_btns[0], lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_tab_btns[0], settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *tab_label0 = lv_label_create(settings_tab_btns[0]);
    lv_label_set_text(tab_label0, "Time");
    lv_obj_center(tab_label0);
    lv_obj_align(settings_tab_btns[0], LV_ALIGN_TOP_LEFT, 20, 36);

    settings_tab_btns[1] = lv_btn_create(settings_screen);
    lv_obj_set_size(settings_tab_btns[1], tab_width, 32);
    lv_obj_set_style_bg_color(settings_tab_btns[1], lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_tab_btns[1], LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_border_color(settings_tab_btns[1], lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_tab_btns[1], settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *tab_label1 = lv_label_create(settings_tab_btns[1]);
    lv_label_set_text(tab_label1, "Transition");
    lv_obj_center(tab_label1);
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

    const int field_width = 80;
    const int field_height = 60;
    const int field_spacing = 12;
    const int base_x = -field_width - field_spacing;

    lv_obj_t *time_row = lv_obj_create(settings_page_containers[0]);
    lv_obj_set_size(time_row, LV_HOR_RES, 80);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(time_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(time_row, LV_ALIGN_TOP_MID, 0, 10);

    for (int i = 0; i < 3; i++) {
        settings_time_field_btns[i] = lv_btn_create(time_row);
        lv_obj_set_size(settings_time_field_btns[i], field_width, field_height);
        lv_obj_set_style_border_width(settings_time_field_btns[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(settings_time_field_btns[i], lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(settings_time_field_btns[i], lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(settings_time_field_btns[i], LV_OPA_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(settings_time_field_btns[i], lv_color_white(), 0);
        lv_obj_add_event_cb(settings_time_field_btns[i], settings_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label = lv_label_create(settings_time_field_btns[i]);
        lv_label_set_text(label, "00");
        lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
        lv_obj_center(label);
        settings_time_value_labels[i] = label;

        lv_obj_align(settings_time_field_btns[i], LV_ALIGN_CENTER, base_x + i * (field_width + field_spacing), 0);

        if (i < 2) {
            lv_obj_t *colon = lv_label_create(time_row);
            lv_label_set_text(colon, ":");
            lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(colon, lv_color_white(), 0);
            lv_obj_align(colon, LV_ALIGN_CENTER, base_x + i * (field_width + field_spacing) + field_width / 2 + field_spacing / 2, 0);
        }
    }

    lv_obj_t *day_label = lv_label_create(settings_page_containers[1]);
    lv_label_set_text(day_label, "Day");
    lv_obj_set_style_text_color(day_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(day_label, &lv_font_montserrat_14, 0);
    lv_obj_align(day_label, LV_ALIGN_TOP_LEFT, 20, 20);

    for (int i = 0; i < 2; i++) {
        settings_transition_field_btns[i] = lv_btn_create(settings_page_containers[1]);
        lv_obj_set_size(settings_transition_field_btns[i], field_width, field_height);
        lv_obj_set_style_border_width(settings_transition_field_btns[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(settings_transition_field_btns[i], lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(settings_transition_field_btns[i], lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(settings_transition_field_btns[i], LV_OPA_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(settings_transition_field_btns[i], lv_color_white(), 0);
        lv_obj_add_event_cb(settings_transition_field_btns[i], settings_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label = lv_label_create(settings_transition_field_btns[i]);
        lv_label_set_text(label, "00");
        lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
        lv_obj_center(label);
        settings_transition_value_labels[i] = label;

        lv_obj_align(settings_transition_field_btns[i], LV_ALIGN_TOP_MID, base_x + i * (field_width + field_spacing), 20);

        if (i == 0) {
            lv_obj_t *colon = lv_label_create(settings_page_containers[1]);
            lv_label_set_text(colon, ":");
            lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(colon, lv_color_white(), 0);
            lv_obj_align(colon, LV_ALIGN_TOP_MID, base_x + field_width + field_spacing / 2, 20);
        }
    }

    lv_obj_t *night_label = lv_label_create(settings_page_containers[1]);
    lv_label_set_text(night_label, "Night");
    lv_obj_set_style_text_color(night_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(night_label, &lv_font_montserrat_14, 0);
    lv_obj_align(night_label, LV_ALIGN_TOP_LEFT, 20, 110);

    for (int i = 0; i < 2; i++) {
        settings_transition_field_btns[2 + i] = lv_btn_create(settings_page_containers[1]);
        lv_obj_set_size(settings_transition_field_btns[2 + i], field_width, field_height);
        lv_obj_set_style_border_width(settings_transition_field_btns[2 + i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(settings_transition_field_btns[2 + i], lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(settings_transition_field_btns[2 + i], lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(settings_transition_field_btns[2 + i], LV_OPA_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(settings_transition_field_btns[2 + i], lv_color_white(), 0);
        lv_obj_add_event_cb(settings_transition_field_btns[2 + i], settings_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label = lv_label_create(settings_transition_field_btns[2 + i]);
        lv_label_set_text(label, "00");
        lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
        lv_obj_center(label);
        settings_transition_value_labels[2 + i] = label;

        lv_obj_align(settings_transition_field_btns[2 + i], LV_ALIGN_TOP_MID, base_x + i * (field_width + field_spacing), 110);

        if (i == 0) {
            lv_obj_t *colon = lv_label_create(settings_page_containers[1]);
            lv_label_set_text(colon, ":");
            lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(colon, lv_color_white(), 0);
            lv_obj_align(colon, LV_ALIGN_TOP_MID, base_x + field_width + field_spacing / 2, 110);
        }
    }

    settings_prev_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(settings_prev_btn, 56, 40);
    lv_obj_set_style_bg_color(settings_prev_btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_prev_btn, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_border_color(settings_prev_btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_prev_btn, settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *prev_label = lv_label_create(settings_prev_btn);
    lv_label_set_text(prev_label, "<");
    lv_obj_center(prev_label);
    lv_obj_align(settings_prev_btn, LV_ALIGN_BOTTOM_MID, -90, -140);

    settings_next_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(settings_next_btn, 56, 40);
    lv_obj_set_style_bg_color(settings_next_btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_next_btn, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_border_color(settings_next_btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_next_btn, settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *next_label = lv_label_create(settings_next_btn);
    lv_label_set_text(next_label, ">");
    lv_obj_center(next_label);
    lv_obj_align(settings_next_btn, LV_ALIGN_BOTTOM_MID, 90, -140);

    settings_dec_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(settings_dec_btn, 80, 40);
    lv_obj_set_style_bg_color(settings_dec_btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_dec_btn, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_border_color(settings_dec_btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_dec_btn, settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *dec_label = lv_label_create(settings_dec_btn);
    lv_label_set_text(dec_label, "-    ");
    lv_obj_center(dec_label);
    lv_obj_align(settings_dec_btn, LV_ALIGN_BOTTOM_MID, -90, -90);

    settings_inc_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(settings_inc_btn, 80, 40);
    lv_obj_set_style_bg_color(settings_inc_btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_inc_btn, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_border_color(settings_inc_btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_inc_btn, settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *inc_label = lv_label_create(settings_inc_btn);
    lv_label_set_text(inc_label, "+");
    lv_obj_center(inc_label);
    lv_obj_align(settings_inc_btn, LV_ALIGN_BOTTOM_MID, 90, -90);

    settings_save_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(settings_save_btn, 100, 40);
    lv_obj_set_style_bg_color(settings_save_btn, lv_color_make(0x33, 0x99, 0x33), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_save_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(settings_save_btn, settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_label = lv_label_create(settings_save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);
    lv_obj_align(settings_save_btn, LV_ALIGN_BOTTOM_LEFT, 40, -20);

    settings_cancel_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(settings_cancel_btn, 100, 40);
    lv_obj_set_style_bg_color(settings_cancel_btn, lv_color_make(0x99, 0x33, 0x33), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_cancel_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(settings_cancel_btn, settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_label = lv_label_create(settings_cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    lv_obj_align(settings_cancel_btn, LV_ALIGN_BOTTOM_RIGHT, -40, -20);
}

static void create_gear_button(lv_obj_t *parent)
{
    lv_obj_t *gear_btn = lv_btn_create(parent);
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

static void update_clock_hands(void)
{
    const int diameter = min_int(LV_HOR_RES, LV_VER_RES) - 24;
    const int radius = diameter / 2;
    const int hour_angle = (hour % 12) * 30 + (minute * 30) / 60;
    const int minute_angle = minute * 6 + (second * 6) / 60;
    const int second_angle = second * 6;

    update_hand(hour_hand, hour_hand_points, hour_angle, radius * 50 / 100);
    update_hand(minute_hand, minute_hand_points, minute_angle, radius * 70 / 100);
    update_hand(second_hand, second_hand_points, second_angle, radius * 88 / 100);
}

static void clock_timer_cb(lv_timer_t *timer)
{
    ARG_UNUSED(timer);

    second++;
    if (second >= 60) {
        second = 0;
        minute++;
        if (minute >= 60) {
            minute = 0;
            hour = (hour + 1) % 24;
            update_background_color();
        }
    }

    update_clock_hands();
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
    const int diameter = min_int(LV_HOR_RES, LV_VER_RES) - 24;
    const int marker_radius = diameter / 2 - 10;
    const int outer_radius = diameter / 2;

    background_obj = lv_obj_create(screen);
    lv_obj_set_size(background_obj, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(background_obj, get_background_color(hour, minute), 0);
    lv_obj_set_style_bg_opa(background_obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(background_obj, LV_OBJ_FLAG_SCROLLABLE);

    dial_outline = lv_obj_create(background_obj);
    lv_obj_set_size(dial_outline, diameter, diameter);
    lv_obj_align(dial_outline, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(dial_outline, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(dial_outline, lv_color_white(), 0);
    lv_obj_set_style_border_width(dial_outline, 5, 0);
    lv_obj_set_style_radius(dial_outline, LV_RADIUS_CIRCLE, 0);

    for (int i = 0; i < 12; i++) {
        lv_obj_t *marker = lv_line_create(background_obj);
        lv_obj_set_size(marker, diameter, diameter);
        lv_obj_set_style_line_width(marker, HOUR_MARKER_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_line_color(marker, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_line_rounded(marker, true, LV_PART_MAIN);

        const int angle = i * 30;
        const double rad = (90 - angle) * M_PI / 180.0;
        const int x_outer = diameter / 2 + (int)(cos(rad) * outer_radius);
        const int y_outer = diameter / 2 - (int)(sin(rad) * outer_radius);
        const int x_inner = diameter / 2 + (int)(cos(rad) * (outer_radius - HOUR_MARKER_LENGTH));
        const int y_inner = diameter / 2 - (int)(sin(rad) * (outer_radius - HOUR_MARKER_LENGTH));

        set_line_points(marker, hour_marker_points[i], x_inner, y_inner, x_outer, y_outer);
        hour_markers[i] = marker;

        lv_obj_t *number = lv_label_create(background_obj);
        lv_label_set_text_fmt(number, "%d", i == 0 ? 12 : i);
        lv_obj_set_style_text_color(number, lv_color_white(), 0);
        lv_obj_set_style_text_font(number, &lv_font_montserrat_14, 0);

        const int number_radius = outer_radius - 26;
        const int x_num = (int)(cos(rad) * number_radius);
        const int y_num = (int)(-sin(rad) * number_radius);
        lv_obj_align(number, LV_ALIGN_CENTER, x_num, y_num);
        hour_numbers[i] = number;
    }

    hour_hand = lv_line_create(background_obj);
    lv_obj_set_size(hour_hand, diameter, diameter);
    lv_obj_set_style_line_width(hour_hand, HOUR_HAND_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_line_color(hour_hand, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(hour_hand, true, LV_PART_MAIN);

    minute_hand = lv_line_create(background_obj);
    lv_obj_set_size(minute_hand, diameter, diameter);
    lv_obj_set_style_line_width(minute_hand, MINUTE_HAND_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_line_color(minute_hand, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(minute_hand, true, LV_PART_MAIN);

    second_hand = lv_line_create(background_obj);
    lv_obj_set_size(second_hand, diameter, diameter);
    lv_obj_set_style_line_width(second_hand, SECOND_HAND_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_line_color(second_hand, lv_color_make(0xDD, 0x33, 0x33), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(second_hand, true, LV_PART_MAIN);

    update_clock_hands();

    create_settings_menu(screen);
    create_gear_button(screen);

    lv_timer_create(clock_timer_cb, 1000, NULL);

    ret = display_blanking_off(display_dev);
    if (ret < 0 && ret != -ENOSYS) {
        LOG_ERR("Failed to turn display blanking off: %d", ret);
        return;
    }

    update_background_color();
    update_clock_hands();

    while (1) {
        lv_timer_handler();
        k_sleep(K_MSEC(20));
    }
}
