#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>

#define APP_START_HOUR 8
#define APP_START_MINUTE 0
#define APP_START_SECOND 0

static uint32_t color_for_time(int hour, int minute)
{
    (void)minute;

    if (hour >= 8 && hour < 19) {
        return 0xB8F2B0;
    }

    if ((hour >= 21) || (hour < 8)) {
        return 0xBFBFBF;
    }

    return 0xFFB347;
}

static void put_pixel(uint8_t *framebuffer, uint16_t x, uint16_t y,
                      uint16_t width, uint16_t height,
                      uint8_t bytes_per_pixel,
                      enum display_pixel_format pixel_format,
                      uint32_t color)
{
    if (x >= width || y >= height) {
        return;
    }

    size_t offset = ((size_t)y * width + x) * bytes_per_pixel;

    switch (pixel_format) {
    case PIXEL_FORMAT_RGB_565: {
        uint16_t rgb565 = (uint16_t)(((color >> 16) & 0xF8) << 8 |
                                     ((color >> 8) & 0xFC) << 3 |
                                     (color & 0xF8) >> 3);
        memcpy(framebuffer + offset, &rgb565, sizeof(rgb565));
        break;
    }
    case PIXEL_FORMAT_RGB_888:
        framebuffer[offset + 0] = (uint8_t)((color >> 16) & 0xFF);
        framebuffer[offset + 1] = (uint8_t)((color >> 8) & 0xFF);
        framebuffer[offset + 2] = (uint8_t)(color & 0xFF);
        break;
    case PIXEL_FORMAT_ARGB_8888:
    default:
        memcpy(framebuffer + offset, &color, sizeof(color));
        break;
    }
}

static void draw_line(uint8_t *framebuffer, int x0, int y0, int x1, int y1,
                      uint16_t width, uint16_t height,
                      uint8_t bytes_per_pixel,
                      enum display_pixel_format pixel_format,
                      uint32_t color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        put_pixel(framebuffer, x0, y0, width, height, bytes_per_pixel,
                  pixel_format, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_circle(uint8_t *framebuffer, int cx, int cy, int radius,
                        uint16_t width, uint16_t height,
                        uint8_t bytes_per_pixel,
                        enum display_pixel_format pixel_format,
                        uint32_t color)
{
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        put_pixel(framebuffer, cx + x, cy + y, width, height, bytes_per_pixel,
                  pixel_format, color);
        put_pixel(framebuffer, cx + y, cy + x, width, height, bytes_per_pixel,
                  pixel_format, color);
        put_pixel(framebuffer, cx - y, cy + x, width, height, bytes_per_pixel,
                  pixel_format, color);
        put_pixel(framebuffer, cx - x, cy + y, width, height, bytes_per_pixel,
                  pixel_format, color);
        put_pixel(framebuffer, cx - x, cy - y, width, height, bytes_per_pixel,
                  pixel_format, color);
        put_pixel(framebuffer, cx - y, cy - x, width, height, bytes_per_pixel,
                  pixel_format, color);
        put_pixel(framebuffer, cx + y, cy - x, width, height, bytes_per_pixel,
                  pixel_format, color);
        put_pixel(framebuffer, cx + x, cy - y, width, height, bytes_per_pixel,
                  pixel_format, color);

        y += 1;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x -= 1;
            err += 1 - 2 * x;
        }
    }
}

static void draw_hand(uint8_t *framebuffer, int cx, int cy, int length,
                      int angle_deg, uint16_t width, uint16_t height,
                      uint8_t bytes_per_pixel,
                      enum display_pixel_format pixel_format,
                      uint32_t color)
{
    float angle = (angle_deg - 90) * (3.14159265f / 180.0f);
    int end_x = cx + (int)(length * cosf(angle));
    int end_y = cy + (int)(length * sinf(angle));
    draw_line(framebuffer, cx, cy, end_x, end_y, width, height, bytes_per_pixel,
              pixel_format, color);
}

static void render_clock(const struct device *display, uint16_t width,
                         uint16_t height,
                         enum display_pixel_format pixel_format,
                         uint8_t *framebuffer, int hour, int minute, int second)
{
    uint32_t bg = color_for_time(hour, minute);
    size_t bytes_per_pixel = 2;

    switch (pixel_format) {
    case PIXEL_FORMAT_RGB_888:
        bytes_per_pixel = 3;
        break;
    case PIXEL_FORMAT_ARGB_8888:
        bytes_per_pixel = 4;
        break;
    case PIXEL_FORMAT_RGB_565:
    default:
        bytes_per_pixel = 2;
        break;
    }

    size_t frame_size = (size_t)width * height * bytes_per_pixel;
    memset(framebuffer, 0, frame_size);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            put_pixel(framebuffer, x, y, width, height, bytes_per_pixel,
                      pixel_format, bg);
        }
    }

    int center_x = width / 2;
    int center_y = height / 2;
    int radius = (width < height ? width : height) / 2 - 20;

    draw_circle(framebuffer, center_x, center_y, radius, width, height,
                bytes_per_pixel, pixel_format, 0x000000);
    draw_circle(framebuffer, center_x, center_y, radius - 6, width, height,
                bytes_per_pixel, pixel_format, 0xFFFFFF);

    for (int tick = 0; tick < 12; tick++) {
        int angle = tick * 30 - 90;
        float radians = angle * (3.14159265f / 180.0f);
        int outer_x = center_x + (int)((radius - 8) * cosf(radians));
        int outer_y = center_y + (int)((radius - 8) * sinf(radians));
        int inner_x = center_x + (int)((radius - 20) * cosf(radians));
        int inner_y = center_y + (int)((radius - 20) * sinf(radians));
        draw_line(framebuffer, inner_x, inner_y, outer_x, outer_y, width, height,
                  bytes_per_pixel, pixel_format, 0x000000);
    }

    int hour_angle = (hour % 12) * 30 + minute / 2;
    int minute_angle = minute * 6 + second / 10;
    int second_angle = second * 6;

    draw_hand(framebuffer, center_x, center_y, radius / 2, hour_angle,
              width, height, bytes_per_pixel, pixel_format, 0x1F3A5F);
    draw_hand(framebuffer, center_x, center_y, radius - 20, minute_angle,
              width, height, bytes_per_pixel, pixel_format, 0x2E4053);
    draw_hand(framebuffer, center_x, center_y, radius - 10, second_angle,
              width, height, bytes_per_pixel, pixel_format, 0xD9534F);

    draw_circle(framebuffer, center_x, center_y, 7, width, height,
                bytes_per_pixel, pixel_format, 0x222222);

    struct display_buffer_descriptor desc = {
        .buf_size = frame_size,
        .width = width,
        .height = height,
        .pitch = width,
    };

    int ret = display_write(display, 0, 0, &desc, framebuffer);
    if (ret < 0) {
        printk("display_write failed: %d\n", ret);
    }
}

int main(void)
{
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) {
        printk("Display device is not ready\n");
        return 0;
    }

    struct display_capabilities capabilities;
    display_get_capabilities(display, &capabilities);

    uint16_t width = capabilities.x_resolution > 0 ? capabilities.x_resolution : 480;
    uint16_t height = capabilities.y_resolution > 0 ? capabilities.y_resolution : 272;

    size_t bytes_per_pixel = 2;
    switch (capabilities.current_pixel_format) {
    case PIXEL_FORMAT_RGB_888:
        bytes_per_pixel = 3;
        break;
    case PIXEL_FORMAT_ARGB_8888:
        bytes_per_pixel = 4;
        break;
    case PIXEL_FORMAT_RGB_565:
    default:
        bytes_per_pixel = 2;
        break;
    }

    size_t frame_size = (size_t)width * height * bytes_per_pixel;
    uint8_t *framebuffer = k_malloc(frame_size);
    if (framebuffer == NULL) {
        printk("Unable to allocate display frame buffer\n");
        return 0;
    }

    int hour = APP_START_HOUR;
    int minute = APP_START_MINUTE;
    int second = APP_START_SECOND;

    while (1) {
        render_clock(display, width, height, capabilities.current_pixel_format,
                     framebuffer, hour, minute, second);
        k_sleep(K_SECONDS(1));

        second++;
        if (second == 60) {
            second = 0;
            minute++;
        }
        if (minute == 60) {
            minute = 0;
            hour = (hour + 1) % 24;
        }
    }
}
