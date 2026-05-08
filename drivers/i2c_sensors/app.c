#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define MPU6050_DEV   "/dev/mpu6050_raw"
#define AP3216C_DEV   "/dev/ap3216c_raw"
#define BUF_SIZE      512

static void msleep_user(unsigned int ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    nanosleep(&ts, NULL);
}

static int read_device_once(const char *dev_path, const char *dev_name)
{
    int fd;
    ssize_t ret;
    char buf[BUF_SIZE];

    fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[%s] open %s failed: %s\n",
                dev_name, dev_path, strerror(errno));

        if (errno == ENOENT) {
            fprintf(stderr, "  hint: device node does not exist. "
                            "Check whether the driver is loaded and probe succeeds.\n");
        } else if (errno == EACCES) {
            fprintf(stderr, "  hint: permission denied. Try running as root.\n");
        }

        return -1;
    }

    memset(buf, 0, sizeof(buf));

    ret = read(fd, buf, sizeof(buf) - 1);
    if (ret < 0) {
        fprintf(stderr, "[%s] read failed: %s\n",
                dev_name, strerror(errno));
        close(fd);
        return -1;
    }

    if (ret == 0) {
        printf("[%s] no data returned\n", dev_name);
    } else {
        buf[ret] = '\0';
        printf("========== %s ==========\n", dev_name);
        printf("%s", buf);

        if (buf[ret - 1] != '\n')
            printf("\n");
    }

    close(fd);
    return 0;
}

static void print_usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s [loop_count] [interval_ms]\n", prog);
    printf("\n");
    printf("Examples:\n");
    printf("  %s                 # read 10 times, interval 1000 ms\n", prog);
    printf("  %s 1               # read once\n", prog);
    printf("  %s 0 500           # read forever, interval 500 ms\n", prog);
    printf("  %s 20 200          # read 20 times, interval 200 ms\n", prog);
    printf("\n");
}

int main(int argc, char *argv[])
{
    int loop_count = 10;
    int interval_ms = 1000;
    int i = 0;

    if (argc > 1) {
        if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            print_usage(argv[0]);
            return 0;
        }

        loop_count = atoi(argv[1]);
        if (loop_count < 0) {
            fprintf(stderr, "Invalid loop_count: %s\n", argv[1]);
            return -1;
        }
    }

    if (argc > 2) {
        interval_ms = atoi(argv[2]);
        if (interval_ms <= 0) {
            fprintf(stderr, "Invalid interval_ms: %s\n", argv[2]);
            return -1;
        }
    }

    printf("MPU6050 device : %s\n", MPU6050_DEV);
    printf("AP3216C device : %s\n", AP3216C_DEV);
    printf("loop_count     : %d%s\n",
           loop_count,
           loop_count == 0 ? " (forever)" : "");
    printf("interval_ms    : %d\n", interval_ms);
    printf("\n");

    while (loop_count == 0 || i < loop_count) {
        printf("\n==================== sample %d ====================\n", i + 1);

        read_device_once(MPU6050_DEV, "MPU6050");
        read_device_once(AP3216C_DEV, "AP3216C");

        i++;

        if (loop_count == 0 || i < loop_count)
            msleep_user(interval_ms);
    }

    return 0;
}