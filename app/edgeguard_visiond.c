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
#define MAX_SNAPSHOTS           50         /* keep at most this many snapshots */

/* Motion detection: JPEG-size-based heuristic.
   When the scene changes (person walks in, lighting change), the JPEG
   encoder produces a frame with different size.  This is not pixel-accurate
   but works as a lightweight "something changed" detector without requiring
   a JPEG decoder or AI model. */
#define MOTION_SIZE_THRESHOLD   15        /* percent change to trigger */

/* ---- global state ---- */
static volatile int g_running = 1;
static char   g_device[256];
static int    g_interval_ms = DEFAULT_INTERVAL_MS;
static int    g_snapshot_count = 0;

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
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

/* Delete oldest snapshot(s) to stay under MAX_SNAPSHOTS */
static void cleanup_old_snapshots(const char *dir)
{
    if (g_snapshot_count <= MAX_SNAPSHOTS) return;
    /* Simple: delete all snapshots when we hit the limit.
       A production system would delete the oldest by timestamp. */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ls -t %s/*.jpg 2>/dev/null | tail -n +%d | xargs rm -f 2>/dev/null",
             dir, MAX_SNAPSHOTS + 1);
    int sys_ret = system(cmd);
    (void)sys_ret;  /* best-effort cleanup, ignore failures */
    g_snapshot_count = MAX_SNAPSHOTS;
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
                               long inference_ms)
{
    char ts[64];
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    FILE *fp = fopen(VISION_JSON_TMP, "w");
    if (!fp) return;

    const char *sp = (snapshot_path && snapshot_path[0]) ? snapshot_path : NULL;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"camera_online\": %s,\n",
            camera_online ? "true" : "false");
    fprintf(fp, "  \"motion_detected\": %s,\n",
            motion_detected ? "true" : "false");
    fprintf(fp, "  \"face_count\": %d,\n", face_count);
    if (sp)
        fprintf(fp, "  \"snapshot_path\": \"%s\",\n", sp);
    else
        fprintf(fp, "  \"snapshot_path\": null,\n");
    fprintf(fp, "  \"inference_ms\": %ld,\n", inference_ms);
    fprintf(fp, "  \"timestamp\": \"%s\",\n", ts);
    fprintf(fp, "  \"face_verify_result\": null\n");
    fprintf(fp, "}\n");

    fclose(fp);
    rename(VISION_JSON_TMP, VISION_JSON_PATH);
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

    /* initialize face detection (stub mode if ncnn not available) */
    face_detect_init("/etc/edgeguard/models");

    printf("[visiond] starting  device=%s  %ux%u  interval=%d ms\n",
           g_device, width, height, g_interval_ms);

    /* main loop — camera may come and go (USB hotplug) */
    while (g_running) {
        struct camera_ctx *cam = camera_open(g_device, width, height);
        if (!cam) {
            fprintf(stderr, "[visiond] camera open failed: %s\n",
                    camera_error(NULL));
            /* Write offline status so consumers know camera is down. */
            write_vision_json(0, 0, 0, "null", 0);
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
        char snapshot_path[512];
        struct camera_frame frame;
        size_t prev_size = 0;

        while (g_running && camera_online) {
            struct timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);

            int ret = camera_capture(cam, &frame);
            if (ret < 0) {
                fprintf(stderr, "[visiond] capture failed: %s\n",
                        camera_error(cam));
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

            /* motion detection (JPEG-size heuristic) */
            size_t old_size = prev_size;
            motion_detected = detect_motion(frame.size, prev_size);
            prev_size = frame.size;

            /* face detection — only run when motion triggers, saves CPU.
               In stub mode this always returns face_count=0 harmlessly. */
            int face_count = 0;
            if (motion_detected) {
                face_detect_run(frame.data, (int)frame.size, &face_count);
                if (face_count > 0)
                    printf("[visiond] FACE detected  count=%d\n", face_count);
            }

            /* save snapshot */
            snapshot_path[0] = '\0';
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
                g_snapshot_count++;
                cleanup_old_snapshots(SNAPSHOT_DIR);
            } else {
                snprintf(snapshot_path, sizeof(snapshot_path), "null");
            }

            /* write JSON for consumers */
            write_vision_json(camera_online, motion_detected,
                              face_count, snapshot_path, elapsed_ms);

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
            write_vision_json(0, 0, 0, "null", 0);
    }

    face_detect_deinit();
    printf("[visiond] stopped\n");
    return 0;
}
