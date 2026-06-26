// edgeguard_visiond.c — EdgeGuard Vision Daemon
// Periodically captures frames from USB UVC camera, performs simple
// motion detection, and writes /tmp/edgeguard_vision.json.
// IPC: other processes (sensor_hubd, httpd, ui) read the JSON file.

#include "camera_v4l2.h"
#include "face_detect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ---- defaults ---- */
#define DEFAULT_DEVICE          "/dev/video0"
#define DEFAULT_INTERVAL_MS     2000
#define DEFAULT_WIDTH           640
#define DEFAULT_HEIGHT          480
#define SNAPSHOT_DIR            "/var/log/edgeguard/snapshots"
#define VISION_JSON_PATH        "/tmp/edgeguard_vision.json"
#define VISION_JSON_TMP         "/tmp/edgeguard_vision.json.tmp"
#define FACE_COUNT_FILE         "/var/log/edgeguard/face_count.dat"
#define MAX_SNAPSHOTS           50         /* keep at most this many snapshots */

/* Motion detection: JPEG-size-based heuristic.
   When the scene changes (person walks in, lighting change), the JPEG
   encoder produces a frame with different size.  This is not pixel-accurate
   but works as a lightweight "something changed" detector without requiring
   a JPEG decoder or AI model. */
#define MOTION_SIZE_THRESHOLD   15        /* percent change to trigger */

/* ---- visiond operation modes ---- */
typedef enum {
    MODE_MONITOR   = 0,  /* motion + face + tamper detection */
    MODE_TAMPER    = 1,  /* motion + face + occlusion detection */
} vision_mode_t;

/* Occlusion detection: consecutive frames below this size → tamper */
#define TAMPER_SIZE_THRESHOLD   5120      /* 5 KB — solid black/white frame */
#define TAMPER_CONSEC_COUNT     3         /* N consecutive small frames → alarm */

/* ---- global state ---- */
static volatile int g_running = 1;
static char   g_device[256];
static int    g_interval_ms = DEFAULT_INTERVAL_MS;
static vision_mode_t g_mode = MODE_MONITOR;
static int    g_tamper_streak = 0;  /* consecutive small frames in tamper mode */
static time_t g_last_cmd_mtime = 0; /* for cmd file change detection */
static int    g_last_face_count = 0;    /* latched face count */
static int    g_face_latch_left = 0;    /* remaining cycles to hold latch */
static int    g_total_face_count = 0;   /* cumulative faces detected, never resets */
static char   g_last_face_snap[512];    /* snapshot path from last face detection */

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ---- cumulative face count persistence ---- */
static void save_face_count(void)
{
    FILE *fp = fopen(FACE_COUNT_FILE, "w");
    if (fp) {
        fprintf(fp, "%d\n", g_total_face_count);
        fclose(fp);
    }
}

static void load_face_count(void)
{
    FILE *fp = fopen(FACE_COUNT_FILE, "r");
    if (fp) {
        int saved = 0;
        if (fscanf(fp, "%d", &saved) == 1 && saved > 0)
            g_total_face_count = saved;
        fclose(fp);
        printf("[visiond] restored total_face_count=%d from %s\n",
               g_total_face_count, FACE_COUNT_FILE);
    }
}

static void msleep_user(unsigned int ms)
{
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) ;
}

/* ---- helpers ---- */
static int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        return -1;
    }
    return mkdir(path, 0755);
}

/* ---- cmd channel ---- */
#define CMD_JSON_PATH       "/tmp/edgeguard_cmd.json"

static void check_cmd_file(void)
{
    struct stat st;
    if (stat(CMD_JSON_PATH, &st) != 0) {
        g_last_cmd_mtime = 0;
        return;
    }
    if (st.st_mtime == g_last_cmd_mtime) return;
    g_last_cmd_mtime = st.st_mtime;

    FILE *fp = fopen(CMD_JSON_PATH, "r");
    if (!fp) return;

    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    if (n == 0) return;
    buf[n] = '\0';

    /* only handle "vision_mode" commands */
    if (!strstr(buf, "\"vision_mode\"")) return;

    const char *p = strstr(buf, "\"mode\"");
    if (!p) return;
    p = strchr(p, ':');
    if (!p) return;
    p++;
    while (*p == ' ' || *p == '"') p++;

    char mode_str[32] = {0};
    int i = 0;
    while (*p && *p != '"' && *p != '\n' && *p != '}' && i < 31)
        mode_str[i++] = *p++;
    mode_str[i] = '\0';

    if (mode_str[0] == '\0') return;

    vision_mode_t new_mode = MODE_MONITOR;
    if (!strcmp(mode_str, "tamper"))
        new_mode = MODE_TAMPER;
    else if (!strcmp(mode_str, "monitor"))
        new_mode = MODE_MONITOR;
    else
        return;  /* unknown mode, ignore */

    if (new_mode != g_mode) {
        printf("[visiond] mode switch: %d → %d (%s)\n",
               g_mode, new_mode, mode_str);
        g_mode = new_mode;
        g_tamper_streak = 0;   /* reset occlusion streak on mode change */
        g_face_latch_left = 0; /* reset face latch */
        g_last_face_count = 0;
    }

    /* remove cmd file after processing */
    unlink(CMD_JSON_PATH);
}

/* Delete oldest snapshot(s) to stay under MAX_SNAPSHOTS.
   Uses actual file count on disk, not the in-memory counter. */
static void cleanup_old_snapshots(const char *dir)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ls -t %s/*.jpg 2>/dev/null | tail -n +%d | xargs rm -f 2>/dev/null",
             dir, MAX_SNAPSHOTS + 1);
    int sys_ret = system(cmd);
    (void)sys_ret;  /* best-effort cleanup, ignore failures */
}

/* ---- motion detection ---- */
static int detect_motion(size_t cur_size, size_t prev_size)
{
    if (prev_size == 0 || cur_size == 0) return 0;
    /* percent difference */
    long diff = (long)cur_size - (long)prev_size;
    if (diff < 0) diff = -diff;
    long pct = diff * 100 / (long)prev_size;
    return pct >= MOTION_SIZE_THRESHOLD;
}

/* ---- JSON writer ---- */
static void write_vision_json(int camera_online, int motion_detected,
                               int face_count, const char *snapshot_path,
                               long inference_ms, int tamper_detected)
{
    char ts[64];
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    FILE *fp = fopen(VISION_JSON_TMP, "w");
    if (!fp) return;

    const char *sp = (snapshot_path && snapshot_path[0]) ? snapshot_path : NULL;

    const char *mode_str = (g_mode == MODE_TAMPER) ? "tamper" : "monitor";

    fprintf(fp, "{\n");
    fprintf(fp, "  \"mode\": \"%s\",\n", mode_str);
    fprintf(fp, "  \"camera_online\": %s,\n",
            camera_online ? "true" : "false");
    fprintf(fp, "  \"motion_detected\": %s,\n",
            motion_detected ? "true" : "false");
    fprintf(fp, "  \"face_count\": %d,\n", face_count);
    fprintf(fp, "  \"total_face_count\": %d,\n", g_total_face_count);
    fprintf(fp, "  \"last_face_snapshot\": \"%s\",\n",
            g_last_face_snap[0] ? g_last_face_snap : "");
    if (sp)
        fprintf(fp, "  \"snapshot_path\": \"%s\",\n", sp);
    else
        fprintf(fp, "  \"snapshot_path\": null,\n");
    fprintf(fp, "  \"tamper_detected\": %s,\n",
            tamper_detected ? "true" : "false");
    fprintf(fp, "  \"inference_ms\": %ld,\n", inference_ms);
    fprintf(fp, "  \"timestamp\": \"%s\"\n", ts);
    fprintf(fp, "}\n");

    fclose(fp);
    rename(VISION_JSON_TMP, VISION_JSON_PATH);

    /* persist cumulative face count */
    save_face_count();
}

/* ---- usage ---- */
static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --device <path>   video device (default " DEFAULT_DEVICE ")\n");
    printf("  --interval <ms>   capture interval (default %d)\n",
           DEFAULT_INTERVAL_MS);
    printf("  --width  <px>     capture width  (default %d)\n", DEFAULT_WIDTH);
    printf("  --height <px>     capture height (default %d)\n", DEFAULT_HEIGHT);
    printf("  -h, --help        this help\n");
}

/* ---- main ---- */
int main(int argc, char *argv[])
{
    int width  = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;

    snprintf(g_device, sizeof(g_device), "%s", DEFAULT_DEVICE);

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argv[0]);
            return 0;
        } else if (!strcmp(argv[i], "--device") && i + 1 < argc) {
            snprintf(g_device, sizeof(g_device), "%s", argv[++i]);
        } else if (!strcmp(argv[i], "--interval") && i + 1 < argc) {
            g_interval_ms = atoi(argv[++i]);
            if (g_interval_ms <= 0) g_interval_ms = DEFAULT_INTERVAL_MS;
        } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
            width = atoi(argv[++i]);
            if (width <= 0) width = DEFAULT_WIDTH;
        } else if (!strcmp(argv[i], "--height") && i + 1 < argc) {
            height = atoi(argv[++i]);
            if (height <= 0) height = DEFAULT_HEIGHT;
        } else {
            fprintf(stderr, "unknown: %s\n", argv[i]);
            return 1;
        }
    }

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    /* ensure snapshot directory exists */
    ensure_dir(SNAPSHOT_DIR);

    /* restore cumulative face count from previous runs */
    load_face_count();

    /* initialize face detection (stub if ncnn not available) */
    face_detect_init("/etc/edgeguard/models");

    printf("[visiond] starting  device=%s  %ux%u  interval=%d ms\n",
           g_device, width, height, g_interval_ms);

    /* main loop — camera may come and go (USB hotplug) */
    while (g_running) {
        check_cmd_file();
        struct camera_ctx *cam = camera_open(g_device, width, height);
        if (!cam) {
            fprintf(stderr, "[visiond] camera open failed: %s\n",
                    camera_error(NULL));
            /* Write offline status so consumers know camera is down. */
            write_vision_json(0, 0, 0, "null", 0, 0);
            /* retry after interval */
            msleep_user(g_interval_ms);
            continue;
        }

        printf("[visiond] camera opened  %ux%u  pixelformat=0x%08x (%c%c%c%c)\n",
               width, height,
               camera_pixelformat(cam),
               (char)(camera_pixelformat(cam) & 0xFF),
               (char)((camera_pixelformat(cam) >> 8) & 0xFF),
               (char)((camera_pixelformat(cam) >> 16) & 0xFF),
               (char)((camera_pixelformat(cam) >> 24) & 0xFF));

        int camera_online = 1;
        int motion_detected = 0;
        int tamper_detected = 0;
        char snapshot_path[512];
        struct camera_frame frame;
        size_t prev_size = 0;

        while (g_running && camera_online) {
            /* check for mode-switch commands */
            check_cmd_file();

            struct timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);

            frame.data = NULL;  /* safety: ensure clean state before capture */
            int ret = camera_capture(cam, &frame);
            if (ret < 0) {
                fprintf(stderr, "[visiond] capture failed: %s\n",
                        camera_error(cam));
                free(frame.data);  /* defensive: free partial allocation if any */
                camera_online = 0;
                break;
            }

            /* validate JPEG magic */
            if (frame.size < 4 || frame.data[0] != 0xFF || frame.data[1] != 0xD8) {
                fprintf(stderr, "[visiond] NOT JPEG! magic=%02X%02X  "
                        "size=%zu  pixelformat=0x%08X\n",
                        frame.size >= 2 ? frame.data[0] : 0,
                        frame.size >= 2 ? frame.data[1] : 0,
                        frame.size, camera_pixelformat(cam));
            }

            clock_gettime(CLOCK_MONOTONIC, &t1);
            long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000L +
                              (t1.tv_nsec - t0.tv_nsec) / 1000000L;

            /* ---- unified detection: motion + face + tamper ---- */
            size_t old_size = prev_size;
            motion_detected = detect_motion(frame.size, prev_size);
            prev_size = frame.size;

            /* ---- MODE TAMPER: occlusion (tamper) detection ---- */
            if (g_mode == MODE_TAMPER) {
                if (frame.size < TAMPER_SIZE_THRESHOLD) {
                    g_tamper_streak++;
                    if (g_tamper_streak >= TAMPER_CONSEC_COUNT) {
                        tamper_detected = 1;
                        printf("[visiond] TAMPER detected  "
                               "streak=%d  size=%zu\n",
                               g_tamper_streak, frame.size);
                    }
                } else {
                    if (g_tamper_streak >= TAMPER_CONSEC_COUNT)
                        printf("[visiond] tamper cleared\n");
                    g_tamper_streak = 0;
                    tamper_detected = 0;
                }
            } else {
                tamper_detected = 0;
                g_tamper_streak = 0;
            }

            /* face detection — only run when motion triggers, saves CPU */
            int face_count = 0;
            face_detect_run(frame.data, (int)frame.size, &face_count);
            if (face_count > 0) {
                printf("[visiond] FACE detected  count=%d\n", face_count);
                g_total_face_count += face_count;
                g_last_face_count = face_count;
                g_face_latch_left = 5;  /* hold for ~10s (5 cycles × 2s) */
            }
            /* latch: keep showing last face count for N cycles after faces gone */
            if (g_face_latch_left > 0) {
                if (face_count == 0)
                    face_count = g_last_face_count;
                g_face_latch_left--;
            } else {
                g_last_face_count = 0;
            }

            /* total inference time (capture + motion + face detect) */
            struct timespec t2;
            clock_gettime(CLOCK_MONOTONIC, &t2);
            long total_ms = (t2.tv_sec - t0.tv_sec) * 1000L +
                            (t2.tv_nsec - t0.tv_nsec) / 1000000L;

            /* save snapshot — only when something meaningful happened */
            snapshot_path[0] = '\0';
            if (motion_detected || face_count > 0 || tamper_detected) {
                time_t now = time(NULL);
                struct tm tm_buf;
                localtime_r(&now, &tm_buf);
                char ts_file[32];
                strftime(ts_file, sizeof(ts_file), "%Y%m%d_%H%M%S", &tm_buf);
                snprintf(snapshot_path, sizeof(snapshot_path),
                         "%s/%s.jpg", SNAPSHOT_DIR, ts_file);

                FILE *fp = fopen(snapshot_path, "wb");
                if (fp) {
                    fwrite(frame.data, 1, frame.size, fp);
                    fclose(fp);
                    cleanup_old_snapshots(SNAPSHOT_DIR);
                } else {
                    snprintf(snapshot_path, sizeof(snapshot_path), "null");
                }
            } else {
                snprintf(snapshot_path, sizeof(snapshot_path), "null");
            }

            /* remember snapshot path when face was detected */
            if (face_count > 0 && snapshot_path[0] != 'n') {
                snprintf(g_last_face_snap, sizeof(g_last_face_snap),
                         "%s", snapshot_path);
            }

            /* write JSON for consumers */
            write_vision_json(camera_online, motion_detected,
                              face_count, snapshot_path, total_ms,
                              tamper_detected);

            if (motion_detected)
                printf("[visiond] MOTION detected  size_change=%zu→%zu  "
                       "snapshot=%s\n",
                       old_size, frame.size,
                       snapshot_path);

            free(frame.data);

            /* sleep until next capture */
            long remain = g_interval_ms - elapsed_ms;
            if (remain > 0)
                msleep_user((unsigned int)remain);
        }

        camera_close(cam);
        printf("[visiond] camera disconnected, will retry...\n");

        /* write offline status */
        if (g_running)
            write_vision_json(0, 0, 0, "null", 0, 0);
    }

    save_face_count();
    face_detect_deinit();
    printf("[visiond] stopped\n");
    return 0;
}
