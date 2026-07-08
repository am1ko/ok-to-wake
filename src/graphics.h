#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <lvgl.h>

void graphics_init(lv_obj_t *screen);
void graphics_tick(lv_timer_t *timer);
void graphics_set_time(int hour, int minute, int second);
void graphics_set_time_trim(int trim_ppm);
void graphics_set_transition_times(int day_hour, int day_minute, int night_hour, int night_minute);
void graphics_set_weekend_transition_times(int day_hour, int day_minute, int night_hour, int night_minute);
void graphics_get_time(int *hour, int *minute, int *second);
void graphics_get_time_trim(int *trim_ppm);
void graphics_get_transition_times(int *day_hour, int *day_minute, int *night_hour, int *night_minute);
void graphics_get_weekend_transition_times(int *day_hour, int *day_minute, int *night_hour, int *night_minute);
void graphics_set_day_of_week(int day_of_week);
void graphics_get_day_of_week(int *day_of_week);

#endif /* GRAPHICS_H */
