// sensor_hubd.c — EdgeGuard Sensor Hub Daemon
// State machine: NORMAL → WARNING → ALARM → FAULT
// Writes /tmp/edgeguard_status.json, reads /etc/edgeguard/config.json
// Command channel: /tmp/edgeguard_cmd.json
// Log: /var/log/edgeguard/alarm.log

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
#include "sqlite3.h"

/* ---- device nodes ---- */
#define MPU6050_DEV       "/dev/mpu6050_raw"
#define AP3216C_DEV       "/dev/ap3216c_raw"
#define EDGE_LEDS_DEV     "/dev/edge_leds"
#define EDGE_BUZZER_DEV   "/dev/edge_buzzer"

/* ---- JSON / cmd / log paths ---- */
#define STATUS_JSON_PATH  "/tmp/edgeguard_status.json"
#define STATUS_JSON_TMP   "/tmp/edgeguard_status.json.tmp"
#define CMD_JSON_PATH     "/tmp/edgeguard_cmd.json"
#define CONFIG_PATH       "/etc/edgeguard/config.json"
#define CONFIG_DIR        "/etc/edgeguard"
#define LOG_DIR           "/var/log/edgeguard"
#define LOG_PATH          "/var/log/edgeguard/alarm.log"
#define ALARM_DB_PATH     "/var/log/edgeguard/alarms.db"

#define BUF_SIZE          512
#define CONFIG_BUF_SIZE   4096

/* ---- defaults (used when config file is missing) ---- */
#define DEFAULT_INTERVAL_MS         500
#define DEFAULT_ALS_LOW_TH          80
#define DEFAULT_PS_WARNING_TH       120
#define DEFAULT_PS_ALARM_TH         220
#define DEFAULT_MOTION_WARNING_TH   8000
#define DEFAULT_MOTION_ALARM_TH     15000
#define DEFAULT_BUZZER_ENABLE       1
#define DEFAULT_LED_ENABLE          1
#define DEFAULT_LOG_ENABLE          1

#define FAULT_THRESHOLD   3   /* consecutive failures before FAULT */
#define BUZZER_BEEP_MS    200

/* ---- data structures ---- */
struct mpu6050_data {
    int ax, ay, az;
    int temp;
    int gx, gy, gz;
    int valid;
};

struct ap3216c_data {
    int ir, als, ps;
    int valid;
};

enum alarm_state {
    STATE_NORMAL = 0,
    STATE_WARNING,
    STATE_ALARM,
    STATE_FAULT,
};

struct app_config {
    int interval_ms;
    int als_low_th;
    int ps_warning_th;
    int ps_alarm_th;
    int motion_warning_th;
    int motion_alarm_th;
    int buzzer_enable;
    int led_enable;
    int log_enable;
    char config_path[256];
    char key_dev[128];
};

/* ---- global state ---- */
static volatile int g_running = 1;

static int         g_alarm_count      = 0;
static time_t      g_last_alarm_time  = 0;
static char        g_cur_led[32]      = "green";
static char        g_cur_buzzer[32]   = "off";
static int         g_muted            = 0;
static int         g_acknowledged     = 0;
static int         g_led_on           = 1;
static int         g_fault_count      = 0;
static long long   g_startup_ms       = 0;

static int         g_last_led_toggle_ms   = 0;
static int         g_last_beep_ms         = 0;

static sqlite3    *g_db = NULL;

/* ---- helpers ---- */
static const char *state_to_string(enum alarm_state s)
{
    switch (s) {
    case STATE_NORMAL:   return "NORMAL";
    case STATE_WARNING:  return "WARNING";
    case STATE_ALARM:    return "ALARM";
    case STATE_FAULT:    return "FAULT";
    default:             return "UNKNOWN";
    }
}

static const char *get_alarm_reason(enum alarm_state s, int motion_delta,
                                     int ps, int als, int mpu_ok, int ap_ok)
{
    switch (s) {
    case STATE_NORMAL:   return "none";
    case STATE_FAULT:    return mpu_ok ? "ap3216c sensor fault"
                                      : "mpu6050 sensor fault";
    case STATE_ALARM:
        if (motion_delta > 0) return "motion threshold exceeded";
        if (ps > 0)           return "proximity alarm";
        return "alarm";
    case STATE_WARNING:
        if (motion_delta > 0) return "motion warning";
        if (als > 0)          return "low ambient light";
        if (ps > 0)           return "proximity warning";
        return "warning";
    default: return "unknown";
    }
}

static long long uptime_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static double read_uptime_sec(void)
{
    double sec = 0.0;
    FILE *fp = fopen("/proc/uptime", "r");
    if (fp) {
        fscanf(fp, "%lf", &sec);
        fclose(fp);
    }
    return sec;
}

static void msleep_user(unsigned int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) ;
}

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ---- file helpers ---- */
static int read_text_device(const char *path, char *buf, size_t size)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[ERR] open %s: %s\n", path, strerror(errno));
        return -1;
    }
    memset(buf, 0, size);
    ssize_t n = read(fd, buf, size - 1);
    close(fd);
    if (n < 0) {
        fprintf(stderr, "[ERR] read %s: %s\n", path, strerror(errno));
        return -1;
    }
    buf[n] = '\0';
    return 0;
}

static int write_text_device(const char *path, const char *cmd)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[ERR] open %s: %s\n", path, strerror(errno));
        return -1;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "%s\n", cmd);
    ssize_t n = write(fd, buf, strlen(buf));
    close(fd);
    return (n >= 0) ? 0 : -1;
}

/* ---- sensor parsers ---- */
static int parse_mpu6050(const char *raw, struct mpu6050_data *d)
{
    memset(d, 0, sizeof(*d));
    int n = sscanf(raw,
                   "accel_raw: %d %d %d\n"
                   "temp_raw: %d\n"
                   "gyro_raw: %d %d %d",
                   &d->ax, &d->ay, &d->az,
                   &d->temp,
                   &d->gx, &d->gy, &d->gz);
    if (n != 7) return -1;
    d->valid = 1;
    return 0;
}

static int parse_ap3216c(const char *raw, struct ap3216c_data *d)
{
    memset(d, 0, sizeof(*d));
    int n = sscanf(raw,
                   "ir_raw: %d\n"
                   "als_raw: %d\n"
                   "ps_raw: %d",
                   &d->ir, &d->als, &d->ps);
    if (n != 3) return -1;
    d->valid = 1;
    return 0;
}

static int read_mpu6050(struct mpu6050_data *d)
{
    char buf[BUF_SIZE];
    if (read_text_device(MPU6050_DEV, buf, sizeof(buf)) < 0)
        return -1;
    return parse_mpu6050(buf, d);
}

static int read_ap3216c(struct ap3216c_data *d)
{
    char buf[BUF_SIZE];
    if (read_text_device(AP3216C_DEV, buf, sizeof(buf)) < 0)
        return -1;
    return parse_ap3216c(buf, d);
}

static int calc_motion_delta(const struct mpu6050_data *cur,
                              const struct mpu6050_data *prev)
{
    if (!cur->valid || !prev->valid) return 0;
    int d = 0;
    d += abs(cur->ax - prev->ax);
    d += abs(cur->ay - prev->ay);
    d += abs(cur->az - prev->az);
    d += abs(cur->gx - prev->gx);
    d += abs(cur->gy - prev->gy);
    d += abs(cur->gz - prev->gz);
    return d;
}

/* ---- config file ---- */
static void write_default_config(const char *path)
{
    mkdir(CONFIG_DIR, 0755);
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    fprintf(fp,
        "{\n"
        "  \"sample_interval_ms\": %d,\n"
        "  \"als_low_threshold\": %d,\n"
        "  \"ps_warning_threshold\": %d,\n"
        "  \"ps_alarm_threshold\": %d,\n"
        "  \"motion_warning_threshold\": %d,\n"
        "  \"motion_alarm_threshold\": %d,\n"
        "  \"buzzer_enable\": true,\n"
        "  \"led_enable\": true,\n"
        "  \"log_enable\": true\n"
        "}\n",
        DEFAULT_INTERVAL_MS,
        DEFAULT_ALS_LOW_TH,
        DEFAULT_PS_WARNING_TH,
        DEFAULT_PS_ALARM_TH,
        DEFAULT_MOTION_WARNING_TH,
        DEFAULT_MOTION_ALARM_TH);
    fclose(fp);
    printf("[INFO] wrote default config to %s\n", path);
}

/* minimal JSON int extractor: finds "key": <int> in a string */
static int json_get_int(const char *json, const char *key, int def)
{
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p = strchr(p + strlen(pat), ':');
    if (!p) return def;
    return atoi(p + 1);
}

/* minimal JSON bool extractor */
static int json_get_bool(const char *json, const char *key, int def)
{
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p = strchr(p + strlen(pat), ':');
    if (!p) return def;
    return strstr(p, "true") ? 1 : 0;
}

static int load_config(struct app_config *cfg)
{
    /* set defaults first */
    cfg->interval_ms          = DEFAULT_INTERVAL_MS;
    cfg->als_low_th           = DEFAULT_ALS_LOW_TH;
    cfg->ps_warning_th        = DEFAULT_PS_WARNING_TH;
    cfg->ps_alarm_th          = DEFAULT_PS_ALARM_TH;
    cfg->motion_warning_th    = DEFAULT_MOTION_WARNING_TH;
    cfg->motion_alarm_th      = DEFAULT_MOTION_ALARM_TH;
    cfg->buzzer_enable        = DEFAULT_BUZZER_ENABLE;
    cfg->led_enable           = DEFAULT_LED_ENABLE;
    cfg->log_enable           = DEFAULT_LOG_ENABLE;

    const char *path = cfg->config_path;
    FILE *fp = fopen(path, "r");
    if (!fp) {
        write_default_config(path);
        fp = fopen(path, "r");
        if (!fp) return -1;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz <= 0 || sz > CONFIG_BUF_SIZE) { fclose(fp); return -1; }
    rewind(fp);

    char *buf = malloc(sz + 1);
    if (!buf) { fclose(fp); return -1; }
    size_t n = fread(buf, 1, sz, fp);
    fclose(fp);
    buf[n] = '\0';

    cfg->interval_ms          = json_get_int(buf, "sample_interval_ms",      cfg->interval_ms);
    cfg->als_low_th           = json_get_int(buf, "als_low_threshold",        cfg->als_low_th);
    cfg->ps_warning_th        = json_get_int(buf, "ps_warning_threshold",     cfg->ps_warning_th);
    cfg->ps_alarm_th          = json_get_int(buf, "ps_alarm_threshold",       cfg->ps_alarm_th);
    cfg->motion_warning_th    = json_get_int(buf, "motion_warning_threshold",  cfg->motion_warning_th);
    cfg->motion_alarm_th      = json_get_int(buf, "motion_alarm_threshold",    cfg->motion_alarm_th);
    cfg->buzzer_enable        = json_get_bool(buf, "buzzer_enable",           cfg->buzzer_enable);
    cfg->led_enable           = json_get_bool(buf, "led_enable",              cfg->led_enable);
    cfg->log_enable           = json_get_bool(buf, "log_enable",              cfg->log_enable);

    free(buf);
    printf("[INFO] config loaded from %s\n", path);
    return 0;
}

/* ---- logging ---- */
static void log_event(const struct app_config *cfg, const char *level, const char *msg)
{
    if (!cfg->log_enable) return;

    mkdir(LOG_DIR, 0755);
    FILE *fp = fopen(LOG_PATH, "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    fprintf(fp, "%s %s %s\n", ts, level, msg);
    fclose(fp);
}

/* ---- SQLite alarm database ---- */
static int db_init(void)
{
    int rc = sqlite3_open(ALARM_DB_PATH, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERR] sqlite3_open: %s\n", sqlite3_errmsg(g_db));
        g_db = NULL;
        return -1;
    }
    const char *sql =
        "CREATE TABLE IF NOT EXISTS alarm_events ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT NOT NULL,"
        "  state TEXT NOT NULL,"
        "  reason TEXT,"
        "  motion_delta INTEGER,"
        "  ps INTEGER, als INTEGER,"
        "  mpu_temp REAL,"
        "  acknowledged INTEGER DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_ts ON alarm_events(timestamp);";
    char *err = NULL;
    rc = sqlite3_exec(g_db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERR] db create table: %s\n", err);
        sqlite3_free(err);
    }
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    printf("[INFO] alarm database opened: %s\n", ALARM_DB_PATH);
    return 0;
}

static void db_log_alarm(const char *state_str, const char *reason,
                          int motion_delta, int ps, int als,
                          double mpu_temp, int acknowledged)
{
    if (!g_db) return;
    char ts[32];
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    const char *sql =
        "INSERT INTO alarm_events "
        "(timestamp, state, reason, motion_delta, ps, als, mpu_temp, acknowledged) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(stmt,  1, ts, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt,  2, state_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt,  3, reason, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt,   4, motion_delta);
    sqlite3_bind_int(stmt,   5, ps);
    sqlite3_bind_int(stmt,   6, als);
    sqlite3_bind_double(stmt,7, mpu_temp);
    sqlite3_bind_int(stmt,   8, acknowledged);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void db_close(void)
{
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
        printf("[INFO] alarm database closed\n");
    }
}

/* ---- JSON status writer ---- */
static void get_network_info(char *ip_out, size_t ip_size)
{
    FILE *fp = popen(
        "ip -4 addr show eth0 2>/dev/null | "
        "awk '/inet /{print $2}' | cut -d/ -f1", "r");
    if (fp) {
        if (fgets(ip_out, (int)ip_size, fp)) {
            size_t len = strlen(ip_out);
            if (len > 0 && ip_out[len - 1] == '\n')
                ip_out[len - 1] = '\0';
        }
        pclose(fp);
    }
    if (ip_out[0] == '\0')
        snprintf(ip_out, ip_size, "N/A");
}

static void write_status_json(const struct app_config *cfg,
                              const struct mpu6050_data *mpu,
                              const struct ap3216c_data *ap,
                              enum alarm_state state,
                              int motion_delta)
{
    FILE *fp = fopen(STATUS_JSON_TMP, "w");
    if (!fp) return;

    char ip[64] = "N/A";
    get_network_info(ip, sizeof(ip));

    char last_alarm_str[64] = "none";
    if (g_last_alarm_time > 0) {
        struct tm tm_buf;
        strftime(last_alarm_str, sizeof(last_alarm_str),
                 "%Y-%m-%d %H:%M:%S", localtime_r(&g_last_alarm_time, &tm_buf));
    }

    long long ts_ms = uptime_ms() - g_startup_ms;
    double uptime_s = read_uptime_sec();

    /* temperature: raw → Celsius (MPU6050 datasheet) */
    double temp_c = mpu->valid ? (mpu->temp / 340.0 + 36.53) : 0.0;

    fprintf(fp,
        "{\n"
        "  \"state\": \"%s\",\n"
        "  \"alarm_reason\": \"%s\",\n"
        "  \"timestamp_ms\": %lld,\n"
        "  \"mpu6050\": {\n"
        "    \"ax\": %d,\n"
        "    \"ay\": %d,\n"
        "    \"az\": %d,\n"
        "    \"gx\": %d,\n"
        "    \"gy\": %d,\n"
        "    \"gz\": %d,\n"
        "    \"temp\": %.1f,\n"
        "    \"motion_delta\": %d,\n"
        "    \"online\": %s\n"
        "  },\n"
        "  \"ap3216c\": {\n"
        "    \"ir\": %d,\n"
        "    \"als\": %d,\n"
        "    \"ps\": %d,\n"
        "    \"online\": %s\n"
        "  },\n"
        "  \"device\": {\n"
        "    \"led\": \"%s\",\n"
        "    \"buzzer\": \"%s\",\n"
        "    \"key\": \"released\"\n"
        "  },\n"
        "  \"alarm\": {\n"
        "    \"count\": %d,\n"
        "    \"last\": \"%s\",\n"
        "    \"muted\": %s,\n"
        "    \"acknowledged\": %s\n"
        "  },\n"
        "  \"system\": {\n"
        "    \"uptime_sec\": %.0f,\n"
        "    \"ip\": \"%s\",\n"
        "    \"sensor_hubd\": \"running\"\n"
        "  }\n"
        "}\n",
        state_to_string(state),
        get_alarm_reason(state, motion_delta, ap->ps, ap->als,
                         mpu->valid, ap->valid),
        ts_ms,
        mpu->valid ? mpu->ax : 0,
        mpu->valid ? mpu->ay : 0,
        mpu->valid ? mpu->az : 0,
        mpu->valid ? mpu->gx : 0,
        mpu->valid ? mpu->gy : 0,
        mpu->valid ? mpu->gz : 0,
        temp_c,
        motion_delta,
        mpu->valid ? "true" : "false",
        ap->valid ? ap->ir : 0,
        ap->valid ? ap->als : 0,
        ap->valid ? ap->ps : 0,
        ap->valid ? "true" : "false",
        g_cur_led,
        g_cur_buzzer,
        g_alarm_count,
        last_alarm_str,
        g_muted ? "true" : "false",
        g_acknowledged ? "true" : "false",
        uptime_s,
        ip);

    fclose(fp);
    rename(STATUS_JSON_TMP, STATUS_JSON_PATH);
}

/* ---- command channel ---- */
static void check_cmd_file(struct app_config *cfg, enum alarm_state *state)
{
    struct stat st;
    if (stat(CMD_JSON_PATH, &st) != 0) return;

    FILE *fp = fopen(CMD_JSON_PATH, "r");
    if (!fp) return;

    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    /* parse "cmd" field */
    char cmd[64] = {0};
    const char *p = strstr(buf, "\"cmd\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p++; while (*p == ' ' || *p == '"') p++;
            int i = 0;
            while (*p && *p != '"' && *p != '\n' && i < 63)
                cmd[i++] = *p++;
            cmd[i] = '\0';
        }
    }

    if (cmd[0] == '\0') { unlink(CMD_JSON_PATH); return; }

    printf("[INFO] cmd received: %s\n", cmd);

    if (!strcmp(cmd, "mute_buzzer")) {
        g_muted = 1;
        write_text_device(EDGE_BUZZER_DEV, "off");
        snprintf(g_cur_buzzer, sizeof(g_cur_buzzer), "off");
        log_event(cfg, "INFO", "buzzer muted");
    } else if (!strcmp(cmd, "ack_alarm")) {
        g_acknowledged = 1;
        g_muted = 1;
        *state = STATE_NORMAL;
        write_text_device(EDGE_LEDS_DEV, "green");
        write_text_device(EDGE_BUZZER_DEV, "off");
        snprintf(g_cur_led, sizeof(g_cur_led), "green");
        snprintf(g_cur_buzzer, sizeof(g_cur_buzzer), "off");
        log_event(cfg, "INFO", "alarm acknowledged");
    } else if (!strcmp(cmd, "demo_alarm")) {
        g_alarm_count++;
        g_last_alarm_time = time(NULL);
        g_acknowledged = 0;
        g_muted = 0;
        *state = STATE_ALARM;
        log_event(cfg, "INFO", "demo alarm triggered");
    }

    unlink(CMD_JSON_PATH);
}

/* ---- state machine ---- */
static enum alarm_state evaluate_state(const struct app_config *cfg,
                                        const struct mpu6050_data *mpu,
                                        const struct mpu6050_data *prev_mpu,
                                        const struct ap3216c_data *ap,
                                        int *motion_delta_out,
                                        int *mpu_ok, int *ap_ok)
{
    int motion_delta = calc_motion_delta(mpu, prev_mpu);
    if (motion_delta_out) *motion_delta_out = motion_delta;

    *mpu_ok = mpu->valid;
    *ap_ok  = ap->valid;

    /* FAULT: >3 consecutive failures on either sensor */
    if (!mpu->valid || !ap->valid) {
        g_fault_count++;
        if (g_fault_count >= FAULT_THRESHOLD)
            return STATE_FAULT;
    } else {
        g_fault_count = 0;
    }

    /* ALARM level */
    if (mpu->valid && prev_mpu->valid &&
        motion_delta > cfg->motion_alarm_th)
        return STATE_ALARM;

    if (ap->valid && ap->ps >= 0 && ap->ps > cfg->ps_alarm_th)
        return STATE_ALARM;

    /* WARNING level */
    if (mpu->valid && prev_mpu->valid &&
        motion_delta > cfg->motion_warning_th)
        return STATE_WARNING;

    if (ap->valid && ap->als >= 0 && ap->als < cfg->als_low_th)
        return STATE_WARNING;

    if (ap->valid && ap->ps >= 0 && ap->ps > cfg->ps_warning_th)
        return STATE_WARNING;

    return STATE_NORMAL;
}

/* ---- LED / Buzzer blink logic ---- */
static void apply_blink(enum alarm_state state, const struct app_config *cfg,
                         long long now_ms)
{
    const char *led_color = "green";
    int blink_ms = 0;

    switch (state) {
    case STATE_NORMAL:
        led_color = "green";
        blink_ms = 0;  /* solid */
        break;
    case STATE_WARNING:
        led_color = "yellow";
        blink_ms = 500;
        break;
    case STATE_ALARM:
        led_color = "red";
        blink_ms = 250;
        break;
    case STATE_FAULT:
        led_color = "red";
        blink_ms = 1000;
        break;
    }

    if (!cfg->led_enable) {
        led_color = "off";
        blink_ms = 0;
    }

    /* LED blinking */
    if (blink_ms > 0) {
        int elapsed = (int)(now_ms - g_last_led_toggle_ms);
        if (elapsed >= blink_ms || g_last_led_toggle_ms == 0) {
            g_led_on = !g_led_on;
            write_text_device(EDGE_LEDS_DEV, g_led_on ? led_color : "off");
            snprintf(g_cur_led, sizeof(g_cur_led), g_led_on ? led_color : "off");
            g_last_led_toggle_ms = (int)now_ms;
        }
    } else {
        /* solid */
        if (g_led_on != 1 || strcmp(g_cur_led, led_color)) {
            write_text_device(EDGE_LEDS_DEV, led_color);
            snprintf(g_cur_led, sizeof(g_cur_led), led_color);
            g_led_on = 1;
        }
    }

    /* Buzzer */
    if (!cfg->buzzer_enable || g_muted || g_acknowledged) {
        if (strcmp(g_cur_buzzer, "off")) {
            write_text_device(EDGE_BUZZER_DEV, "off");
            snprintf(g_cur_buzzer, sizeof(g_cur_buzzer), "off");
        }
        return;
    }

    if (state == STATE_ALARM) {
        int elapsed = (int)(now_ms - g_last_beep_ms);
        if (elapsed >= 1000 || g_last_beep_ms == 0) {
            write_text_device(EDGE_BUZZER_DEV, "beep");
            snprintf(g_cur_buzzer, sizeof(g_cur_buzzer), "beep");
            g_last_beep_ms = (int)now_ms;
        }
    } else {
        if (strcmp(g_cur_buzzer, "off")) {
            write_text_device(EDGE_BUZZER_DEV, "off");
            snprintf(g_cur_buzzer, sizeof(g_cur_buzzer), "off");
        }
    }
}

/* ---- key input ---- */
static int open_key_device(const char *path)
{
    if (path[0] == '\0') return -1;
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        fprintf(stderr, "[WARN] open key %s: %s\n", path, strerror(errno));
    else
        printf("[INFO] key device opened: %s\n", path);
    return fd;
}

static int process_key_events(int key_fd)
{
    if (key_fd < 0) return 0;
    struct input_event ev;
    int pressed = 0;
    while (1) {
        ssize_t n = read(key_fd, &ev, sizeof(ev));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            break;
        }
        if (n != sizeof(ev)) break;
        if (ev.type == EV_KEY && ev.value == 1) pressed = 1;
    }
    return pressed;
}

/* ---- print helpers ---- */
static void print_status(const struct mpu6050_data *mpu,
                         const struct ap3216c_data *ap,
                         enum alarm_state state,
                         int motion_delta)
{
    printf("[EdgeGuard] state=%s motion_delta=%d\n",
           state_to_string(state), motion_delta);
    if (mpu->valid)
        printf("  MPU6050: accel=(%d,%d,%d) temp=%d gyro=(%d,%d,%d)\n",
               mpu->ax, mpu->ay, mpu->az, mpu->temp,
               mpu->gx, mpu->gy, mpu->gz);
    else
        printf("  MPU6050: OFFLINE\n");
    if (ap->valid)
        printf("  AP3216C: ir=%d als=%d ps=%d\n", ap->ir, ap->als, ap->ps);
    else
        printf("  AP3216C: OFFLINE\n");
    fflush(stdout);
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --key <dev>       input event device (alarm clear)\n");
    printf("  --interval <ms>   override sample interval\n");
    printf("  --config <path>   config file (default " CONFIG_PATH ")\n");
    printf("  -h, --help        this help\n");
}

static int parse_args(int argc, char *argv[], struct app_config *cfg)
{
    cfg->interval_ms = DEFAULT_INTERVAL_MS;
    snprintf(cfg->config_path, sizeof(cfg->config_path), "%s", CONFIG_PATH);
    cfg->key_dev[0] = '\0';

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argv[0]);
            exit(0);
        } else if (!strcmp(argv[i], "--key") && i + 1 < argc) {
            snprintf(cfg->key_dev, sizeof(cfg->key_dev), "%s", argv[++i]);
        } else if (!strcmp(argv[i], "--interval") && i + 1 < argc) {
            cfg->interval_ms = atoi(argv[++i]);
            if (cfg->interval_ms <= 0) cfg->interval_ms = DEFAULT_INTERVAL_MS;
        } else if (!strcmp(argv[i], "--config") && i + 1 < argc) {
            snprintf(cfg->config_path, sizeof(cfg->config_path), "%s", argv[++i]);
        } else {
            fprintf(stderr, "unknown: %s\n", argv[i]);
            return -1;
        }
    }
    return 0;
}

/* ---- main ---- */
int main(int argc, char *argv[])
{
    struct app_config cfg;
    struct mpu6050_data mpu, prev_mpu;
    struct ap3216c_data ap;
    enum alarm_state state = STATE_NORMAL;
    enum alarm_state last_state = -1;
    int motion_delta = 0;
    int key_fd = -1;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    g_startup_ms = uptime_ms();

    memset(&cfg, 0, sizeof(cfg));
    if (parse_args(argc, argv, &cfg) < 0) {
        print_usage(argv[0]);
        return 1;
    }

    if (load_config(&cfg) < 0)
        printf("[WARN] using default config values\n");

    mkdir(LOG_DIR, 0755);
    db_init();

    memset(&mpu, 0, sizeof(mpu));
    memset(&prev_mpu, 0, sizeof(prev_mpu));
    memset(&ap, 0, sizeof(ap));

    printf("[INFO] sensor_hubd started  interval=%d ms\n", cfg.interval_ms);
    printf("[INFO] thresholds: als_low=%d ps_warn=%d ps_alarm=%d "
           "motion_warn=%d motion_alarm=%d\n",
           cfg.als_low_th, cfg.ps_warning_th, cfg.ps_alarm_th,
           cfg.motion_warning_th, cfg.motion_alarm_th);

    if (cfg.key_dev[0])
        key_fd = open_key_device(cfg.key_dev);

    /* initial state */
    write_text_device(EDGE_LEDS_DEV, "green");
    write_text_device(EDGE_BUZZER_DEV, "off");
    log_event(&cfg, "INFO", "sensor_hubd started");

    while (g_running) {
        long long now_ms = uptime_ms();
        int mpu_ok, ap_ok;

        prev_mpu = mpu;

        read_mpu6050(&mpu);
        read_ap3216c(&ap);

        /* key override: clear alarm */
        if (process_key_events(key_fd)) {
            printf("[INFO] key pressed: clear alarm\n");
            state = STATE_NORMAL;
            g_acknowledged = 1;
            g_muted = 1;
            write_text_device(EDGE_LEDS_DEV, "green");
            write_text_device(EDGE_BUZZER_DEV, "off");
            snprintf(g_cur_led, sizeof(g_cur_led), "green");
            snprintf(g_cur_buzzer, sizeof(g_cur_buzzer), "off");
            log_event(&cfg, "INFO", "alarm cleared by key");
            last_state = -1;
        } else {
            state = evaluate_state(&cfg, &mpu, &prev_mpu, &ap,
                                   &motion_delta, &mpu_ok, &ap_ok);
        }

        /* alarm state transition tracking */
        if (state != STATE_NORMAL && last_state == STATE_NORMAL) {
            g_alarm_count++;
            g_last_alarm_time = time(NULL);
            g_acknowledged = 0;
            g_muted = 0;
        }
        if (state != last_state && last_state >= 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "%s → %s  (reason: %s)",
                     state_to_string(last_state), state_to_string(state),
                     get_alarm_reason(state, motion_delta,
                                      ap.ps, ap.als, mpu_ok, ap_ok));
            log_event(&cfg,
                      state == STATE_NORMAL ? "INFO" : "ALARM",
                      msg);
            /* log to SQLite on state transitions */
            {
                double t = mpu.valid ? (mpu.temp / 340.0 + 36.53) : 0.0;
                const char *reason = get_alarm_reason(state, motion_delta,
                    ap.ps, ap.als, mpu_ok, ap_ok);
                db_log_alarm(state_to_string(state), reason,
                             motion_delta, ap.ps, ap.als, t,
                             (state == STATE_NORMAL) ? 1 : 0);
            }
        }

        /* LED / buzzer blink */
        apply_blink(state, &cfg, now_ms);

        /* check command channel */
        check_cmd_file(&cfg, &state);

        /* write JSON status */
        write_status_json(&cfg, &mpu, &ap, state, motion_delta);

        /* console */
        print_status(&mpu, &ap, state, motion_delta);

        last_state = state;
        msleep_user(cfg.interval_ms);
    }

    printf("[INFO] sensor_hubd stopping\n");
    write_text_device(EDGE_BUZZER_DEV, "off");
    write_text_device(EDGE_LEDS_DEV, "off");
    log_event(&cfg, "INFO", "sensor_hubd stopped");

    if (key_fd >= 0) close(key_fd);
    db_close();
    return 0;
}
