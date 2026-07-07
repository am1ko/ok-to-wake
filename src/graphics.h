#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <lvgl.h>

void graphics_init(lv_obj_t *screen);
void graphics_tick(lv_timer_t *timer);
void graphics_set_time(int hour, int minute, int second);
void graphics_set_transition_times(int day_hour, int day_minute, int night_hour, int night_minute);
void graphics_get_time(int *hour, int *minute, int *second);
void graphics_get_transition_times(int *day_hour, int *day_minute, int *night_hour, int *night_minute);

#endif /* GRAPHICS_H */
