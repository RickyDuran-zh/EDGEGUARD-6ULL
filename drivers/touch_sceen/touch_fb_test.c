#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/input.h>

struct fb_ctx {
    int fd;
    void *mem;
    long size;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
};

static uint32_t scale_to_field(uint8_t value, struct fb_bitfield field)
{
    uint32_t max;

    if (field.length == 0)
        return 0;

    max = (1U << field.length) - 1;
    return ((value * max) / 255) << field.offset;
}

static uint32_t pack_color(struct fb_var_screeninfo *vinfo,
                           uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t pixel = 0;

    pixel |= scale_to_field(r, vinfo->red);
    pixel |= scale_to_field(g, vinfo->green);
    pixel |= scale_to_field(b, vinfo->blue);

    return pixel;
}

static void put_pixel(struct fb_ctx *fb, int x, int y,
                      uint8_t r, uint8_t g, uint8_t b)
{
    int bytes_per_pixel;
    long location;
    uint32_t pixel;

    if (x < 0 || y < 0 ||
        x >= (int)fb->vinfo.xres ||
        y >= (int)fb->vinfo.yres)
        return;

    bytes_per_pixel = fb->vinfo.bits_per_pixel / 8;
    location = (x + fb->vinfo.xoffset) * bytes_per_pixel +
               (y + fb->vinfo.yoffset) * fb->finfo.line_length;

    pixel = pack_color(&fb->vinfo, r, g, b);

    if (bytes_per_pixel == 4) {
        *((uint32_t *)((char *)fb->mem + location)) = pixel;
    } else if (bytes_per_pixel == 2) {
        *((uint16_t *)((char *)fb->mem + location)) = pixel & 0xffff;
    } else if (bytes_per_pixel == 3) {
        char *p = (char *)fb->mem + location;
        p[0] = pixel & 0xff;
        p[1] = (pixel >> 8) & 0xff;
        p[2] = (pixel >> 16) & 0xff;
    }
}

static void fill_screen(struct fb_ctx *fb, uint8_t r, uint8_t g, uint8_t b)
{
    unsigned int x, y;

    for (y = 0; y < fb->vinfo.yres; y++) {
        for (x = 0; x < fb->vinfo.xres; x++) {
            put_pixel(fb, x, y, r, g, b);
        }
    }
}

static void draw_hline(struct fb_ctx *fb, int y,
                       uint8_t r, uint8_t g, uint8_t b)
{
    int x;

    for (x = 0; x < (int)fb->vinfo.xres; x++)
        put_pixel(fb, x, y, r, g, b);
}

static void draw_vline(struct fb_ctx *fb, int x,
                       uint8_t r, uint8_t g, uint8_t b)
{
    int y;

    for (y = 0; y < (int)fb->vinfo.yres; y++)
        put_pixel(fb, x, y, r, g, b);
}

static void draw_grid(struct fb_ctx *fb)
{
    int x, y;

    fill_screen(fb, 0, 0, 0);

    for (x = 0; x < (int)fb->vinfo.xres; x += 100)
        draw_vline(fb, x, 45, 45, 45);

    for (y = 0; y < (int)fb->vinfo.yres; y += 80)
        draw_hline(fb, y, 45, 45, 45);

    draw_hline(fb, 0, 0, 255, 0);
    draw_hline(fb, fb->vinfo.yres - 1, 0, 255, 0);
    draw_vline(fb, 0, 0, 255, 0);
    draw_vline(fb, fb->vinfo.xres - 1, 0, 255, 0);
}

static void draw_cross(struct fb_ctx *fb, int x, int y)
{
    int i;

    draw_grid(fb);

    for (i = -25; i <= 25; i++) {
        put_pixel(fb, x + i, y, 255, 0, 0);
        put_pixel(fb, x, y + i, 255, 0, 0);
    }

    for (i = -8; i <= 8; i++) {
        put_pixel(fb, x + i, y - 8, 255, 255, 0);
        put_pixel(fb, x + i, y + 8, 255, 255, 0);
        put_pixel(fb, x - 8, y + i, 255, 255, 0);
        put_pixel(fb, x + 8, y + i, 255, 255, 0);
    }
}

static int fb_open(struct fb_ctx *fb, const char *fbdev)
{
    fb->fd = open(fbdev, O_RDWR);
    if (fb->fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", fbdev, strerror(errno));
        return -1;
    }

    if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->finfo) < 0) {
        fprintf(stderr, "FBIOGET_FSCREENINFO failed: %s\n", strerror(errno));
        close(fb->fd);
        return -1;
    }

    if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->vinfo) < 0) {
        fprintf(stderr, "FBIOGET_VSCREENINFO failed: %s\n", strerror(errno));
        close(fb->fd);
        return -1;
    }

    fb->size = fb->finfo.smem_len;

    fb->mem = mmap(NULL, fb->size,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED,
                   fb->fd, 0);

    if (fb->mem == MAP_FAILED) {
        fprintf(stderr, "mmap framebuffer failed: %s\n", strerror(errno));
        close(fb->fd);
        return -1;
    }

    printf("fb: %s, resolution=%ux%u, bpp=%u\n",
           fbdev,
           fb->vinfo.xres,
           fb->vinfo.yres,
           fb->vinfo.bits_per_pixel);

    return 0;
}

static void fb_close(struct fb_ctx *fb)
{
    if (fb->mem && fb->mem != MAP_FAILED)
        munmap(fb->mem, fb->size);

    if (fb->fd >= 0)
        close(fb->fd);
}

static int get_abs_range(int fd, int code, int *min, int *max)
{
    struct input_absinfo absinfo;

    if (ioctl(fd, EVIOCGABS(code), &absinfo) < 0)
        return -1;

    *min = absinfo.minimum;
    *max = absinfo.maximum;

    return 0;
}

static int scale_value(int value, int in_min, int in_max, int out_max)
{
    if (in_max <= in_min)
        return value;

    if (value < in_min)
        value = in_min;
    if (value > in_max)
        value = in_max;

    return (value - in_min) * (out_max - 1) / (in_max - in_min);
}

int main(int argc, char *argv[])
{
    const char *evdev = "/dev/input/event1";
    const char *fbdev = "/dev/fb0";
    struct fb_ctx fb;
    int evfd;
    struct input_event ev;
    int raw_x = 0;
    int raw_y = 0;
    int have_x = 0;
    int have_y = 0;
    int touch_down = 1;
    int x_min = 0, x_max = 799;
    int y_min = 0, y_max = 479;
    int x_code = ABS_MT_POSITION_X;
    int y_code = ABS_MT_POSITION_Y;

    memset(&fb, 0, sizeof(fb));
    fb.fd = -1;

    if (argc > 1)
        evdev = argv[1];
    if (argc > 2)
        fbdev = argv[2];

    if (fb_open(&fb, fbdev) < 0)
        return 1;

    evfd = open(evdev, O_RDONLY);
    if (evfd < 0) {
        fprintf(stderr, "open %s failed: %s\n", evdev, strerror(errno));
        fb_close(&fb);
        return 1;
    }

    if (get_abs_range(evfd, ABS_MT_POSITION_X, &x_min, &x_max) < 0) {
        x_code = ABS_X;
        get_abs_range(evfd, ABS_X, &x_min, &x_max);
    }

    if (get_abs_range(evfd, ABS_MT_POSITION_Y, &y_min, &y_max) < 0) {
        y_code = ABS_Y;
        get_abs_range(evfd, ABS_Y, &y_min, &y_max);
    }

    printf("input: %s\n", evdev);
    printf("x range: %d ~ %d, code=%s\n",
           x_min, x_max,
           x_code == ABS_MT_POSITION_X ? "ABS_MT_POSITION_X" : "ABS_X");
    printf("y range: %d ~ %d, code=%s\n",
           y_min, y_max,
           y_code == ABS_MT_POSITION_Y ? "ABS_MT_POSITION_Y" : "ABS_Y");
    printf("touch the screen, Ctrl+C to exit\n");

    draw_grid(&fb);

    while (1) {
        ssize_t n = read(evfd, &ev, sizeof(ev));
        if (n != sizeof(ev))
            continue;

        if (ev.type == EV_ABS) {
            if (ev.code == x_code) {
                raw_x = ev.value;
                have_x = 1;
            } else if (ev.code == y_code) {
                raw_y = ev.value;
                have_y = 1;
            } else if (ev.code == ABS_MT_TRACKING_ID) {
                touch_down = ev.value >= 0;
            }
        } else if (ev.type == EV_KEY) {
            if (ev.code == BTN_TOUCH)
                touch_down = ev.value;
        } else if (ev.type == EV_SYN) {
            if (have_x && have_y && touch_down) {
                int x = scale_value(raw_x, x_min, x_max, fb.vinfo.xres);
                int y = scale_value(raw_y, y_min, y_max, fb.vinfo.yres);

                printf("touch raw=(%d,%d), fb=(%d,%d)\n",
                       raw_x, raw_y, x, y);

                draw_cross(&fb, x, y);
                fflush(stdout);
            }
        }
    }

    close(evfd);
    fb_close(&fb);

    return 0;
}