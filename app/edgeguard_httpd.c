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
#define RESP_BUF_SIZE       16384
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

/* ---- get eth0 IPv4 address via ioctl ---- */
static void get_eth0_ip(char *out, size_t size)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        snprintf(out, size, "0.0.0.0");
        return;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "eth0");

    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
        close(fd);
        snprintf(out, size, "0.0.0.0");
        return;
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
    const char *ip = inet_ntoa(addr->sin_addr);
    snprintf(out, size, "%s", ip ? ip : "0.0.0.0");
    close(fd);
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

/* ---- embedded HTML dashboard ---- */
static const char *get_dashboard_html(void)
{
    return
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"<meta charset=\"UTF-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"<title>EdgeGuard Remote Dashboard</title>\n"
"<style>\n"
"*{margin:0;padding:0;box-sizing:border-box}\n"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
"background:linear-gradient(135deg,#0d1117 0%,#161b22 100%);"
"color:#c9d1d9;min-height:100vh}\n"
".header{background:linear-gradient(90deg,#0d419d,#1a1a2e);padding:20px 24px;"
"display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:12px}\n"
".header h1{font-size:20px;color:#fff;margin:0}\n"
".header-info{display:flex;align-items:center;gap:12px;font-size:12px;color:#8b949e}\n"
".conn-dot{width:8px;height:8px;border-radius:50%;display:inline-block}\n"
".conn-live{background:#3fb950;box-shadow:0 0 6px #3fb950}\n"
".conn-lost{background:#f85149;box-shadow:0 0 6px #f85149}\n"
".container{max-width:760px;margin:0 auto;padding:20px}\n"
".status-bar{display:flex;align-items:center;gap:12px;margin-bottom:16px;flex-wrap:wrap}\n"
".state-badge{display:inline-block;padding:8px 20px;border-radius:6px;"
"font-weight:700;font-size:20px;letter-spacing:.5px}\n"
".state-NORMAL{background:#1a3a1a;color:#3fb950;box-shadow:0 0 12px rgba(63,185,80,.3)}\n"
".state-WARNING{background:#3a2e00;color:#d29922;box-shadow:0 0 12px rgba(210,153,34,.3)}\n"
".state-ALARM{background:#490202;color:#f85149;box-shadow:0 0 16px rgba(248,81,73,.5);"
"animation:blink .5s infinite}\n"
".state-FAULT{background:#3a1111;color:#f85149;box-shadow:0 0 12px rgba(248,81,73,.3)}\n"
".state-WAITING{background:#1a1a2e;color:#8b949e}\n"
"@keyframes blink{50%{opacity:.5;box-shadow:0 0 24px rgba(248,81,73,.8)}}\n"
".reason{font-size:14px;color:#8b949e}\n"
".grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px}\n"
"@media(max-width:520px){.grid2{grid-template-columns:1fr}}\n"
".card{background:#161b22;border:1px solid #21262d;border-radius:8px;"
"padding:14px 18px;transition:border-color .2s,box-shadow .2s}\n"
".card:hover{border-color:#30363d;box-shadow:0 4px 12px rgba(0,0,0,.3)}\n"
".card h2{font-size:13px;color:#58a6ff;margin-bottom:10px;text-transform:uppercase;"
"letter-spacing:.5px}\n"
".row{display:flex;flex-wrap:wrap;gap:6px 20px;font-size:13px}\n"
".row .val{color:#f0f6fc;font-weight:500}\n"
".row .lbl{color:#8b949e}\n"
".dot{display:inline-block;width:7px;height:7px;border-radius:50%;margin-right:4px}\n"
".dot-on{background:#3fb950;box-shadow:0 0 4px #3fb950}\n"
".dot-off{background:#f85149;box-shadow:0 0 4px #f85149}\n"
".actions{display:flex;gap:10px;margin-top:18px;flex-wrap:wrap}\n"
".actions button{padding:11px 22px;border:none;border-radius:6px;"
"font-size:14px;font-weight:600;cursor:pointer;color:#fff;"
"transition:transform .1s,box-shadow .1s,opacity .2s}\n"
".actions button:active{transform:scale(.96)}\n"
".actions button:disabled{opacity:.5;cursor:not-allowed;transform:none}\n"
".btn-mute{background:#d29922;box-shadow:0 2px 6px rgba(210,153,34,.3)}\n"
".btn-mute:hover:not(:disabled){background:#e5a828}\n"
".btn-ack{background:#238636;box-shadow:0 2px 6px rgba(35,134,54,.3)}\n"
".btn-ack:hover:not(:disabled){background:#2ea043}\n"
".btn-demo{background:#b8600f;box-shadow:0 2px 6px rgba(184,96,15,.3)}\n"
".btn-demo:hover:not(:disabled){background:#d47015}\n"
".footer{font-size:11px;color:#484f58;margin-top:20px;text-align:center}\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<div class=\"header\">\n"
"<h1>EdgeGuard Remote Dashboard</h1>\n"
"<div class=\"header-info\">"
"<span id=\"conn_status\"><span class=\"conn-dot conn-live\"></span> LIVE</span>"
"<span>IP: <strong id=\"hdr_ip\">--</strong></span>"
"</div>\n"
"</div>\n"
"<div class=\"container\">\n"
"<div class=\"status-bar\">\n"
"<div class=\"state-badge\" id=\"state_badge\">Connecting...</div>\n"
"<div class=\"reason\" id=\"reason_line\"></div>\n"
"</div>\n"
"<div class=\"grid2\">\n"
"<div class=\"card\">\n"
"<h2>MPU6050 <span class=\"dot\" id=\"mpu_dot\"></span></h2>\n"
"<div class=\"row\">"
"<span class=\"lbl\">Accel:</span><span class=\"val\" id=\"mpu_accel\">--</span>"
"<span class=\"lbl\">Gyro:</span><span class=\"val\" id=\"mpu_gyro\">--</span>"
"<span class=\"lbl\">Temp:</span><span class=\"val\" id=\"mpu_temp\">--</span>"
"<span class=\"lbl\">Motion:</span><span class=\"val\" id=\"mpu_motion\">--</span>"
"</div>\n"
"</div>\n"
"<div class=\"card\">\n"
"<h2>AP3216C <span class=\"dot\" id=\"ap_dot\"></span></h2>\n"
"<div class=\"row\">"
"<span class=\"lbl\">IR:</span><span class=\"val\" id=\"ap_ir\">--</span>"
"<span class=\"lbl\">ALS:</span><span class=\"val\" id=\"ap_als\">--</span>"
"<span class=\"lbl\">PS:</span><span class=\"val\" id=\"ap_ps\">--</span>"
"</div>\n"
"</div>\n"
"</div>\n"
"<div class=\"card\">\n"
"<h2>Device &amp; Alarm</h2>\n"
"<div class=\"row\">"
"<span class=\"lbl\">LED:</span><span class=\"val\" id=\"dev_led\">--</span>"
"<span class=\"lbl\">Buzzer:</span><span class=\"val\" id=\"dev_buzzer\">--</span>"
"<span class=\"lbl\">Alarms:</span><span class=\"val\" id=\"alarm_count\">0</span>"
"<span class=\"lbl\">Muted:</span><span class=\"val\" id=\"alarm_muted\">--</span>"
"<span class=\"lbl\">Acked:</span><span class=\"val\" id=\"alarm_acked\">--</span>"
"</div>\n"
"</div>\n"
"<div class=\"card\">\n"
"<h2>System</h2>\n"
"<div class=\"row\">"
"<span class=\"lbl\">Uptime:</span><span class=\"val\" id=\"sys_uptime\">--</span>"
"<span class=\"lbl\">IP:</span><span class=\"val\" id=\"sys_ip\">--</span>"
"</div>\n"
"</div>\n"
"<div class=\"actions\">\n"
"<button class=\"btn-mute\" id=\"btn_mute\" onclick=\"sendCmd('mute_buzzer',this)\">Mute Buzzer</button>\n"
"<button class=\"btn-ack\" id=\"btn_ack\" onclick=\"sendCmd('ack_alarm',this)\">ACK Alarm</button>\n"
"<button class=\"btn-demo\" id=\"btn_demo\" onclick=\"sendCmd('demo_alarm',this)\">Demo Alarm</button>\n"
"</div>\n"
"<div class=\"footer\">EdgeGuard-6ULL &mdash; HTTP Remote Service</div>\n"
"</div>\n"
"<script>\n"
"var connLost=0;\n"
"function setDot(id,on){var d=document.getElementById(id);"
"d.className='dot '+(on?'dot-on':'dot-off');}\n"
"function updateUI(d){\n"
"var st=d.state||'UNKNOWN';\n"
"var b=document.getElementById('state_badge');\n"
"b.textContent=st;b.className='state-badge state-'+st;\n"
"document.getElementById('reason_line').textContent=d.alarm_reason||'';\n"
"var m=d.mpu6050||{};\n"
"document.getElementById('mpu_accel').textContent=(m.ax||0)+', '+(m.ay||0)+', '+(m.az||0);\n"
"document.getElementById('mpu_gyro').textContent=(m.gx||0)+', '+(m.gy||0)+', '+(m.gz||0);\n"
"document.getElementById('mpu_temp').textContent=(m.temp||0)+' C';\n"
"document.getElementById('mpu_motion').textContent=m.motion_delta||0;\n"
"setDot('mpu_dot',m.online);\n"
"var a=d.ap3216c||{};\n"
"document.getElementById('ap_ir').textContent=a.ir||0;\n"
"document.getElementById('ap_als').textContent=a.als||0;\n"
"document.getElementById('ap_ps').textContent=a.ps||0;\n"
"setDot('ap_dot',a.online);\n"
"var dev=d.device||{};\n"
"document.getElementById('dev_led').textContent=dev.led||'--';\n"
"document.getElementById('dev_buzzer').textContent=dev.buzzer||'--';\n"
"var al=d.alarm||{};\n"
"document.getElementById('alarm_count').textContent=al.count||0;\n"
"document.getElementById('alarm_muted').textContent=al.muted?'YES':'NO';\n"
"document.getElementById('alarm_acked').textContent=al.acknowledged?'YES':'NO';\n"
"var s=d.system||{};\n"
"document.getElementById('sys_uptime').textContent=(s.uptime_sec||0)+' s';\n"
"document.getElementById('sys_ip').textContent=s.ip||'--';\n"
"document.getElementById('hdr_ip').textContent=s.ip||'--';\n"
"var cs=document.getElementById('conn_status');\n"
"cs.innerHTML='<span class=\"conn-dot conn-live\"></span> LIVE';\n"
"connLost=0;\n"
"}\n"
"async function refresh(){\n"
"try{\n"
"var r=await fetch('/api/status');\n"
"if(!r.ok)throw new Error('status '+r.status);\n"
"updateUI(await r.json());\n"
"}catch(e){\n"
"connLost++;\n"
"if(connLost>=3){\n"
"var cs=document.getElementById('conn_status');\n"
"cs.innerHTML='<span class=\"conn-dot conn-lost\"></span> OFFLINE';\n"
"}\n"
"}\n"
"}\n"
"function sendCmd(cmd,btn){\n"
"if(btn){btn.disabled=true;setTimeout(function(){btn.disabled=false;},2000);}\n"
"fetch('/api/cmd?cmd='+encodeURIComponent(cmd)).catch(function(){});\n"
"}\n"
"setInterval(refresh,1000);refresh();\n"
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
            char decoded[128];
            if (base64_decode(auth_hdr, decoded, sizeof(decoded)) > 0) {
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

    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/alarms")) {
        int limit = 50;
        if (query) {
            const char *lp = strstr(query, "limit=");
            if (lp) { lp += 6; limit = atoi(lp); if (limit <= 0) limit = 50; }
        }
        char alarms_json[8192];
        build_alarms_json(alarms_json, sizeof(alarms_json), limit);
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
