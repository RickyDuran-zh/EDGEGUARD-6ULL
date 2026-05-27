// lcd_fb_bl_test.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#define BACKLIGHT_CLASS_DIR "/sys/class/backlight"
#define PATH_SIZE 512

static int read_int_from_file(const char *path)
{
    int fd;
    char buf[64];
    ssize_t ret;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    ret = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (ret <= 0)
        return -1;

    buf[ret] = '\0';
    return atoi(buf);
}

static int write_int_to_file(const char *path, int value)
{
    int fd;
    char buf[64];
    int len;
    ssize_t ret;

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    len = snprintf(buf, sizeof(buf), "%d\n", value);
    ret = write(fd, buf, len);

    close(fd);

    if (ret != len) {
        fprintf(stderr, "write %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

static int find_backlight(char *brightness_path,
                          char *max_brightness_path,
                          size_t size)
{
    DIR *dir;
    struct dirent *ent;

    dir = opendir(BACKLIGHT_CLASS_DIR);
    if (!dir) {
        fprintf(stderr, "opendir %s failed: %s\n",
                BACKLIGHT_CLASS_DIR, strerror(errno));
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;

        snprintf(brightness_path, size,
                 "%s/%s/brightness",
                 BACKLIGHT_CLASS_DIR, ent->d_name);

        snprintf(max_brightness_path, size,
                 "%s/%s/max_brightness",
                 BACKLIGHT_CLASS_DIR, ent->d_name);

        closedir(dir);
        return 0;
    }

    closedir(dir);
    return -1;
}

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

static void fill_color(void *fbp,
                       struct fb_var_screeninfo *vinfo,
                       struct fb_fix_screeninfo *finfo,
                       uint8_t r, uint8_t g, uint8_t b)
{
    unsigned int x, y;
    int bytes_per_pixel = vinfo->bits_per_pixel / 8;
    uint32_t pixel = pack_color(vinfo, r, g, b);

    for (y = 0; y < vinfo->yres; y++) {
        for (x = 0; x < vinfo->xres; x++) {
            long location = (x + vinfo->xoffset) * bytes_per_pixel +
                            (y + vinfo->yoffset) * finfo->line_length;

            if (bytes_per_pixel == 4) {
                *((uint32_t *)((char *)fbp + location)) = pixel;
            } else if (bytes_per_pixel == 3) {
                char *p = (char *)fbp + location;
                p[0] = pixel & 0xff;
                p[1] = (pixel >> 8) & 0xff;
                p[2] = (pixel >> 16) & 0xff;
            } else if (bytes_per_pixel == 2) {
                *((uint16_t *)((char *)fbp + location)) = pixel & 0xffff;
            }
        }
    }
}

static int test_backlight(void)
{
    char brightness_path[PATH_SIZE];
    char max_brightness_path[PATH_SIZE];
    int max_brightness;
    int values[4];
    int i;

    printf("\n========== BACKLIGHT TEST ==========\n");

    if (find_backlight(brightness_path,
                       max_brightness_path,
                       sizeof(brightness_path)) < 0) {
        fprintf(stderr, "No backlight device found under %s\n",
                BACKLIGHT_CLASS_DIR);
        return -1;
    }

    max_brightness = read_int_from_file(max_brightness_path);
    if (max_brightness < 0) {
        fprintf(stderr, "read max_brightness failed\n");
        return -1;
    }

    printf("brightness path     : %s\n", brightness_path);
    printf("max_brightness path : %s\n", max_brightness_path);
    printf("max_brightness      : %d\n", max_brightness);

    values[0] = 0;
    values[1] = max_brightness / 4;
    values[2] = max_brightness / 2;
    values[3] = max_brightness;

    for (i = 0; i < 4; i++) {
        printf("set brightness = %d\n", values[i]);
        write_int_to_file(brightness_path, values[i]);
        sleep(1);
    }

    printf("restore brightness = %d\n", max_brightness);
    write_int_to_file(brightness_path, max_brightness);

    return 0;
}

static int test_framebuffer(const char *fbdev)
{
    int fd;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long screensize;
    void *fbp;

    printf("\n========== FRAMEBUFFER TEST ==========\n");

    fd = open(fbdev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", fbdev, strerror(errno));
        fprintf(stderr, "hint: check whether lcdif/mxsfb has registered /dev/fb0\n");
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        fprintf(stderr, "FBIOGET_FSCREENINFO failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        fprintf(stderr, "FBIOGET_VSCREENINFO failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    printf("fbdev      : %s\n", fbdev);
    printf("resolution : %ux%u\n", vinfo.xres, vinfo.yres);
    printf("virtual    : %ux%u\n", vinfo.xres_virtual, vinfo.yres_virtual);
    printf("bpp        : %u\n", vinfo.bits_per_pixel);
    printf("line_len   : %u\n", finfo.line_length);
    printf("smem_len   : %u\n", finfo.smem_len);
    printf("red        : offset=%u length=%u\n",
           vinfo.red.offset, vinfo.red.length);
    printf("green      : offset=%u length=%u\n",
           vinfo.green.offset, vinfo.green.length);
    printf("blue       : offset=%u length=%u\n",
           vinfo.blue.offset, vinfo.blue.length);

    screensize = finfo.smem_len;

    fbp = mmap(NULL, screensize, PROT_READ | PROT_WRITE,
               MAP_SHARED, fd, 0);
    if (fbp == MAP_FAILED) {
        fprintf(stderr, "mmap framebuffer failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    printf("fill red\n");
    fill_color(fbp, &vinfo, &finfo, 255, 0, 0);
    sleep(1);

    printf("fill green\n");
    fill_color(fbp, &vinfo, &finfo, 0, 255, 0);
    sleep(1);

    printf("fill blue\n");
    fill_color(fbp, &vinfo, &finfo, 0, 0, 255);
    sleep(1);

    printf("fill white\n");
    fill_color(fbp, &vinfo, &finfo, 255, 255, 255);
    sleep(1);

    printf("fill black\n");
    fill_color(fbp, &vinfo, &finfo, 0, 0, 0);

    munmap(fbp, screensize);
    close(fd);

    return 0;
}

int main(int argc, char *argv[])
{
    const char *fbdev = "/dev/fb0";
    int ret = 0;

    if (argc > 1)
        fbdev = argv[1];

    printf("LCD framebuffer and backlight test\n");

    if (test_backlight() < 0)
        ret = 1;

    if (test_framebuffer(fbdev) < 0)
        ret = 1;

    if (ret == 0)
        printf("\n[RESULT] LCD display path test finished\n");
    else
        printf("\n[RESULT] Some tests failed, check messages above\n");

    return ret;
}