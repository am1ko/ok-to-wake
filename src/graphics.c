#include "graphics.h"
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

static lv_point_precise_t hour_hand_points[2];
static lv_point_precise_t minute_hand_points[2];
static lv_point_precise_t second_hand_points[2];
static lv_point_precise_t hour_marker_points[12][2];

static int current_hour = INITIAL_HOUR;
static int current_minute = INITIAL_MINUTE;
static int current_second = INITIAL_SECOND;
static int day_hour = 8;
static int day_minute = 0;
static int night_hour = 21;
static int night_minute = 0;

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

static lv_color_t get_background_color(int hour_value, int minute_value)
{
    int current_minutes = hour_value * 60 + minute_value;
    int day_transition = day_hour * 60 + day_minute;
    int night_transition = night_hour * 60 + night_minute;
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
    if (background_obj) {
        lv_obj_set_style_bg_color(background_obj, get_background_color(current_hour, current_minute), 0);
    }
}

static void update_clock_hands(void)
{
    const int diameter = min_int(LV_HOR_RES, LV_VER_RES) - 24;
    const int radius = diameter / 2;
    const int hour_angle = (current_hour % 12) * 30 + (current_minute * 30) / 60;
    const int minute_angle = current_minute * 6 + (current_second * 6) / 60;
    const int second_angle = current_second * 6;

    update_hand(hour_hand, hour_hand_points, hour_angle, radius * 50 / 100);
    update_hand(minute_hand, minute_hand_points, minute_angle, radius * 70 / 100);
    update_hand(second_hand, second_hand_points, second_angle, radius * 88 / 100);
}

void graphics_init(lv_obj_t *screen)
{
    const int diameter = min_int(LV_HOR_RES, LV_VER_RES) - 24;
    const int outer_radius = diameter / 2;

    background_obj = lv_obj_create(screen);
    lv_obj_set_size(background_obj, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(background_obj, get_background_color(current_hour, current_minute), 0);
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
}

void graphics_set_time(int hour_value, int minute_value, int second_value)
{
    current_hour = hour_value;
    current_minute = minute_value;
    current_second = second_value;
    update_background_color();
    update_clock_hands();
}

void graphics_set_transition_times(int day_hour_value, int day_minute_value, int night_hour_value, int night_minute_value)
{
    day_hour = day_hour_value;
    day_minute = day_minute_value;
    night_hour = night_hour_value;
    night_minute = night_minute_value;
    update_background_color();
}

void graphics_get_time(int *hour_value, int *minute_value, int *second_value)
{
    if (hour_value) {
        *hour_value = current_hour;
    }
    if (minute_value) {
        *minute_value = current_minute;
    }
    if (second_value) {
        *second_value = current_second;
    }
}

void graphics_get_transition_times(int *day_hour_value, int *day_minute_value, int *night_hour_value, int *night_minute_value)
{
    if (day_hour_value) {
        *day_hour_value = day_hour;
    }
    if (day_minute_value) {
        *day_minute_value = day_minute;
    }
    if (night_hour_value) {
        *night_hour_value = night_hour;
    }
    if (night_minute_value) {
        *night_minute_value = night_minute;
    }
}

void graphics_tick(lv_timer_t *timer)
{
    ARG_UNUSED(timer);

    current_second++;
    if (current_second >= 60) {
        current_second = 0;
        current_minute++;
        if (current_minute >= 60) {
            current_minute = 0;
            current_hour = (current_hour + 1) % 24;
        }
        update_background_color();
    }
    update_clock_hands();
}
