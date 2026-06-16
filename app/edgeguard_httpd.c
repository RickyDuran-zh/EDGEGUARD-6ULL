// edgeguard_httpd.c — EdgeGuard HTTP Remote Access Daemon
// Standalone lightweight HTTP server for remote monitoring and control.
// Reads /tmp/edgeguard_status.json, writes /tmp/edgeguard_cmd.json.
// No external dependencies — pure POSIX sockets + pthreads.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include "sqlite3.h"

/* ---- defaults ---- */
#define DEFAULT_PORT        8080
#define DEFAULT_STATUS_PATH "/tmp/edgeguard_status.json"
#define DEFAULT_CMD_PATH    "/tmp/edgeguard_cmd.json"
#define ALARM_DB_PATH       "/var/log/edgeguard/alarms.db"
#define MAX_CONNECTIONS     10
#define MAX_SSE_CLIENTS     8
#define REQ_BUF_SIZE        4096
#define RESP_BUF_SIZE       32768
#define RECV_TIMEOUT_SEC    2

/* ---- global config ---- */
static volatile int g_running = 1;
static int           g_port        = DEFAULT_PORT;
static char          g_status_path[256];
static char          g_cmd_path[256];
static char          g_cmd_tmp_path[272];  /* g_cmd_path + ".tmp" */
static pthread_mutex_t g_cmd_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ---- auth settings (can be overridden via CLI) ---- */
#define AUTH_ENABLE 1
static char g_auth_user[64] = "admin";
static char g_auth_pass[64] = "edgeguard";

/* ---- SSE broadcast state ---- */
struct sse_client {
    int fd;
    struct sse_client *next;
};
static struct sse_client *g_sse_clients = NULL;
static pthread_mutex_t     g_sse_mutex = PTHREAD_MUTEX_INITIALIZER;
static char                g_sse_last_json[8192];
static volatile int        g_sse_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ---- auto-detect eth* IPv4 address via ioctl ---- */
static void get_eth0_ip(char *out, size_t size)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        snprintf(out, size, "0.0.0.0");
        return;
    }

    struct ifreq ifr;
    const char *try_names[] = {"eth0", "eth1", "eth2", "eth3", NULL};
    int found = 0;

    for (int i = 0; try_names[i] != NULL; i++) {
        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", try_names[i]);
        if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) continue;
        struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
        if (addr->sin_addr.s_addr == 0 || addr->sin_addr.s_addr == INADDR_NONE) continue;
        const char *ip = inet_ntoa(addr->sin_addr);
        if (ip && ip[0] != '0') {
            snprintf(out, size, "%s", ip);
            found = 1;
            break;
        }
    }
    close(fd);
    if (!found) snprintf(out, size, "0.0.0.0");
}

/* ---- read entire file into a stack buffer ---- */
static int read_file(const char *path, char *buf, size_t size)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    memset(buf, 0, size);
    ssize_t n = read(fd, buf, size - 1);
    close(fd);
    if (n < 0) return -1;
    buf[n] = '\0';
    return (int)n;
}

/* ---- atomic write to command file ---- */
static int write_cmd_file(const char *json_str)
{
    pthread_mutex_lock(&g_cmd_mutex);

    int fd = open(g_cmd_tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        pthread_mutex_unlock(&g_cmd_mutex);
        fprintf(stderr, "[edgeguard_httpd] open %s: %s\n",
                g_cmd_tmp_path, strerror(errno));
        return -1;
    }

    size_t len = strlen(json_str);
    ssize_t n = write(fd, json_str, len);
    close(fd);

    if (n != (ssize_t)len) {
        unlink(g_cmd_tmp_path);
        pthread_mutex_unlock(&g_cmd_mutex);
        return -1;
    }

    if (rename(g_cmd_tmp_path, g_cmd_path) < 0) {
        unlink(g_cmd_tmp_path);
        pthread_mutex_unlock(&g_cmd_mutex);
        fprintf(stderr, "[edgeguard_httpd] rename → %s: %s\n",
                g_cmd_path, strerror(errno));
        return -1;
    }

    pthread_mutex_unlock(&g_cmd_mutex);
    printf("[edgeguard_httpd] cmd written: %.120s\n", json_str);
    return 0;
}

/* ---- build HTTP response for GET /api/status ---- */
static void build_status_response(char *resp, size_t size)
{
    char json_buf[8192];
    int ok = 0;

    if (read_file(g_status_path, json_buf, sizeof(json_buf)) >= 0
        && json_buf[0] != '\0') {
        ok = 1;
    }

    if (!ok) {
        snprintf(json_buf, sizeof(json_buf),
                 "{\n"
                 "  \"state\": \"WAITING\",\n"
                 "  \"alarm_reason\": \"status file not ready\"\n"
                 "}\n");
    }

    size_t body_len = strlen(json_buf);
    snprintf(resp, size,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "\r\n"
             "%s",
             body_len, json_buf);
}

/* ---- build HTTP response for POST /api/cmd ---- */
static void build_cmd_ok_response(char *resp, size_t size)
{
    const char *body = "{\"ok\":true}\n";
    snprintf(resp, size,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "\r\n"
             "%s",
             strlen(body), body);
}

/* ---- URL-decode in-place (simplified) ---- */
static void url_decode(char *s)
{
    char *src = s, *dst = s;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* ---- base64 decode ---- */
static int base64_decode(const char *in, char *out, size_t out_size)
{
    static const unsigned char table[256] = {
        ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
        ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
        ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
        ['Y']=24,['Z']=25,['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,
        ['g']=32,['h']=33,['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,
        ['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,
        ['w']=48,['x']=49,['y']=50,['z']=51,['0']=52,['1']=53,['2']=54,['3']=55,
        ['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['+']=62,['/']=63,
    };

    size_t in_len = strlen(in);
    size_t out_pos = 0;
    unsigned char buf[4];
    int buf_pos = 0;

    for (size_t i = 0; i < in_len; i++) {
        if (in[i] == '=') break;
        if (in[i] == ' ' || in[i] == '\r' || in[i] == '\n') continue;
        unsigned char val = table[(unsigned char)in[i]];
        if (val == 0 && in[i] != 'A') return -1;
        buf[buf_pos++] = val;
        if (buf_pos == 4) {
            if (out_pos + 3 > out_size) return -1;
            out[out_pos++] = (buf[0] << 2) | (buf[1] >> 4);
            out[out_pos++] = (buf[1] << 4) | (buf[2] >> 2);
            out[out_pos++] = (buf[2] << 6) | buf[3];
            buf_pos = 0;
        }
    }
    if (buf_pos >= 2) {
        if (out_pos + 1 > out_size) return -1;
        out[out_pos++] = (buf[0] << 2) | (buf[1] >> 4);
        if (buf_pos >= 3) {
            if (out_pos + 1 > out_size) return -1;
            out[out_pos++] = (buf[1] << 4) | (buf[2] >> 2);
        }
    }
    out[out_pos] = '\0';
    return (int)out_pos;
}

/* ---- embedded HTML dashboard (multi-page SPA) ---- */
static const char *get_dashboard_html(void)
{
    return
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"<meta charset=\"UTF-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"<title>RickyDuran-EdgeGuard</title>\n"
"<style>\n"
"*{margin:0;padding:0;box-sizing:border-box}\n"
":root{--bg:#0a0e14;--bg2:#12171f;--bg3:#1a2030;--border:#1e2a3a;"
"--accent:#4f8cff;--accent2:#ff6b6b;--green:#3fb950;--yellow:#d29922;--red:#f85149;"
"--text:#c9d1d9;--text2:#8b949e;--white:#f0f6fc;--gold:#f0c060}\n"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
"background:var(--bg);color:var(--text);min-height:100vh}\n"
"body::before{content:'';position:fixed;top:0;left:0;width:100%;height:100%;"
"background:radial-gradient(ellipse at 20% 20%,rgba(79,140,255,.04) 0%,transparent 60%),"
"radial-gradient(ellipse at 80% 80%,rgba(240,192,96,.03) 0%,transparent 60%);"
"pointer-events:none;z-index:0}\n"

/* ---- top bar ---- */
".topbar{background:linear-gradient(90deg,#0d1a2d,#0a0e14);"
"border-bottom:1px solid var(--border);padding:0 20px;height:52px;"
"display:flex;align-items:center;gap:16px;position:sticky;top:0;z-index:100}\n"
".logo{width:32px;height:32px;border-radius:8px;"
"background:linear-gradient(135deg,var(--accent),#2b5fc0);"
"display:flex;align-items:center;justify-content:center;font-size:18px;"
"box-shadow:0 0 12px rgba(79,140,255,.35);flex-shrink:0}\n"
".brand{font-size:15px;font-weight:700;color:var(--white);white-space:nowrap;"
"letter-spacing:-.2px}\n"
".brand em{font-style:normal;color:var(--accent)}\n"
".tabs{display:flex;gap:2px;flex:1}\n"
".tab{background:transparent;color:var(--text2);border:none;padding:8px 16px;"
"border-radius:6px;font-size:13px;font-weight:600;cursor:pointer;"
"transition:background .15s,color .15s;white-space:nowrap}\n"
".tab:hover{background:var(--bg3);color:var(--white)}\n"
".tab.active{background:var(--blue);color:#fff}\n"
".topbar-right{display:flex;align-items:center;gap:14px;font-size:12px;color:var(--text2);white-space:nowrap}\n"
".conn-dot{width:7px;height:7px;border-radius:50%;display:inline-block;margin-right:4px}\n"
".conn-live{background:var(--green);box-shadow:0 0 5px var(--green)}\n"
".conn-lost{background:var(--red);box-shadow:0 0 5px var(--red)}\n"

/* ---- pages ---- */
".page{display:none;padding:20px 24px}\n"
".page.active{display:block}\n"

/* ---- overview strip ---- */
".overview-strip{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:16px}\n"
".ov-card{background:var(--bg2);border:1px solid var(--border);border-radius:10px;"
"padding:16px 20px;text-align:center}\n"
".ov-card .ov-val{font-size:28px;font-weight:800;line-height:1.1}\n"
".ov-card .ov-lbl{font-size:11px;color:var(--text2);margin-top:4px;text-transform:uppercase;letter-spacing:.5px}\n"
".ov-card.state-NORMAL .ov-val{color:var(--green)}\n"
".ov-card.state-WARNING .ov-val{color:var(--yellow)}\n"
".ov-card.state-ALARM .ov-val{color:var(--red);animation:blink .5s infinite}\n"
".ov-card.state-FAULT .ov-val{color:var(--red)}\n"
"@keyframes blink{50%{opacity:.3}}\n"

/* ---- cards ---- */
".card{background:var(--bg2);border:1px solid var(--border);border-radius:10px;"
"padding:16px 20px;margin-bottom:12px}\n"
".card h2{font-size:12px;color:var(--blue);margin-bottom:12px;"
"text-transform:uppercase;letter-spacing:.5px;display:flex;align-items:center;gap:6px}\n"
".dot{width:7px;height:7px;border-radius:50%;display:inline-block}\n"
".dot-on{background:var(--green);box-shadow:0 0 4px var(--green)}\n"
".dot-off{background:var(--red);box-shadow:0 0 4px var(--red)}\n"

/* ---- sensor grid ---- */
".sensor-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px}\n"
".sensor-row{display:flex;flex-wrap:wrap;gap:6px 18px;font-size:13px;line-height:1.6}\n"
".sensor-row .lbl{color:var(--text2);min-width:55px}\n"
".sensor-row .val{color:var(--white);font-weight:500;font-variant-numeric:tabular-nums}\n"

/* ---- device card ---- */
".dev-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:10px 16px;"
"font-size:13px;margin-bottom:14px}\n"
".dev-grid .lbl{color:var(--text2)}\n"
".dev-grid .val{color:var(--white);font-weight:600}\n"

/* ---- buttons ---- */
".btn-row{display:flex;gap:10px;flex-wrap:wrap}\n"
".btn{padding:10px 20px;border:none;border-radius:6px;font-size:13px;font-weight:600;"
"cursor:pointer;color:#fff;transition:transform .1s,opacity .2s}\n"
".btn:active{transform:scale(.96)}\n"
".btn:disabled{opacity:.45;cursor:not-allowed;transform:none}\n"
".btn-mute{background:var(--yellow);color:#000}\n"
".btn-mute.active{background:#5c4a0a;color:var(--yellow)}\n"
".btn-ack{background:var(--green);color:#000}\n"
".btn-ack.active{background:#0a3d14;color:var(--green)}\n"
".btn-demo{background:#b8600f}\n"

/* ---- alarm table ---- */
".alarm-table{width:100%;border-collapse:collapse;font-size:13px}\n"
".alarm-table th{text-align:left;padding:10px 12px;color:var(--text2);font-size:11px;"
"text-transform:uppercase;letter-spacing:.5px;border-bottom:1px solid var(--border)}\n"
".alarm-table td{padding:9px 12px;border-bottom:1px solid var(--border);color:var(--text)}\n"
".alarm-table tr:hover td{background:var(--bg3)}\n"
".badge-sm{display:inline-block;padding:2px 8px;border-radius:4px;font-size:11px;font-weight:700}\n"
".badge-ALARM{background:#490202;color:var(--red)}\n"
".badge-WARNING{background:#3a2e00;color:var(--yellow)}\n"
".badge-NORMAL{background:#1a3a1a;color:var(--green)}\n"
".badge-FAULT{background:#3a1111;color:var(--red)}\n"

/* ---- settings ---- */
".cfg-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:10px}\n"
".cfg-item{background:var(--bg3);border-radius:8px;padding:14px 16px;display:flex;"
"justify-content:space-between;align-items:center}\n"
".cfg-item .cfg-lbl{font-size:13px;color:var(--text2)}\n"
".cfg-item .cfg-val{font-size:16px;font-weight:700;color:var(--white)}\n"

/* ---- camera ---- */
".cam-preview{text-align:center;margin-bottom:16px}\n"
".cam-preview img{max-width:100%;max-height:360px;border-radius:8px;border:2px solid var(--border)}\n"
".cam-placeholder{display:inline-block;padding:60px 80px;background:var(--bg3);"
"border-radius:8px;color:var(--text2);font-size:14px}\n"
".cam-info{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px}\n"

/* ---- refresh bar ---- */
".refresh-bar{display:flex;align-items:center;gap:8px;font-size:11px;color:var(--text2);"
"margin-bottom:12px}\n"
".refresh-dot{width:6px;height:6px;border-radius:50%;background:var(--green)}\n"

/* ---- responsive ---- */
"@media(max-width:900px){.overview-strip{grid-template-columns:repeat(2,1fr)}"
".sensor-grid{grid-template-columns:1fr}}\n"
"@media(max-width:520px){.overview-strip{grid-template-columns:1fr}"
".topbar{padding:0 10px}.tab{padding:8px 10px;font-size:12px}"
".page{padding:12px 10px}.brand{font-size:15px}"
".topbar-right{gap:8px;font-size:11px}}\n"
"</style>\n"
"</head>\n"
"<body>\n"

/* ---- Top Nav Bar ---- */
"<nav class=\"topbar\">\n"
"<div class=\"logo\">🛡</div>\n"
"<div class=\"brand\">RickyDuran<em>-EdgeGuard</em></div>\n"
"<div class=\"tabs\">\n"
"<button class=\"tab active\" data-page=\"dashboard\">仪表盘</button>\n"
"<button class=\"tab\" data-page=\"alarms\">报警记录</button>\n"
"<button class=\"tab\" data-page=\"camera\">摄像头</button>\n"
"<button class=\"tab\" data-page=\"settings\">系统设置</button>\n"
"</div>\n"
"<div class=\"topbar-right\">\n"
"<span id=\"conn_status\"><span class=\"conn-dot conn-live\"></span>在线</span>\n"
"<span id=\"clock\">--:--:--</span>\n"
"</div>\n"
"</nav>\n"

/* ======== Page: Dashboard ======== */
"<div id=\"page-dashboard\" class=\"page active\">\n"

/* status overview strip */
"<div class=\"overview-strip\">\n"
"<div class=\"ov-card\" id=\"ov_state\"><div class=\"ov-val\">--</div><div class=\"ov-lbl\">系统状态</div></div>\n"
"<div class=\"ov-card\" id=\"ov_alarms\"><div class=\"ov-val\">0</div><div class=\"ov-lbl\">累计报警</div></div>\n"
"<div class=\"ov-card\" id=\"ov_uptime\"><div class=\"ov-val\">--</div><div class=\"ov-lbl\">运行时长</div></div>\n"
"<div class=\"ov-card\" id=\"ov_ip\"><div class=\"ov-val\">--</div><div class=\"ov-lbl\">板卡 IP</div></div>\n"
"</div>\n"

/* sensor cards */
"<div class=\"sensor-grid\">\n"
"<div class=\"card\">\n"
"<h2><span class=\"dot\" id=\"mpu_dot\"></span> MPU6050</h2>\n"
"<div class=\"sensor-row\">"
"<span class=\"lbl\">加速度</span><span class=\"val\" id=\"mpu_accel\">--</span>"
"<span class=\"lbl\">陀螺仪</span><span class=\"val\" id=\"mpu_gyro\">--</span>"
"<span class=\"lbl\">温度</span><span class=\"val\" id=\"mpu_temp\">--</span>"
"<span class=\"lbl\">震动</span><span class=\"val\" id=\"mpu_motion\">--</span>"
"</div>\n"
"</div>\n"
"<div class=\"card\">\n"
"<h2><span class=\"dot\" id=\"ap_dot\"></span> AP3216C</h2>\n"
"<div class=\"sensor-row\">"
"<span class=\"lbl\">红外</span><span class=\"val\" id=\"ap_ir\">--</span>"
"<span class=\"lbl\">环境光</span><span class=\"val\" id=\"ap_als\">--</span>"
"<span class=\"lbl\">接近</span><span class=\"val\" id=\"ap_ps\">--</span>"
"</div>\n"
"</div>\n"
"</div>\n"

/* device & control */
"<div class=\"card\">\n"
"<h2>设备控制</h2>\n"
"<div class=\"dev-grid\">"
"<div><span class=\"lbl\">LED</span> <span class=\"val\" id=\"dev_led\">--</span></div>"
"<div><span class=\"lbl\">蜂鸣器</span> <span class=\"val\" id=\"dev_buzzer\">--</span></div>"
"<div><span class=\"lbl\">已静音</span> <span class=\"val\" id=\"dev_muted\">--</span></div>"
"<div><span class=\"lbl\">已确认</span> <span class=\"val\" id=\"dev_acked\">--</span></div>"
"</div>\n"
"<div class=\"btn-row\">\n"
"<button class=\"btn btn-mute\" id=\"btn_mute\" onclick=\"sendCmd('mute_buzzer',this)\">🔇 静音</button>\n"
"<button class=\"btn btn-ack\"  id=\"btn_ack\"  onclick=\"sendCmd('ack_alarm',this)\">✅ 确认报警</button>\n"
"<button class=\"btn btn-demo\" id=\"btn_demo\" onclick=\"sendCmd('demo_alarm',this)\">🧪 测试报警</button>\n"
"</div>\n"
"</div>\n"

/* recent alarms */
"<div class=\"card\">\n"
"<h2>最近报警 <span style=\"font-weight:400;color:var(--text2);margin-left:auto;cursor:pointer\" onclick=\"switchPage('alarms')\">查看全部 →</span></h2>\n"
"<table class=\"alarm-table\">\n"
"<thead><tr><th>时间</th><th>级别</th><th>原因</th><th>详情</th></tr></thead>\n"
"<tbody id=\"recent_alarms_tbody\">\n"
"<tr><td colspan=\"4\" style=\"text-align:center;color:var(--text2)\">加载中...</td></tr>\n"
"</tbody>\n"
"</table>\n"
"</div>\n"

"</div>\n" /* end dashboard */

/* ======== Page: Alarms ======== */
"<div id=\"page-alarms\" class=\"page\">\n"
"<div class=\"card\">\n"
"<h2>报警历史 <span id=\"alarm_page_info\" style=\"font-weight:400;font-size:12px;color:var(--text2);margin-left:8px\"></span></h2>\n"
"<table class=\"alarm-table\">\n"
"<thead><tr><th>时间</th><th>级别</th><th>原因</th><th>震动</th><th>接近</th><th>环境光</th><th>温度</th></tr></thead>\n"
"<tbody id=\"alarms_tbody\">\n"
"<tr><td colspan=\"7\" style=\"text-align:center;color:var(--text2);padding:40px\">加载中...</td></tr>\n"
"</tbody>\n"
"</table>\n"
"<div style=\"display:flex;justify-content:space-between;align-items:center;margin-top:12px\">\n"
"<button class=\"btn btn-demo\" style=\"font-size:11px;padding:6px 14px\" onclick=\"deleteOldAlarms()\">🗑 删除早期记录</button>\n"
"<div style=\"display:flex;gap:6px\">\n"
"<button class=\"btn btn-mute\" style=\"font-size:11px;padding:6px 14px\" id=\"alarm_prev\" onclick=\"alarmPage(-1)\">◀ 上一页</button>\n"
"<span style=\"font-size:12px;color:var(--text2);align-self:center\" id=\"alarm_pager\">1/1</span>\n"
"<button class=\"btn btn-mute\" style=\"font-size:11px;padding:6px 14px\" id=\"alarm_next\" onclick=\"alarmPage(1)\">下一页 ▶</button>\n"
"</div>\n"
"</div>\n"
"</div>\n"
"</div>\n"

/* ======== Page: Camera ======== */
"<div id=\"page-camera\" class=\"page\">\n"
"<div class=\"card\">\n"
"<h2>实时快照</h2>\n"
"<div class=\"cam-preview\" id=\"cam_preview\">\n"
"<span class=\"cam-placeholder\">摄像头离线或未连接</span>\n"
"</div>\n"
"<div style=\"text-align:center;margin-bottom:12px\">\n"
"<span style=\"font-size:12px;color:var(--text2)\" id=\"cam_update_hint\"></span>\n"
"</div>\n"
"</div>\n"
"<div class=\"cam-info\">\n"
"<div class=\"card\"><h2>摄像头</h2><div class=\"ov-val\" id=\"cam_status_val\" style=\"font-size:22px\">离线</div></div>\n"
"<div class=\"card\"><h2>运动检测</h2><div class=\"ov-val\" id=\"cam_motion_val\" style=\"font-size:22px\">--</div></div>\n"
"<div class=\"card\"><h2>人脸计数</h2><div class=\"ov-val\" id=\"cam_faces_val\" style=\"font-size:22px\">0</div></div>\n"
"<div class=\"card\"><h2>推理耗时</h2><div class=\"ov-val\" id=\"cam_infer_val\" style=\"font-size:22px\">--</div></div>\n"
"</div>\n"
"</div>\n"

/* ======== Page: Settings ======== */
"<div id=\"page-settings\" class=\"page\">\n"
"<div class=\"card\">\n"
"<h2>传感器阈值 <span style=\"font-weight:400;font-size:11px;color:var(--text2);margin-left:8px\">修改后点击保存立即生效</span></h2>\n"
"<div class=\"cfg-grid\" id=\"cfg_grid\">\n"
"<div class=\"cfg-item\"><span class=\"cfg-lbl\">加载中...</span></div>\n"
"</div>\n"
"</div>\n"
"<div class=\"card\">\n"
"<h2>系统信息</h2>\n"
"<div class=\"sensor-row\">"
"<span class=\"lbl\">核心进程</span><span class=\"val\" id=\"cfg_hubd\">--</span>"
"<span class=\"lbl\">板卡时间</span><span class=\"val\" id=\"cfg_time\">--</span>"
"</div>\n"
"</div>\n"
"</div>\n"

/* ======== JavaScript ======== */
"<script>\n"
"var connLost=0,currentPage='dashboard',alarmData=[],camOnline=!1,lastState='',audioCtx=null;\n"
"var reasonCN={"
"'none':'无','motion threshold exceeded':'震动超限','motion warning':'震动告警',"
"'proximity alarm':'接近报警','proximity warning':'接近告警',"
"'low ambient light':'光照不足','face intrusion':'人脸入侵',"
"'vision motion detected':'视觉运动','vision motion warning':'视觉运动告警',"
"'mpu6050 sensor fault':'MPU6050故障','ap3216c sensor fault':'AP3216C故障',"
"'alarm acknowledged':'报警已确认','buzzer muted':'蜂鸣器已静音',"
"'demo alarm triggered':'测试报警触发',"
"'sensor_hubd started':'进程启动','sensor_hubd stopped':'进程停止'"
"};\n"
"function trReason(en){return reasonCN[en]||en}\n"

/* ---- clock ---- */
"function tick(){var d=new Date();"
"document.getElementById('clock').textContent="
"('0'+d.getHours()).slice(-2)+':'+('0'+d.getMinutes()).slice(-2)+':'+('0'+d.getSeconds()).slice(-2)}\n"
"setInterval(tick,1000);tick();\n"

/* ---- tab navigation ---- */
"function switchPage(name){\n"
"currentPage=name;\n"
"document.querySelectorAll('.page').forEach(function(p){p.classList.remove('active')});\n"
"document.getElementById('page-'+name).classList.add('active');\n"
"document.querySelectorAll('.tab').forEach(function(t){t.classList.remove('active')});\n"
"document.querySelector('[data-page='+name+']').classList.add('active');\n"
"if(name==='alarms')loadAlarms(200);\n"
"if(name==='camera')refreshCamera();\n"
"if(name==='settings'){fetch('/api/status').then(function(r){return r.json()}).then(function(d){loadSettings(d)});}\n"
"}\n"
"document.querySelectorAll('.tab').forEach(function(t){\n"
"t.addEventListener('click',function(){switchPage(this.dataset.page)})\n"
"});\n"

/* ---- dot helper ---- */
"function setDot(id,on){var d=document.getElementById(id);"
"d.className='dot '+(on?'dot-on':'dot-off')}\n"

/* ---- format uptime ---- */
"function fmtUptime(s){if(s<60)return s+'秒';if(s<3600)return Math.floor(s/60)+'分';"
"var h=Math.floor(s/3600),m=Math.floor((s%3600)/60);"
"if(h<24)return h+'时 '+m+'分';var d=Math.floor(h/24);return d+'天 '+(h%24)+'时'}\n"

/* ---- update dashboard UI ---- */
"function updateUI(d){\n"
"var st=d.state||'UNKNOWN';var stCN={NORMAL:'正常',WARNING:'警告',ALARM:'报警',FAULT:'故障',UNKNOWN:'未知'}[st]||st;\n"
/* overview strip */
"var os=document.getElementById('ov_state');os.className='ov-card state-'+st;"
"os.querySelector('.ov-val').textContent=stCN;\n"
"document.getElementById('ov_alarms').querySelector('.ov-val').textContent="
"(d.alarm||{}).count||0;\n"
"var up=(d.system||{}).uptime_sec||0;\n"
"document.getElementById('ov_uptime').querySelector('.ov-val').textContent=fmtUptime(up);\n"
"document.getElementById('ov_ip').querySelector('.ov-val').textContent=(d.system||{}).ip||'--';\n"
/* MPU6050 */
"var m=d.mpu6050||{};\n"
"document.getElementById('mpu_accel').textContent=(m.ax||0)+' '+(m.ay||0)+' '+(m.az||0);\n"
"document.getElementById('mpu_gyro').textContent=(m.gx||0)+' '+(m.gy||0)+' '+(m.gz||0);\n"
"document.getElementById('mpu_temp').textContent=(m.temp||0).toFixed(1)+' \\u00b0C';\n"
"document.getElementById('mpu_motion').textContent=m.motion_delta||0;\n"
"setDot('mpu_dot',m.online);\n"
/* AP3216C */
"var a=d.ap3216c||{};\n"
"document.getElementById('ap_ir').textContent=a.ir||0;\n"
"document.getElementById('ap_als').textContent=a.als||0;\n"
"document.getElementById('ap_ps').textContent=a.ps||0;\n"
"setDot('ap_dot',a.online);\n"
/* device */
"var dev=d.device||{};\n"
"document.getElementById('dev_led').textContent=dev.led||'--';\n"
"document.getElementById('dev_buzzer').textContent=dev.buzzer||'--';\n"
"var al=d.alarm||{};\n"
"document.getElementById('dev_muted').textContent=al.muted?'YES':'NO';\n"
"document.getElementById('dev_acked').textContent=al.acknowledged?'YES':'NO';\n"
/* button state feedback */
"var bm=document.getElementById('btn_mute');"
"if(al.muted){bm.textContent='🔇 已静音';bm.classList.add('active');bm.disabled=!0}"
"else{bm.textContent='🔇 静音';bm.classList.remove('active');bm.disabled=!1}\n"
"var ba=document.getElementById('btn_ack');"
"if(al.acknowledged){ba.textContent='✅ 已确认';ba.classList.add('active');ba.disabled=!0}"
"else{ba.textContent='✅ 确认报警';ba.classList.remove('active');ba.disabled=!1}\n"
/* vision */
"var vis=d.vision||{};camOnline=vis.camera_online||!1;\n"
/* connection */
"document.getElementById('conn_status').innerHTML="
"'<span class=\"conn-dot conn-live\"></span>在线';connLost=0;\n"
/* page title */
"var emoji={NORMAL:'\\u2705',WARNING:'\\u26a0\\ufe0f',ALARM:'\\ud83d\\udd25',FAULT:'\\u274c',UNKNOWN:'\\u2753'};"
"document.title=(emoji[st]||'')+' '+stCN+' - RickyDuran-EdgeGuard';\n"
/* audio alert on ALARM */
"if(st==='ALARM'&&lastState!=='ALARM'){playAlarm()}"
"lastState=st;\n"
"}\n"

/* ---- load alarm history ---- */
"var alarmPageNum=0,alarmPerPage=15;\n"
"function loadAlarms(limit){\n"
"fetch('/api/alarms?limit='+limit).then(function(r){return r.json()}).then(function(rows){\n"
"alarmData=rows||[];alarmPageNum=0;\n"
"if(document.getElementById('alarms_tbody'))renderPagedAlarms();\n"
"if(document.getElementById('recent_alarms_tbody'))\n"
"renderAlarmTable('recent_alarms_tbody',alarmData.slice(0,5),4);\n"
"}).catch(function(){});\n"
"}\n"
"function renderPagedAlarms(){\n"
"var total=alarmData.length, pages=Math.ceil(total/alarmPerPage)||1;\n"
"if(alarmPageNum>=pages)alarmPageNum=pages-1;\n"
"if(alarmPageNum<0)alarmPageNum=0;\n"
"var start=alarmPageNum*alarmPerPage,end=start+alarmPerPage;\n"
"var page=alarmData.slice(start,end);\n"
"renderAlarmTable('alarms_tbody',page,7);\n"
"var pi=document.getElementById('alarm_page_info');\n"
"if(pi)pi.textContent='共 '+total+' 条';\n"
"var pager=document.getElementById('alarm_pager');\n"
"if(pager)pager.textContent=(alarmPageNum+1)+'/'+pages;\n"
"var prev=document.getElementById('alarm_prev');\n"
"var next=document.getElementById('alarm_next');\n"
"if(prev)prev.disabled=alarmPageNum<=0;\n"
"if(next)next.disabled=alarmPageNum>=pages-1;\n"
"}\n"
"function alarmPage(dir){\n"
"alarmPageNum+=dir;renderPagedAlarms();\n"
"}\n"
"function deleteOldAlarms(){\n"
"if(!confirm('确认删除早期报警记录，仅保留最新 100 条？'))return;\n"
"var body='{\"cmd\":\"delete_old_alarms\",\"keep\":100}';\n"
"fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:body,credentials:'include'}).then(function(r){\n"
"if(r.ok){alert('已删除');loadAlarms(200)}\n"
"else if(r.status===401){alert('需要认证: 请先在仪表盘点击操作按钮登录')}\n"
"}).catch(function(){});\n"
"}\n"

"function renderAlarmTable(tbodyId,rows,cols){\n"
"var tb=document.getElementById(tbodyId);if(!tb)return;\n"
"if(!rows.length){tb.innerHTML='<tr><td colspan='+cols+' style=\"text-align:center;color:var(--text2);padding:30px\">暂无报警记录</td></tr>';return}\n"
"var h='';\n"
"rows.forEach(function(r){\n"
"var badge='<span class=\"badge-sm badge-'+r.state+'\">'+r.state+'</span>';\n"
"if(cols<=4){\n"
"h+='<tr><td>'+r.timestamp+'</td><td>'+badge+'</td><td>'+trReason(r.reason)+'</td><td>'+(r.motion_delta||0)+' / PS'+(r.ps||0)+'</td></tr>';\n"
"}else{\n"
"h+='<tr><td>'+r.timestamp+'</td><td>'+badge+'</td><td>'+trReason(r.reason)+'</td><td>'+(r.motion_delta||0)+'</td><td>'+(r.ps||0)+'</td><td>'+(r.als||0)+'</td><td>'+(r.mpu_temp||0).toFixed(1)+' \\u00b0C</td></tr>';\n"
"}\n"
"});\n"
"tb.innerHTML=h;\n"
"}\n"

/* ---- camera refresh ---- */
"function refreshCamera(){\n"
"var cs=document.getElementById('cam_status_val');\n"
"var cm=document.getElementById('cam_motion_val');\n"
"var cf=document.getElementById('cam_faces_val');\n"
"var ci=document.getElementById('cam_infer_val');\n"
"var cp=document.getElementById('cam_preview');\n"
"var ch=document.getElementById('cam_update_hint');\n"
"fetch('/api/vision').then(function(r){return r.json()}).then(function(v){\n"
"var on=v.camera_online||!1;camOnline=on;\n"
"if(cs){cs.textContent=on?'在线':'离线';cs.style.color=on?'#3fb950':'#f85149'}\n"
"if(cm)cm.textContent=v.motion_detected?'检测到':'无';\n"
"if(cf)cf.textContent=v.face_count||0;\n"
"if(ci)ci.textContent=v.inference_ms?v.inference_ms+' ms':'--';\n"
"if(cp&&on){cp.innerHTML='<img src=\"/api/snapshot?t='+Date.now()+'\" alt=\"snapshot\" onerror=\"this.parentElement.innerHTML=\\'<span class=cam-placeholder>快照加载失败</span>\\'\">';\n"
"if(ch)ch.textContent='更新于 '+new Date().toLocaleTimeString();}\n"
"if(cp&&!on){cp.innerHTML='<span class=\"cam-placeholder\">摄像头离线或未连接</span>';if(ch)ch.textContent=''}\n"
"}).catch(function(){});\n"
"}\n"

/* ---- settings ---- */
"function loadSettings(d){\n"
"var cfg=d.config||{};\n"
"var g=document.getElementById('cfg_grid');if(!g)return;\n"
"var items=["
"{k:'sample_interval_ms',l:'采样周期 (ms)',u:''},"
"{k:'als_low_threshold',l:'环境光下限',u:''},"
"{k:'ps_warning_threshold',l:'接近告警阈值',u:''},"
"{k:'ps_alarm_threshold',l:'接近报警阈值',u:''},"
"{k:'motion_warning_threshold',l:'震动告警阈值',u:''},"
"{k:'motion_alarm_threshold',l:'震动报警阈值',u:''}"
"];\n"
"var h='';\n"
"items.forEach(function(it){var v=cfg[it.k];"
"h+='<div class=\"cfg-item\"><span class=\"cfg-lbl\">'+it.l+'</span>'"
"+'<input type=\"number\" id=\"cfg_'+it.k+'\" value=\"'+(v!==undefined?v:'')+'\" style=\"background:var(--bg);border:1px solid var(--border);color:var(--white);padding:6px 10px;border-radius:6px;font-size:14px;font-weight:700;text-align:center;width:80px\">'"
"+'<button class=\"btn btn-ack\" style=\"padding:6px 12px;font-size:11px;margin:0 4px\" onclick=\"saveConfig(\\x27'+it.k+'\\x27)\">保存</button>'"
"+'<span style=\"font-size:11px;color:var(--green);display:none\" id=\"ok_'+it.k+'\">&#10003;</span>'"
"+'</div>'})\n"
"g.innerHTML=h;\n"
"var hubd=(d.system||{}).sensor_hubd||'--';\n"
"document.getElementById('cfg_hubd').textContent=hubd;\n"
"document.getElementById('cfg_time').textContent=new Date().toLocaleString();\n"
"}\n"
"function updateSettingsInfo(d){\n"
"var hubd=(d.system||{}).sensor_hubd||'--';\n"
"var el=document.getElementById('cfg_hubd');if(el)el.textContent=hubd;\n"
"var et=document.getElementById('cfg_time');if(et)et.textContent=new Date().toLocaleString();\n"
"}\n"
"var authed=!1;\n"
"function ensureAuth(cb){\n"
"if(authed){cb();return}\n"
"var iframe=document.createElement('iframe');\n"
"iframe.style.display='none';iframe.src='/api/cmd?cmd=_ping';\n"
"iframe.onload=function(){authed=!0;document.body.removeChild(iframe);cb()}\n"
"iframe.onerror=function(){document.body.removeChild(iframe)}\n"
"document.body.appendChild(iframe);\n"
"}\n"
"function saveConfig(key){\n"
"var inp=document.getElementById('cfg_'+key);if(!inp)return;\n"
"var val=parseInt(inp.value,10);if(isNaN(val))return;\n"
"var body='{\"cmd\":\"set_config\",\"key\":\"'+key+'\",\"value\":'+val+'}';\n"
"function doSave(){\n"
"fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:body,credentials:'include'}).then(function(r){\n"
"if(r.ok){var ok=document.getElementById('ok_'+key);if(ok){ok.style.display='inline';setTimeout(function(){ok.style.display='none'},2000);authed=!0}}\n"
"else if(r.status===401){authed=!1;alert('认证失败，请重试')}\n"
"}).catch(function(){alert('保存失败')});\n"
"}\n"
"ensureAuth(doSave);\n"
"}\n"

/* ---- main refresh loop ---- */
"function refresh(){\n"
"fetch('/api/status').then(function(r){return r.json()}).then(function(d){\n"
"if(currentPage==='dashboard')updateUI(d);\n"
"if(currentPage==='settings')updateSettingsInfo(d);\n"
"}).catch(function(){\n"
"connLost++;if(connLost>=3){\n"
"document.getElementById('conn_status').innerHTML='<span class=\"conn-dot conn-lost\"></span>离线'}\n"
"});\n"
"}\n"

/* ---- send command ---- */
"function sendCmd(cmd,btn){\n"
"if(btn){btn.disabled=true;setTimeout(function(){btn.disabled=false},2000)}\n"
"var f=function(){fetch('/api/cmd?cmd='+encodeURIComponent(cmd),{credentials:'include'}).then(function(r){if(r.status===401)authed=!1}).catch(function(){})};\n"
"if(!authed){ensureAuth(f)}else{f()}\n"
"}\n"

/* ---- alarm sound ---- */
"function playAlarm(){\n"
"try{\n"
"if(!audioCtx)audioCtx=new(window.AudioContext||window.webkitAudioContext)();\n"
"var o=audioCtx.createOscillator(),g=audioCtx.createGain();\n"
"o.type='square';o.frequency.value=800;g.gain.value=.15;\n"
"o.connect(g);g.connect(audioCtx.destination);\n"
"o.start();setTimeout(function(){o.stop()},300);\n"
"}catch(e){}\n"
"}\n"
"\n"
/* ---- startup ---- */
"setInterval(refresh,1000);refresh();\n"
"setInterval(function(){if(currentPage==='camera')refreshCamera()},3000);\n"
"</script>\n"
"</body>\n"
"</html>\n";
}

/* ---- build HTTP response for GET / ---- */
static void build_dashboard_response(char *resp, size_t size)
{
    const char *html = get_dashboard_html();
    size_t body_len = strlen(html);
    snprintf(resp, size,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             body_len, html);
}

/* ---- build 404 response ---- */
static void build_404(char *resp, size_t size)
{
    const char *body = "Not Found\n";
    snprintf(resp, size,
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             strlen(body), body);
}

/* ---- build 405 response ---- */
static void build_405(char *resp, size_t size)
{
    const char *body = "Method Not Allowed\n";
    snprintf(resp, size,
             "HTTP/1.1 405 Method Not Allowed\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             strlen(body), body);
}

/* ---- build 401 Unauthorized response ---- */
static void build_401(char *resp, size_t size)
{
    const char *body = "Unauthorized\n";
    snprintf(resp, size,
             "HTTP/1.1 401 Unauthorized\r\n"
             "WWW-Authenticate: Basic realm=\"EdgeGuard\"\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             strlen(body), body);
}

/* ---- write full response to socket, tolerate partial writes ---- */
static int send_response(int fd, const char *resp)
{
    size_t total = strlen(resp);
    size_t sent = 0;
    while (sent < total) {
        ssize_t n = send(fd, resp + sent, total - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

/* ---- SSE client management ---- */
static void sse_add_client(int fd)
{
    pthread_mutex_lock(&g_sse_mutex);
    int count = 0;
    struct sse_client *c = g_sse_clients;
    while (c) { count++; c = c->next; }
    if (count >= MAX_SSE_CLIENTS) {
        pthread_mutex_unlock(&g_sse_mutex);
        close(fd);
        return;
    }
    struct sse_client *cli = malloc(sizeof(*cli));
    if (!cli) { pthread_mutex_unlock(&g_sse_mutex); close(fd); return; }
    cli->fd   = fd;
    cli->next = g_sse_clients;
    g_sse_clients = cli;
    pthread_mutex_unlock(&g_sse_mutex);
}

static void sse_remove_client(int fd)
{
    pthread_mutex_lock(&g_sse_mutex);
    struct sse_client **p = &g_sse_clients;
    while (*p) {
        if ((*p)->fd == fd) {
            struct sse_client *t = *p;
            *p = t->next;
            free(t);
            break;
        }
        p = &(*p)->next;
    }
    pthread_mutex_unlock(&g_sse_mutex);
}

static void sse_broadcast(const char *data)
{
    pthread_mutex_lock(&g_sse_mutex);
    struct sse_client **p = &g_sse_clients;
    while (*p) {
        struct sse_client *c = *p;
        size_t len = strlen(data);
        if (send(c->fd, data, len, MSG_NOSIGNAL) != (ssize_t)len) {
            /* client gone — remove from list but DON'T close fd.
               the handler thread owns the fd and will close it. */
            *p = c->next;
            free(c);
        } else {
            p = &(*p)->next;
        }
    }
    pthread_mutex_unlock(&g_sse_mutex);
}

static void *sse_broadcast_thread(void *arg)
{
    (void)arg;
    while (g_sse_running) {
        /* skip file read if no SSE clients connected */
        if (g_sse_clients != NULL) {
            char buf[8192];
            int n = read_file(g_status_path, buf, sizeof(buf));
            if (n > 0 && buf[0] != '\0') {
                if (strcmp(buf, g_sse_last_json) != 0) {
                    /* compact JSON to single line for SSE data: field */
                    char compact[8192];
                    int wi = 0;
                    for (int i = 0; i < n && wi < (int)sizeof(compact) - 1; i++) {
                        if (buf[i] != '\n' && buf[i] != '\r')
                            compact[wi++] = buf[i];
                    }
                    compact[wi] = '\0';
                    char sse_msg[8500];
                    snprintf(sse_msg, sizeof(sse_msg),
                             "data: %s\n\n", compact);
                    sse_broadcast(sse_msg);
                    snprintf(g_sse_last_json, sizeof(g_sse_last_json), "%s", buf);
                }
            }
        }
        usleep(500000); /* 500ms */
    }
    return NULL;
}

/* ---- SQLite alarm query ---- */
static void json_escape_str(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < dst_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = c; }
        } else if (c < 0x20) {
            /* skip control chars */
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
}

static int build_alarms_json(char *out, size_t size, int limit)
{
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(ALARM_DB_PATH, &db,
                              SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        snprintf(out, size, "[]\n");
        return -1;
    }

    const char *sql =
        "SELECT id, timestamp, state, reason, motion_delta, ps, als, "
        "mpu_temp, acknowledged "
        "FROM alarm_events ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        snprintf(out, size, "[]\n");
        return -1;
    }
    sqlite3_bind_int(stmt, 1, limit > 0 ? limit : 50);

    char *pos = out;
    size_t rem = size;
    int written = snprintf(pos, rem, "[");
    if (written < 0) written = 0;
    pos += written; rem -= (size_t)written;

    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *ts    = (const char *)sqlite3_column_text(stmt, 1);
        const char *state = (const char *)sqlite3_column_text(stmt, 2);
        const char *reason= (const char *)sqlite3_column_text(stmt, 3);
        int md  = sqlite3_column_int(stmt, 4);
        int ps  = sqlite3_column_int(stmt, 5);
        int als = sqlite3_column_int(stmt, 6);
        double tmp = sqlite3_column_double(stmt, 7);
        int ack = sqlite3_column_int(stmt, 8);

        /* escape strings for safe JSON embedding */
        char ets[64], est[32], erea[128];
        json_escape_str(ts ? ts : "", ets, sizeof(ets));
        json_escape_str(state ? state : "", est, sizeof(est));
        json_escape_str(reason ? reason : "", erea, sizeof(erea));

        if (rem < 256) break;
        written = snprintf(pos, rem,
            "%s\n  {\"id\":%d,\"timestamp\":\"%s\",\"state\":\"%s\","
            "\"reason\":\"%s\",\"motion_delta\":%d,\"ps\":%d,\"als\":%d,"
            "\"mpu_temp\":%.1f,\"acknowledged\":%d}",
            first ? "" : ",",
            sqlite3_column_int(stmt, 0),
            ets, est, erea, md, ps, als, tmp, ack);
        if (written < 0) written = 0;
        pos += written; rem -= (size_t)written;
        first = 0;
    }
    written = snprintf(pos, rem, "\n]\n");
    if (written > 0) { pos += written; rem -= (size_t)written; }
    (void)rem;

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}

/* ---- per-connection handler (runs in a detached thread) ---- */
static void *connection_handler(void *arg)
{
    int client_fd = (int)(intptr_t)arg;
    char buf[REQ_BUF_SIZE];

    /* set receive timeout */
    struct timeval tv;
    tv.tv_sec  = RECV_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(buf, 0, sizeof(buf));
    ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return NULL;
    }
    buf[n] = '\0';

    /* parse request line */
    char method[16]  = {0};
    char path[256]   = {0};
    char version[16] = {0};
    if (sscanf(buf, "%15s %255s %15s", method, path, version) != 3) {
        char resp[256];
        build_404(resp, sizeof(resp));
        send_response(client_fd, resp);
        close(client_fd);
        return NULL;
    }

    /* separate path from query string */
    char *query = strchr(path, '?');
    if (query) {
        *query = '\0';
        query++;
    }

    char resp[RESP_BUF_SIZE];

    /* ---- auth check for write operations ---- */
#if AUTH_ENABLE
    if (!strcmp(path, "/api/cmd")) {
        int authed = 0;
        const char *auth_hdr = strstr(buf, "Authorization: Basic ");
        if (auth_hdr) {
            auth_hdr += 21;  /* skip "Authorization: Basic " */
            /* Copy only the base64 token (stop at \r, \n, or end) */
            char b64[256];
            {
                int bi = 0;
                while (auth_hdr[bi] && auth_hdr[bi] != '\r'
                       && auth_hdr[bi] != '\n' && bi < 255)
                    { b64[bi] = auth_hdr[bi]; bi++; }
                b64[bi] = '\0';
            }
            char decoded[128];
            if (base64_decode(b64, decoded, sizeof(decoded)) > 0) {
                char expected[128];
                snprintf(expected, sizeof(expected), "%s:%s", g_auth_user, g_auth_pass);
                if (!strcmp(decoded, expected))
                    authed = 1;
            }
        }
        if (!authed) {
            build_401(resp, sizeof(resp));
            send_response(client_fd, resp);
            close(client_fd);
            return NULL;
        }
    }
#endif

    /* ---- route dispatch ---- */
    if (!strcmp(method, "GET") && !strcmp(path, "/")) {
        build_dashboard_response(resp, sizeof(resp));

    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/status")) {
        build_status_response(resp, sizeof(resp));

    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/cmd")) {
        /* GET /api/cmd?cmd=mute_buzzer */
        if (query) {
            char *p = strstr(query, "cmd=");
            if (p) {
                p += 4;
                char cmd[64] = {0};
                int i = 0;
                while (*p && *p != '&' && i < 63) cmd[i++] = *p++;
                cmd[i] = '\0';
                url_decode(cmd);

                if (cmd[0]) {
                    char json[128];
                    snprintf(json, sizeof(json),
                             "{\"cmd\":\"%s\"}\n", cmd);
                    write_cmd_file(json);
                }
            }
        }
        build_cmd_ok_response(resp, sizeof(resp));

    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/cmd")) {
        /* extract body via Content-Length */
        const char *cl = strstr(buf, "Content-Length:");
        if (!cl) cl = strstr(buf, "content-length:");
        int body_len = 0;
        if (cl) {
            cl += 15;
            while (*cl == ' ') cl++;
            body_len = atoi(cl);
        }

        /* find body start (after \r\n\r\n) */
        const char *body = strstr(buf, "\r\n\r\n");
        if (body) {
            body += 4;
            int remaining = (int)(buf + n - body);
            if (remaining > 0 && body_len > 0 && remaining >= body_len) {
                /* extract JSON body */
                char json[256];
                int copy_len = body_len < (int)sizeof(json) - 1
                               ? body_len : (int)sizeof(json) - 1;
                memcpy(json, body, copy_len);
                json[copy_len] = '\0';
                write_cmd_file(json);
            }
        }
        build_cmd_ok_response(resp, sizeof(resp));

    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/stream")) {
        /* SSE — keep connection open */
        const char *hdr =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n";
        send_response(client_fd, hdr);
        sse_add_client(client_fd);
        /* block until client disconnects */
        char dummy[64];
        while (g_sse_running) {
            ssize_t r = recv(client_fd, dummy, sizeof(dummy), 0);
            if (r <= 0) break;
        }
        sse_remove_client(client_fd);
        close(client_fd);
        return NULL;

    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/snapshot")) {
        /* Find the latest JPEG snapshot */
        char snap_path[512];
        snap_path[0] = '\0';
        /* Use a shell one-liner to get the newest file.  Keeps the
           HTTP server simple and avoids a directory-scan in C. */
        {
            FILE *fp = popen(
                "ls -t /var/log/edgeguard/snapshots/*.jpg 2>/dev/null | head -1",
                "r");
            if (fp) {
                if (fgets(snap_path, sizeof(snap_path), fp)) {
                    size_t len = strlen(snap_path);
                    if (len > 0 && snap_path[len-1] == '\n')
                        snap_path[len-1] = '\0';
                }
                pclose(fp);
            }
        }

        if (snap_path[0]) {
            FILE *img = fopen(snap_path, "rb");
            if (img) {
                fseek(img, 0, SEEK_END);
                long sz = ftell(img);
                rewind(img);
                if (sz > 0 && sz < 1024*1024) {  /* 1 MB limit */
                    /* send header first */
                    char hdr[256];
                    snprintf(hdr, sizeof(hdr),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: image/jpeg\r\n"
                             "Content-Length: %ld\r\n"
                             "Connection: close\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "\r\n", sz);
                    send_response(client_fd, hdr);
                    /* then send file content */
                    char *imgbuf = malloc(sz);
                    if (imgbuf) {
                        fread(imgbuf, 1, sz, img);
                        size_t sent = 0;
                        while (sent < (size_t)sz) {
                            ssize_t n = send(client_fd, imgbuf + sent,
                                             (size_t)sz - sent, MSG_NOSIGNAL);
                            if (n <= 0) break;
                            sent += (size_t)n;
                        }
                        free(imgbuf);
                    }
                }
                fclose(img);
                close(client_fd);
                return NULL;
            }
        }
        /* fallthrough — no snapshot available */
        build_404(resp, sizeof(resp));

    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/vision")) {
        char vision_buf[2048];
        int n = read_file("/tmp/edgeguard_vision.json",
                          vision_buf, sizeof(vision_buf));
        if (n < 0 || vision_buf[0] == '\0') {
            snprintf(vision_buf, sizeof(vision_buf),
                     "{\"camera_online\":false,\"motion_detected\":false,"
                     "\"face_count\":0,\"snapshot_path\":null,"
                     "\"inference_ms\":0,\"face_verify_result\":null}\n");
            n = (int)strlen(vision_buf);
        }
        size_t blen = (size_t)n;
        snprintf(resp, sizeof(resp),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "\r\n"
                 "%s", blen, vision_buf);

    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/alarms")) {
        int limit = 50;
        if (query) {
            const char *lp = strstr(query, "limit=");
            if (lp) { lp += 6; limit = atoi(lp); if (limit <= 0) limit = 50; }
        }
        /* heap alloc — RESP_BUF_SIZE already 32KB on stack (thread stack=64KB) */
        int abuf_size = RESP_BUF_SIZE - 512;
        char *alarms_json = malloc(abuf_size);
        if (!alarms_json) {
            snprintf(resp, sizeof(resp),
                     "HTTP/1.1 500\r\nContent-Length: 21\r\n\r\n"
                     "{\"error\":\"no memory\"}");
            send_response(client_fd, resp);
            close(client_fd);
            return NULL;
        }
        build_alarms_json(alarms_json, abuf_size, limit);
        size_t blen = strlen(alarms_json);
        snprintf(resp, sizeof(resp),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "\r\n"
                 "%s", blen, alarms_json);
        send_response(client_fd, resp);
        free(alarms_json);
        close(client_fd);
        return NULL;

    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/alarms/count")) {
        /* fast count query */
        char count_json[128];
        sqlite3 *db = NULL;
        int rc = sqlite3_open_v2(ALARM_DB_PATH, &db,
                                  SQLITE_OPEN_READONLY, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(db,
                    "SELECT COUNT(*) FROM alarm_events;",
                    -1, &stmt, NULL) == SQLITE_OK) {
                int total = 0;
                if (sqlite3_step(stmt) == SQLITE_ROW)
                    total = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
                snprintf(count_json, sizeof(count_json),
                         "{\"count\":%d}\n", total);
            } else {
                snprintf(count_json, sizeof(count_json), "{\"count\":0}\n");
            }
            sqlite3_close(db);
        } else {
            snprintf(count_json, sizeof(count_json), "{\"count\":0}\n");
            if (db) sqlite3_close(db);
        }
        size_t clen = strlen(count_json);
        snprintf(resp, sizeof(resp),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "\r\n"
                 "%s", clen, count_json);
        send_response(client_fd, resp);
        close(client_fd);
        return NULL;

    } else if (!strcmp(method, "OPTIONS")) {
        /* CORS preflight */
        snprintf(resp, sizeof(resp),
                 "HTTP/1.1 204 No Content\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                 "Access-Control-Allow-Headers: Content-Type\r\n"
                 "Connection: close\r\n"
                 "\r\n");

    } else {
        build_404(resp, sizeof(resp));
    }

    send_response(client_fd, resp);
    close(client_fd);
    return NULL;
}

/* ---- main server loop ---- */
int main(int argc, char *argv[])
{
    /* parse arguments */
    snprintf(g_status_path, sizeof(g_status_path), "%s", DEFAULT_STATUS_PATH);
    snprintf(g_cmd_path,    sizeof(g_cmd_path),    "%s", DEFAULT_CMD_PATH);

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --port <N>      listen port (default %d)\n", DEFAULT_PORT);
            printf("  --status <path> status JSON path (default %s)\n",
                   DEFAULT_STATUS_PATH);
            printf("  --cmd <path>    command JSON path (default %s)\n",
                   DEFAULT_CMD_PATH);
            printf("  --auth-user <u> basic auth user (default admin)\n");
            printf("  --auth-pass <p> basic auth pass (default edgeguard)\n");
            printf("  -h, --help      this help\n");
            return 0;
        } else if (!strcmp(argv[i], "--port") && i + 1 < argc) {
            g_port = atoi(argv[++i]);
            if (g_port <= 0 || g_port > 65535) g_port = DEFAULT_PORT;
        } else if (!strcmp(argv[i], "--status") && i + 1 < argc) {
            snprintf(g_status_path, sizeof(g_status_path), "%s", argv[++i]);
        } else if (!strcmp(argv[i], "--cmd") && i + 1 < argc) {
            snprintf(g_cmd_path, sizeof(g_cmd_path), "%s", argv[++i]);
        } else if (!strcmp(argv[i], "--auth-user") && i + 1 < argc) {
            snprintf(g_auth_user, sizeof(g_auth_user), "%s", argv[++i]);
        } else if (!strcmp(argv[i], "--auth-pass") && i + 1 < argc) {
            snprintf(g_auth_pass, sizeof(g_auth_pass), "%s", argv[++i]);
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    snprintf(g_cmd_tmp_path, sizeof(g_cmd_tmp_path),
             "%s.tmp", g_cmd_path);

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    /* start SSE broadcast thread */
    pthread_t sse_tid;
    pthread_create(&sse_tid, NULL, sse_broadcast_thread, NULL);

    /* create socket */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        fprintf(stderr, "[edgeguard_httpd] socket: %s\n", strerror(errno));
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)g_port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[edgeguard_httpd] bind port %d: %s\n",
                g_port, strerror(errno));
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 10) < 0) {
        fprintf(stderr, "[edgeguard_httpd] listen: %s\n", strerror(errno));
        close(listen_fd);
        return 1;
    }

    char ip[64];
    get_eth0_ip(ip, sizeof(ip));
    printf("[edgeguard_httpd] listening on %s:%d\n", ip, g_port);
    printf("[edgeguard_httpd] status: %s  cmd: %s\n",
           g_status_path, g_cmd_path);

    while (g_running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);

        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        int ret = select(listen_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;  /* timeout, check g_running */

        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "[edgeguard_httpd] accept: %s\n", strerror(errno));
            continue;
        }

        /* spawn handler thread */
        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_attr_setstacksize(&attr, 64 * 1024);

        if (pthread_create(&tid, &attr, connection_handler,
                          (void *)(intptr_t)client_fd) != 0) {
            fprintf(stderr, "[edgeguard_httpd] pthread_create failed\n");
            close(client_fd);
        }
        pthread_attr_destroy(&attr);
    }

    printf("[edgeguard_httpd] shutting down\n");
    g_sse_running = 0;
    pthread_join(sse_tid, NULL);
    close(listen_fd);
    return 0;
}
