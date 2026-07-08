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
static lv_obj_t *eye_lines[2];
static int eye_line_diameter;

static lv_point_precise_t hour_hand_points[2];
static lv_point_precise_t minute_hand_points[2];
static lv_point_precise_t second_hand_points[2];
static lv_point_precise_t hour_marker_points[12][2];
static lv_point_precise_t eye_points[2][3];

static lv_timer_t *clock_timer;
static int current_hour = INITIAL_HOUR;
static int current_minute = INITIAL_MINUTE;
static int current_second = INITIAL_SECOND;
static int day_hour = 8;
static int day_minute = 0;
static int night_hour = 21;
static int night_minute = 0;
static int weekend_day_hour = 9;
static int weekend_day_minute = 0;
static int weekend_night_hour = 22;
static int weekend_night_minute = 0;
static int current_day_of_week = 1; /* 0=Sunday ... 6=Saturday */
static int time_trim_ppm;

static inline int min_int(int a, int b)
{
    return a < b ? a : b;
}

static bool is_weekend(void)
{
    return current_day_of_week == 0 || current_day_of_week == 6;
}

static bool time_in_interval(int current, int start, int end)
{
    if (start <= end) {
        return current >= start && current <= end;
    }

    return current >= start || current <= end;
}

static void get_active_transition_times(int *active_day_hour, int *active_day_minute,
                                        int *active_night_hour, int *active_night_minute)
{
    if (is_weekend()) {
        *active_day_hour = weekend_day_hour;
        *active_day_minute = weekend_day_minute;
        *active_night_hour = weekend_night_hour;
        *active_night_minute = weekend_night_minute;
    } else {
        *active_day_hour = day_hour;
        *active_day_minute = day_minute;
        *active_night_hour = night_hour;
        *active_night_minute = night_minute;
    }
}

static lv_color_t get_background_color(int hour_value, int minute_value)
{
    int active_day_hour;
    int active_day_minute;
    int active_night_hour;
    int active_night_minute;
    get_active_transition_times(&active_day_hour, &active_day_minute,
                                &active_night_hour, &active_night_minute);

    int current_minutes = hour_value * 60 + minute_value;
    int day_transition = active_day_hour * 60 + active_day_minute;
    int night_transition = active_night_hour * 60 + active_night_minute;
    int day_end = (night_transition + 24 * 60 - 1) % (24 * 60);
    int night_end = (day_transition + 24 * 60 - 1) % (24 * 60);
    int dawn_start = (day_transition + 24 * 60 - 30) % (24 * 60);

    if (time_in_interval(current_minutes, day_transition, day_end)) {
        // return lv_color_make(0xB2, 0xD8, 0xB2);
        return lv_color_make(0, 0xFF, 0);
    }

    if (time_in_interval(current_minutes, dawn_start, (day_transition + 24 * 60 - 1) % (24 * 60))) {
        return lv_color_make(0xFF, 0x99, 0x33);
    }

    if (time_in_interval(current_minutes, night_transition, night_end)) {
        return lv_color_make(0x00, 0x12, 0x44);
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

static bool is_night_time(void)
{
    int active_day_hour;
    int active_day_minute;
    int active_night_hour;
    int active_night_minute;
    get_active_transition_times(&active_day_hour, &active_day_minute,
                                &active_night_hour, &active_night_minute);

    int current_minutes = current_hour * 60 + current_minute;
    int day_transition = active_day_hour * 60 + active_day_minute;
    int night_transition = active_night_hour * 60 + active_night_minute;
    int day_end = (night_transition + 24 * 60 - 1) % (24 * 60);

    return !time_in_interval(current_minutes, day_transition, day_end);
}

static bool is_dawn_transition(void)
{
    int active_day_hour;
    int active_day_minute;
    int active_night_hour;
    int active_night_minute;
    get_active_transition_times(&active_day_hour, &active_day_minute,
                                &active_night_hour, &active_night_minute);

    int current_minutes = current_hour * 60 + current_minute;
    int day_transition = active_day_hour * 60 + active_day_minute;
    int dawn_start = (day_transition + 24 * 60 - 30) % (24 * 60);

    return time_in_interval(current_minutes, dawn_start, (day_transition + 24 * 60 - 1) % (24 * 60));
}

static void update_eye_shape(void)
{
    if (!eye_lines[0] || !eye_lines[1]) {
        return;
    }

    int center = eye_line_diameter / 2;
    int eye_x = 40;
    int eye_y = center - eye_line_diameter / 6;
    int curve_height = is_night_time() ? 6 : -6;

    eye_points[0][0].x = center - eye_x - 12;
    eye_points[0][0].y = eye_y;
    eye_points[0][1].x = center - eye_x;
    eye_points[0][1].y = eye_y + curve_height;
    eye_points[0][2].x = center - eye_x + 12;
    eye_points[0][2].y = eye_y;

    eye_points[1][0].x = center + eye_x - 12;
    eye_points[1][0].y = eye_y;
    eye_points[1][1].x = center + eye_x;
    eye_points[1][1].y = eye_y + curve_height;
    eye_points[1][2].x = center + eye_x + 12;
    eye_points[1][2].y = eye_y;

    for (int i = 0; i < 2; i++) {
        lv_line_set_points(eye_lines[i], eye_points[i], 3);
        lv_obj_align(eye_lines[i], LV_ALIGN_CENTER, 0, 0);
    }
}

static void update_hand_colors(void)
{
    if (!hour_hand || !minute_hand) {
        return;
    }

    lv_color_t hand_color = (is_night_time() || is_dawn_transition()) ? lv_color_white() : lv_color_black();
    lv_obj_set_style_line_color(hour_hand, hand_color, LV_PART_MAIN);
    lv_obj_set_style_line_color(minute_hand, hand_color, LV_PART_MAIN);
}

static void update_background_color(void)
{
    if (background_obj) {
        lv_obj_set_style_bg_color(background_obj, get_background_color(current_hour, current_minute), 0);
    }
    update_hand_colors();
    update_eye_shape();
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

    clock_timer = lv_timer_create(graphics_tick, 1000, NULL);
    graphics_set_time_trim(0);

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

    eye_line_diameter = diameter;

    for (int i = 0; i < 2; i++) {
        eye_lines[i] = lv_line_create(background_obj);
        lv_obj_set_size(eye_lines[i], diameter, diameter);
        lv_obj_set_style_line_width(eye_lines[i], 4, LV_PART_MAIN);
        lv_obj_set_style_line_color(eye_lines[i], lv_color_make(0x88, 0x88, 0x88), LV_PART_MAIN);
        lv_obj_set_style_line_rounded(eye_lines[i], true, LV_PART_MAIN);
        lv_obj_align(eye_lines[i], LV_ALIGN_CENTER, 0, 0);
    }

    update_eye_shape();

    hour_hand = lv_line_create(background_obj);
    lv_obj_set_size(hour_hand, diameter, diameter);
    lv_obj_set_style_line_width(hour_hand, HOUR_HAND_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(hour_hand, true, LV_PART_MAIN);

    minute_hand = lv_line_create(background_obj);
    lv_obj_set_size(minute_hand, diameter, diameter);
    lv_obj_set_style_line_width(minute_hand, MINUTE_HAND_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(minute_hand, true, LV_PART_MAIN);

    second_hand = lv_line_create(background_obj);
    lv_obj_set_size(second_hand, diameter, diameter);
    lv_obj_set_style_line_width(second_hand, SECOND_HAND_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_line_color(second_hand, lv_color_make(0xDD, 0x33, 0x33), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(second_hand, true, LV_PART_MAIN);

    update_hand_colors();
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

void graphics_set_time_trim(int trim_ppm)
{
    const int min_trim_ppm = -5000;
    const int max_trim_ppm = 5000;
    int period_ms;

    time_trim_ppm = trim_ppm;
    if (time_trim_ppm < min_trim_ppm) {
        time_trim_ppm = min_trim_ppm;
    } else if (time_trim_ppm > max_trim_ppm) {
        time_trim_ppm = max_trim_ppm;
    }

    period_ms = 1000 + ((time_trim_ppm * 1000) / 1000000);
    if (period_ms < 500) {
        period_ms = 500;
    } else if (period_ms > 1500) {
        period_ms = 1500;
    }

    if (clock_timer) {
        lv_timer_set_period(clock_timer, period_ms);
    }
}

void graphics_set_transition_times(int day_hour_value, int day_minute_value, int night_hour_value, int night_minute_value)
{
    day_hour = day_hour_value;
    day_minute = day_minute_value;
    night_hour = night_hour_value;
    night_minute = night_minute_value;
    update_background_color();
}

void graphics_set_weekend_transition_times(int day_hour_value, int day_minute_value,
                                          int night_hour_value, int night_minute_value)
{
    weekend_day_hour = day_hour_value;
    weekend_day_minute = day_minute_value;
    weekend_night_hour = night_hour_value;
    weekend_night_minute = night_minute_value;
    update_background_color();
}

void graphics_set_day_of_week(int day_of_week)
{
    if (day_of_week < 0 || day_of_week > 6) {
        return;
    }
    current_day_of_week = day_of_week;
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

void graphics_get_time_trim(int *trim_ppm)
{
    if (trim_ppm) {
        *trim_ppm = time_trim_ppm;
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

void graphics_get_weekend_transition_times(int *day_hour_value, int *day_minute_value,
                                           int *night_hour_value, int *night_minute_value)
{
    if (day_hour_value) {
        *day_hour_value = weekend_day_hour;
    }
    if (day_minute_value) {
        *day_minute_value = weekend_day_minute;
    }
    if (night_hour_value) {
        *night_hour_value = weekend_night_hour;
    }
    if (night_minute_value) {
        *night_minute_value = weekend_night_minute;
    }
}

void graphics_get_day_of_week(int *day_of_week)
{
    if (day_of_week) {
        *day_of_week = current_day_of_week;
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
            if (current_hour == 0) {
                current_day_of_week = (current_day_of_week + 1) % 7;
            }
        }
        update_background_color();
    }
    update_clock_hands();
}
