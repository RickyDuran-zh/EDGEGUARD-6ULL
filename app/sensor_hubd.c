// sensor_hubd.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>

#define MPU6050_DEV      "/dev/mpu6050_raw"
#define AP3216C_DEV      "/dev/ap3216c_raw"
#define EDGE_LEDS_DEV    "/dev/edge_leds"
#define EDGE_BUZZER_DEV  "/dev/edge_buzzer"

#define BUF_SIZE         512

#define DEFAULT_INTERVAL_MS      500
#define DEFAULT_ALS_LOW_TH       80
#define DEFAULT_PS_HIGH_TH       200
#define DEFAULT_MOTION_DELTA_TH  12000
#define BUZZER_BEEP_INTERVAL_SEC 2

static volatile int g_running = 1;

struct mpu6050_data {
    int ax;
    int ay;
    int az;
    int temp;
    int gx;
    int gy;
    int gz;
    int valid;
};

struct ap3216c_data {
    int ir;
    int als;
    int ps;
    int valid;
};

enum alarm_state {
    STATE_NORMAL = 0,
    STATE_LIGHT_ALARM,
    STATE_PROXIMITY_ALARM,
    STATE_MOTION_ALARM,
};

struct app_config {
    int interval_ms;
    int als_low_th;
    int ps_high_th;
    int motion_delta_th;
    char key_dev[128];
};

static const char *state_to_string(enum alarm_state state)
{
    switch (state) {
    case STATE_NORMAL:
        return "NORMAL";
    case STATE_LIGHT_ALARM:
        return "LIGHT_ALARM";
    case STATE_PROXIMITY_ALARM:
        return "PROXIMITY_ALARM";
    case STATE_MOTION_ALARM:
        return "MOTION_ALARM";
    default:
        return "UNKNOWN";
    }
}

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static void msleep_user(unsigned int ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    while (nanosleep(&ts, &ts) < 0 && errno == EINTR)
        ;
}

static int read_text_device(const char *path, char *buf, size_t size)
{
    int fd;
    ssize_t ret;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[ERR] open %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    memset(buf, 0, size);

    ret = read(fd, buf, size - 1);
    if (ret < 0) {
        fprintf(stderr, "[ERR] read %s failed: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }

    buf[ret] = '\0';
    close(fd);

    return 0;
}

static int write_text_device(const char *path, const char *cmd)
{
    int fd;
    ssize_t ret;
    char buf[128];

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[ERR] open %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    snprintf(buf, sizeof(buf), "%s\n", cmd);

    ret = write(fd, buf, strlen(buf));
    if (ret < 0) {
        fprintf(stderr, "[ERR] write %s to %s failed: %s\n",
                cmd, path, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int parse_mpu6050(const char *buf, struct mpu6050_data *data)
{
    int ret;

    memset(data, 0, sizeof(*data));

    ret = sscanf(buf,
                 "accel_raw: %d %d %d\n"
                 "temp_raw: %d\n"
                 "gyro_raw: %d %d %d",
                 &data->ax, &data->ay, &data->az,
                 &data->temp,
                 &data->gx, &data->gy, &data->gz);

    if (ret != 7) {
        fprintf(stderr, "[WARN] parse MPU6050 failed, ret=%d, raw=\n%s\n",
                ret, buf);
        return -1;
    }

    data->valid = 1;
    return 0;
}

static int parse_ap3216c(const char *buf, struct ap3216c_data *data)
{
    int ret;

    memset(data, 0, sizeof(*data));

    ret = sscanf(buf,
                 "ir_raw: %d\n"
                 "als_raw: %d\n"
                 "ps_raw: %d",
                 &data->ir, &data->als, &data->ps);

    if (ret != 3) {
        fprintf(stderr, "[WARN] parse AP3216C failed, ret=%d, raw=\n%s\n",
                ret, buf);
        return -1;
    }

    data->valid = 1;
    return 0;
}

static int read_mpu6050(struct mpu6050_data *data)
{
    char buf[BUF_SIZE];

    if (read_text_device(MPU6050_DEV, buf, sizeof(buf)) < 0)
        return -1;

    return parse_mpu6050(buf, data);
}

static int read_ap3216c(struct ap3216c_data *data)
{
    char buf[BUF_SIZE];

    if (read_text_device(AP3216C_DEV, buf, sizeof(buf)) < 0)
        return -1;

    return parse_ap3216c(buf, data);
}

static int calc_motion_delta(const struct mpu6050_data *cur,
                             const struct mpu6050_data *prev)
{
    int delta = 0;

    if (!cur->valid || !prev->valid)
        return 0;

    delta += abs(cur->ax - prev->ax);
    delta += abs(cur->ay - prev->ay);
    delta += abs(cur->az - prev->az);
    delta += abs(cur->gx - prev->gx);
    delta += abs(cur->gy - prev->gy);
    delta += abs(cur->gz - prev->gz);

    return delta;
}

static enum alarm_state evaluate_state(const struct app_config *cfg,
                                       const struct mpu6050_data *mpu,
                                       const struct mpu6050_data *prev_mpu,
                                       const struct ap3216c_data *ap,
                                       int *motion_delta_out)
{
    int motion_delta = 0;

    if (mpu->valid && prev_mpu->valid)
        motion_delta = calc_motion_delta(mpu, prev_mpu);

    if (motion_delta_out)
        *motion_delta_out = motion_delta;

    if (mpu->valid && prev_mpu->valid &&
        motion_delta > cfg->motion_delta_th) {
        return STATE_MOTION_ALARM;
    }

    if (ap->valid && ap->ps >= 0 && ap->ps > cfg->ps_high_th) {
        return STATE_PROXIMITY_ALARM;
    }

    if (ap->valid && ap->als >= 0 && ap->als < cfg->als_low_th) {
        return STATE_LIGHT_ALARM;
    }

    return STATE_NORMAL;
}

static void apply_output(enum alarm_state state, enum alarm_state last_state)
{
    if (state == last_state)
        return;

    switch (state) {
    case STATE_NORMAL:
        write_text_device(EDGE_LEDS_DEV, "green");
        write_text_device(EDGE_BUZZER_DEV, "off");
        break;

    case STATE_LIGHT_ALARM:
        write_text_device(EDGE_LEDS_DEV, "yellow");
        write_text_device(EDGE_BUZZER_DEV, "off");
        break;

    case STATE_PROXIMITY_ALARM:
        write_text_device(EDGE_LEDS_DEV, "blue");
        write_text_device(EDGE_BUZZER_DEV, "off");
        break;

    case STATE_MOTION_ALARM:
        write_text_device(EDGE_LEDS_DEV, "red");
        break;

    default:
        write_text_device(EDGE_LEDS_DEV, "off");
        write_text_device(EDGE_BUZZER_DEV, "off");
        break;
    }
}

static int open_key_device(const char *path)
{
    int fd;

    if (path[0] == '\0')
        return -1;

    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "[WARN] open key device %s failed: %s\n",
                path, strerror(errno));
        return -1;
    }

    printf("[INFO] key device opened: %s\n", path);
    return fd;
}

static int process_key_events(int key_fd)
{
    struct input_event ev;
    ssize_t ret;
    int key_pressed = 0;

    if (key_fd < 0)
        return 0;

    while (1) {
        ret = read(key_fd, &ev, sizeof(ev));
        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;

            fprintf(stderr, "[WARN] read key event failed: %s\n",
                    strerror(errno));
            break;
        }

        if (ret != sizeof(ev))
            break;

        if (ev.type == EV_KEY && ev.value == 1) {
            key_pressed = 1;
        }
    }

    return key_pressed;
}

static void print_status(const struct mpu6050_data *mpu,
                         const struct ap3216c_data *ap,
                         enum alarm_state state,
                         int motion_delta)
{
    printf("[EdgeGuard] state=%s, motion_delta=%d\n",
           state_to_string(state), motion_delta);

    if (mpu->valid) {
        printf("  MPU6050: accel=(%d,%d,%d), temp_raw=%d, gyro=(%d,%d,%d)\n",
               mpu->ax, mpu->ay, mpu->az,
               mpu->temp,
               mpu->gx, mpu->gy, mpu->gz);
    } else {
        printf("  MPU6050: invalid\n");
    }

    if (ap->valid) {
        printf("  AP3216C: ir=%d, als=%d, ps=%d\n",
               ap->ir, ap->als, ap->ps);
    } else {
        printf("  AP3216C: invalid\n");
    }

    fflush(stdout);
}

static void print_usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --key <dev>          input event device, e.g. /dev/input/event1\n");
    printf("  --interval <ms>      sample interval, default %d ms\n",
           DEFAULT_INTERVAL_MS);
    printf("  --als-low <value>    ALS low threshold, default %d\n",
           DEFAULT_ALS_LOW_TH);
    printf("  --ps-high <value>    PS high threshold, default %d\n",
           DEFAULT_PS_HIGH_TH);
    printf("  --motion <value>     motion delta threshold, default %d\n",
           DEFAULT_MOTION_DELTA_TH);
    printf("  -h, --help           show this help\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s --key /dev/input/event1 --interval 500\n", prog);
}

static int parse_args(int argc, char *argv[], struct app_config *cfg)
{
    int i;

    cfg->interval_ms = DEFAULT_INTERVAL_MS;
    cfg->als_low_th = DEFAULT_ALS_LOW_TH;
    cfg->ps_high_th = DEFAULT_PS_HIGH_TH;
    cfg->motion_delta_th = DEFAULT_MOTION_DELTA_TH;
    cfg->key_dev[0] = '\0';

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argv[0]);
            exit(0);
        } else if (!strcmp(argv[i], "--key")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--key requires argument\n");
                return -1;
            }
            snprintf(cfg->key_dev, sizeof(cfg->key_dev), "%s", argv[++i]);
        } else if (!strcmp(argv[i], "--interval")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--interval requires argument\n");
                return -1;
            }
            cfg->interval_ms = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--als-low")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--als-low requires argument\n");
                return -1;
            }
            cfg->als_low_th = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--ps-high")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--ps-high requires argument\n");
                return -1;
            }
            cfg->ps_high_th = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--motion")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--motion requires argument\n");
                return -1;
            }
            cfg->motion_delta_th = atoi(argv[++i]);
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            return -1;
        }
    }

    if (cfg->interval_ms <= 0)
        cfg->interval_ms = DEFAULT_INTERVAL_MS;

    return 0;
}

int main(int argc, char *argv[])
{
    struct app_config cfg;
    struct mpu6050_data mpu;
    struct mpu6050_data prev_mpu;
    struct ap3216c_data ap;
    enum alarm_state state = STATE_NORMAL;
    enum alarm_state last_state = -1;
    int motion_delta = 0;
    int key_fd = -1;
    time_t last_beep_time = 0;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (parse_args(argc, argv, &cfg) < 0) {
        print_usage(argv[0]);
        return 1;
    }

    memset(&mpu, 0, sizeof(mpu));
    memset(&prev_mpu, 0, sizeof(prev_mpu));
    memset(&ap, 0, sizeof(ap));

    printf("[INFO] sensor_hubd started\n");
    printf("[INFO] interval=%d ms, als_low=%d, ps_high=%d, motion_th=%d\n",
           cfg.interval_ms, cfg.als_low_th,
           cfg.ps_high_th, cfg.motion_delta_th);

    if (cfg.key_dev[0] != '\0')
        key_fd = open_key_device(cfg.key_dev);
    else
        printf("[WARN] no key device specified, key function disabled\n");

    write_text_device(EDGE_LEDS_DEV, "green");
    write_text_device(EDGE_BUZZER_DEV, "off");

    while (g_running) {
        time_t now;
        int key_pressed;

        prev_mpu = mpu;

        read_mpu6050(&mpu);
        read_ap3216c(&ap);

        key_pressed = process_key_events(key_fd);
        if (key_pressed) {
            printf("[INFO] key pressed: clear alarm\n");
            state = STATE_NORMAL;
            write_text_device(EDGE_LEDS_DEV, "green");
            write_text_device(EDGE_BUZZER_DEV, "off");
            last_state = -1;
        } else {
            state = evaluate_state(&cfg, &mpu, &prev_mpu, &ap, &motion_delta);
        }

        apply_output(state, last_state);

        now = time(NULL);
        if (state == STATE_MOTION_ALARM &&
            now - last_beep_time >= BUZZER_BEEP_INTERVAL_SEC) {
            write_text_device(EDGE_BUZZER_DEV, "beep");
            last_beep_time = now;
        }

        print_status(&mpu, &ap, state, motion_delta);

        last_state = state;
        msleep_user(cfg.interval_ms);
    }

    printf("[INFO] sensor_hubd stopping\n");

    write_text_device(EDGE_BUZZER_DEV, "off");
    write_text_device(EDGE_LEDS_DEV, "off");

    if (key_fd >= 0)
        close(key_fd);

    return 0;
}