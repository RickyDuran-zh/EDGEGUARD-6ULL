// edge_io_test.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <linux/input.h>

#define EDGE_LEDS_DEV      "/dev/edge_leds"
#define EDGE_BUZZER_DEV    "/dev/edge_buzzer"

#define DEFAULT_KEY_WAIT_SEC  15
#define BUF_SIZE              256

static void msleep_user(unsigned int ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    while (nanosleep(&ts, &ts) < 0 && errno == EINTR)
        ;
}

static int write_text_device(const char *dev_path, const char *cmd)
{
    int fd;
    ssize_t ret;
    char buf[BUF_SIZE];

    fd = open(dev_path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[ERR] open %s failed: %s\n",
                dev_path, strerror(errno));
        return -1;
    }

    snprintf(buf, sizeof(buf), "%s\n", cmd);

    ret = write(fd, buf, strlen(buf));
    if (ret < 0) {
        fprintf(stderr, "[ERR] write \"%s\" to %s failed: %s\n",
                cmd, dev_path, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);

    printf("[OK] %s <= \"%s\"\n", dev_path, cmd);
    return 0;
}

static int test_leds(void)
{
    const char *cmds[] = {
        "user on",
        "user off",
        "red",
        "green",
        "blue",
        "yellow",
        "cyan",
        "magenta",
        "white",
        "off",
    };

    int i;
    int ret = 0;

    printf("\n========== LED TEST ==========\n");

    for (i = 0; i < (int)(sizeof(cmds) / sizeof(cmds[0])); i++) {
        if (write_text_device(EDGE_LEDS_DEV, cmds[i]) < 0)
            ret = -1;

        msleep_user(700);
    }

    write_text_device(EDGE_LEDS_DEV, "off");

    return ret;
}

static int test_buzzer(void)
{
    int ret = 0;

    printf("\n========== BUZZER TEST ==========\n");

    if (write_text_device(EDGE_BUZZER_DEV, "beep") < 0)
        ret = -1;

    msleep_user(800);

    if (write_text_device(EDGE_BUZZER_DEV, "on") < 0)
        ret = -1;

    msleep_user(1000);

    if (write_text_device(EDGE_BUZZER_DEV, "off") < 0)
        ret = -1;

    msleep_user(500);

    return ret;
}

/*
 * Try to find input event path from /proc/bus/input/devices.
 * It searches devices whose Name contains "edge_key" or "edge_keys".
 */
static int find_edge_key_event(char *out_path, size_t out_size)
{
    FILE *fp;
    char line[256];
    int in_target = 0;

    fp = fopen("/proc/bus/input/devices", "r");
    if (!fp) {
        fprintf(stderr, "[WARN] open /proc/bus/input/devices failed: %s\n",
                strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == 'N' && strstr(line, "Name=")) {
            if (strstr(line, "edge_key") || strstr(line, "edge_keys"))
                in_target = 1;
            else
                in_target = 0;
        }

        if (in_target && line[0] == 'H' && strstr(line, "Handlers=")) {
            char *p = strstr(line, "event");

            if (p) {
                int event_num = -1;

                if (sscanf(p, "event%d", &event_num) == 1 &&
                    event_num >= 0) {
                    snprintf(out_path, out_size,
                             "/dev/input/event%d", event_num);
                    fclose(fp);
                    return 0;
                }
            }
        }
    }

    fclose(fp);
    return -1;
}

static const char *key_code_to_name(unsigned int code)
{
    switch (code) {
    case KEY_ENTER:
        return "KEY_ENTER";
    case KEY_SPACE:
        return "KEY_SPACE";
    case KEY_POWER:
        return "KEY_POWER";
    default:
        return "KEY_UNKNOWN";
    }
}

static int test_key(const char *key_path, int wait_sec)
{
    int fd;
    int ret;
    time_t start;
    struct input_event ev;

    printf("\n========== KEY TEST ==========\n");

    fd = open(key_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "[ERR] open %s failed: %s\n",
                key_path, strerror(errno));
        fprintf(stderr, "      hint: check /proc/bus/input/devices and run as root if needed.\n");
        return -1;
    }

    printf("[INFO] key device: %s\n", key_path);
    printf("[INFO] please press/release the key within %d seconds...\n", wait_sec);

    start = time(NULL);

    while (time(NULL) - start < wait_sec) {
        fd_set rfds;
        struct timeval tv;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR)
                continue;

            fprintf(stderr, "[ERR] select failed: %s\n", strerror(errno));
            close(fd);
            return -1;
        }

        if (ret == 0)
            continue;

        while (1) {
            ssize_t n;

            n = read(fd, &ev, sizeof(ev));
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;

                fprintf(stderr, "[ERR] read input event failed: %s\n",
                        strerror(errno));
                close(fd);
                return -1;
            }

            if (n != sizeof(ev))
                break;

            if (ev.type == EV_KEY) {
                printf("[KEY] code=%u(%s), value=%d, %s\n",
                       ev.code,
                       key_code_to_name(ev.code),
                       ev.value,
                       ev.value == 1 ? "pressed" :
                       ev.value == 0 ? "released" : "repeat");
            } else if (ev.type == EV_SYN) {
                printf("[SYN] sync\n");
            }
        }
    }

    close(fd);
    printf("[INFO] key test finished\n");

    return 0;
}

static void print_usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --key <event_dev>       specify key input device, e.g. /dev/input/event1\n");
    printf("  --key-timeout <sec>     key test timeout, default %d seconds\n",
           DEFAULT_KEY_WAIT_SEC);
    printf("  --no-led                skip LED test\n");
    printf("  --no-buzzer             skip buzzer test\n");
    printf("  --no-key                skip key test\n");
    printf("  -h, --help              show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s\n", prog);
    printf("  %s --key /dev/input/event1\n", prog);
    printf("  %s --no-key\n", prog);
}

int main(int argc, char *argv[])
{
    char key_path[128] = {0};
    int key_timeout = DEFAULT_KEY_WAIT_SEC;
    int do_led = 1;
    int do_buzzer = 1;
    int do_key = 1;
    int i;
    int ret = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argv[0]);
            return 0;
        } else if (!strcmp(argv[i], "--key")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--key requires argument\n");
                return 1;
            }
            snprintf(key_path, sizeof(key_path), "%s", argv[++i]);
        } else if (!strcmp(argv[i], "--key-timeout")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--key-timeout requires argument\n");
                return 1;
            }
            key_timeout = atoi(argv[++i]);
            if (key_timeout <= 0)
                key_timeout = DEFAULT_KEY_WAIT_SEC;
        } else if (!strcmp(argv[i], "--no-led")) {
            do_led = 0;
        } else if (!strcmp(argv[i], "--no-buzzer")) {
            do_buzzer = 0;
        } else if (!strcmp(argv[i], "--no-key")) {
            do_key = 0;
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    printf("========================================\n");
    printf(" EdgeGuard IO Test\n");
    printf("========================================\n");
    printf("LED device    : %s\n", EDGE_LEDS_DEV);
    printf("Buzzer device : %s\n", EDGE_BUZZER_DEV);

    if (do_key) {
        if (key_path[0] == '\0') {
            if (find_edge_key_event(key_path, sizeof(key_path)) < 0) {
                printf("[WARN] cannot auto-find edge key event device\n");
                printf("[WARN] use: %s --key /dev/input/eventX\n", argv[0]);
                do_key = 0;
            }
        }

        if (do_key)
            printf("Key device    : %s\n", key_path);
    }

    printf("========================================\n");

    if (do_led) {
        if (test_leds() < 0)
            ret = 1;
    }

    if (do_buzzer) {
        if (test_buzzer() < 0)
            ret = 1;
    }

    if (do_key) {
        if (test_key(key_path, key_timeout) < 0)
            ret = 1;
    }

    printf("\n========== TEST DONE ==========\n");

    if (ret == 0)
        printf("[RESULT] all selected tests finished\n");
    else
        printf("[RESULT] some tests failed, check logs above\n");

    return ret;
}