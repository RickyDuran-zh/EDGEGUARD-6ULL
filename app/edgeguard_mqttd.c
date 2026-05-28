// edgeguard_mqttd.c — EdgeGuard MQTT Telemetry Daemon
// Publishes sensor state to an MQTT broker on state change.
// Minimal MQTT 3.1.1 client — pure POSIX sockets, zero dependencies.

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

/* ---- defaults ---- */
#define DEFAULT_BROKER      "192.168.10.1"
#define DEFAULT_PORT_STR    "1883"
#define DEFAULT_STATUS_PATH "/tmp/edgeguard_status.json"
#define DEFAULT_TOPIC_PFX   "edgeguard"
#define DEFAULT_KEEPALIVE   30
#define BUF_SIZE            8192
#define RECONNECT_DELAY_SEC 5

/* ---- MQTT packet types ---- */
#define MQTT_CONNECT    1
#define MQTT_CONNACK    2
#define MQTT_PUBLISH    3
#define MQTT_PINGREQ   12
#define MQTT_PINGRESP  13
#define MQTT_DISCONNECT 14

/* ---- global state ---- */
static volatile int g_running = 1;
static char g_broker_host[128] = DEFAULT_BROKER;
static char g_broker_port[8]   = DEFAULT_PORT_STR;
static char g_status_path[256]  = DEFAULT_STATUS_PATH;
static char g_topic_pfx[64]     = DEFAULT_TOPIC_PFX;
static int  g_keepalive         = DEFAULT_KEEPALIVE;

static void handle_signal(int sig) { (void)sig; g_running = 0; }

/* ---- read entire file ---- */
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

/* ---- minimal JSON value extractor (same style as sensor_hubd) ---- */
static int json_str_val(const char *json, const char *key,
                         char *out, size_t out_size)
{
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return -1;
    p = strchr(p + strlen(pat), ':');
    if (!p) return -1;
    while (*p == ' ' || *p == ':' || *p == '"') p++;
    int i = 0;
    while (*p && *p != '"' && *p != '\n' && i < (int)out_size - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return 0;
}

/* ---- MQTT variable-length encoding ---- */
static int mqtt_encode_len(unsigned char *buf, int len)
{
    int i = 0;
    do {
        unsigned char b = len & 0x7f;
        len >>= 7;
        if (len > 0) b |= 0x80;
        buf[i++] = b;
    } while (len > 0 && i < 4);
    return i;
}

/* ---- MQTT: build CONNECT packet ---- */
static int mqtt_build_connect(unsigned char *buf, int buf_size,
                               const char *client_id, int keepalive)
{
    /* variable header: protocol name + version + flags + keepalive */
    int pos = 0;
    int id_len = (int)strlen(client_id);

    /* fixed header */
    buf[pos++] = MQTT_CONNECT << 4;

    /* remaining length: 10 (var header) + 2 + id_len */
    int rem_len = 10 + 2 + id_len;
    pos += mqtt_encode_len(buf + pos, rem_len);

    /* protocol name "MQTT" (4 bytes) */
    buf[pos++] = 0; buf[pos++] = 4;
    buf[pos++] = 'M'; buf[pos++] = 'Q'; buf[pos++] = 'T'; buf[pos++] = 'T';

    /* protocol level 4 (MQTT 3.1.1) */
    buf[pos++] = 4;

    /* connect flags: clean session */
    buf[pos++] = 0x02;

    /* keepalive MSB, LSB */
    buf[pos++] = (keepalive >> 8) & 0xff;
    buf[pos++] = keepalive & 0xff;

    /* client ID (length-prefixed) */
    buf[pos++] = (id_len >> 8) & 0xff;
    buf[pos++] = id_len & 0xff;
    memcpy(buf + pos, client_id, id_len);
    pos += id_len;

    return pos;
}

/* ---- MQTT: build PUBLISH packet (QoS 0) ---- */
static int mqtt_build_publish(unsigned char *buf, int buf_size,
                               const char *topic, const char *payload)
{
    int pos = 0;
    int t_len = (int)strlen(topic);
    int p_len = (int)strlen(payload);

    /* fixed header: PUBLISH, QoS 0, no retain */
    buf[pos++] = MQTT_PUBLISH << 4;

    /* remaining length: topic_len(2) + topic + payload */
    int rem_len = 2 + t_len + p_len;
    pos += mqtt_encode_len(buf + pos, rem_len);

    /* topic (length-prefixed) */
    buf[pos++] = (t_len >> 8) & 0xff;
    buf[pos++] = t_len & 0xff;
    memcpy(buf + pos, topic, t_len);
    pos += t_len;

    /* payload */
    memcpy(buf + pos, payload, p_len);
    pos += p_len;

    return pos;
}

/* ---- MQTT: build PINGREQ packet ---- */
static int mqtt_build_pingreq(unsigned char *buf, int buf_size)
{
    (void)buf_size;
    buf[0] = MQTT_PINGREQ << 4;
    buf[1] = 0;
    return 2;
}

/* ---- blocking send all ---- */
static int send_all(int fd, const unsigned char *buf, int len)
{
    int sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += (int)n;
    }
    return 0;
}

/* ---- blocking recv exactly N bytes ---- */
static int recv_exact(int fd, unsigned char *buf, int len, int timeout_sec)
{
    struct timeval tv;
    tv.tv_sec = timeout_sec; tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int got = 0;
    while (got < len) {
        ssize_t n = recv(fd, buf + got, len - got, 0);
        if (n <= 0) return -1;
        got += (int)n;
    }
    return 0;
}

/* ---- MQTT: connect to broker, return socket fd or -1 ---- */
static int mqtt_connect(void)
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(g_broker_host, g_broker_port, &hints, &res);
    if (ret != 0) {
        fprintf(stderr, "[mqttd] getaddrinfo %s:%s: %s\n",
                g_broker_host, g_broker_port, gai_strerror(ret));
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        fprintf(stderr, "[mqttd] connect %s:%s: %s\n",
                g_broker_host, g_broker_port, strerror(errno));
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);

    /* send CONNECT */
    unsigned char pkt[256];
    char client_id[64];
    snprintf(client_id, sizeof(client_id), "edgeguard-%04x", getpid() & 0xffff);
    int pkt_len = mqtt_build_connect(pkt, sizeof(pkt), client_id, g_keepalive);
    if (send_all(fd, pkt, pkt_len) < 0) { close(fd); return -1; }

    /* read CONNACK (4 bytes: type, remaining_len(2), flags, return_code) */
    unsigned char resp[4];
    if (recv_exact(fd, resp, 4, 5) < 0) { close(fd); return -1; }
    if (resp[0] != (MQTT_CONNACK << 4) || resp[3] != 0) {
        fprintf(stderr, "[mqttd] CONNACK error: rc=%d\n", resp[3]);
        close(fd);
        return -1;
    }

    printf("[mqttd] connected to %s:%s\n", g_broker_host, g_broker_port);
    return fd;
}

/* ---- MQTT: publish a message ---- */
static int mqtt_publish(int fd, const char *subtopic, const char *payload)
{
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/%s", g_topic_pfx, subtopic);

    unsigned char pkt[8700];
    int len = mqtt_build_publish(pkt, sizeof(pkt), topic, payload);
    if (len < 0) return -1;
    if (send_all(fd, pkt, len) < 0) return -1;

    printf("[mqttd] PUB %s\n", topic);
    return 0;
}

/* ---- MQTT: send PINGREQ and wait for PINGRESP ---- */
static int mqtt_ping(int fd)
{
    unsigned char pkt[2];
    int len = mqtt_build_pingreq(pkt, sizeof(pkt));
    if (send_all(fd, pkt, len) < 0) return -1;

    unsigned char resp[2];
    if (recv_exact(fd, resp, 2, 5) < 0) return -1;
    if (resp[0] != (MQTT_PINGRESP << 4)) return -1;
    return 0;
}

/* ---- main ---- */
int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("  --broker host:port  (default %s:%s)\n",
                   DEFAULT_BROKER, DEFAULT_PORT_STR);
            printf("  --status <path>     (default %s)\n", DEFAULT_STATUS_PATH);
            printf("  --topic <prefix>    (default %s)\n", DEFAULT_TOPIC_PFX);
            printf("  -h, --help\n");
            return 0;
        } else if (!strcmp(argv[i], "--broker") && i + 1 < argc) {
            char *colon = strchr(argv[++i], ':');
            if (colon) {
                *colon = '\0';
                snprintf(g_broker_host, sizeof(g_broker_host), "%s", argv[i]);
                snprintf(g_broker_port, sizeof(g_broker_port), "%s", colon + 1);
            } else {
                snprintf(g_broker_host, sizeof(g_broker_host), "%s", argv[i]);
            }
        } else if (!strcmp(argv[i], "--status") && i + 1 < argc) {
            snprintf(g_status_path, sizeof(g_status_path), "%s", argv[++i]);
        } else if (!strcmp(argv[i], "--topic") && i + 1 < argc) {
            snprintf(g_topic_pfx, sizeof(g_topic_pfx), "%s", argv[++i]);
        } else {
            fprintf(stderr, "unknown: %s\n", argv[i]);
            return 1;
        }
    }

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    printf("[mqttd] broker=%s:%s topic=%s/%s\n",
           g_broker_host, g_broker_port, g_topic_pfx, "+");

    int mqtt_fd = -1;
    char last_state[32] = "";
    int ping_counter = 0;
    int status_counter = 0;

    while (g_running) {
        /* (re)connect */
        if (mqtt_fd < 0) {
            mqtt_fd = mqtt_connect();
            if (mqtt_fd < 0) {
                printf("[mqttd] reconnect in %d s...\n", RECONNECT_DELAY_SEC);
                sleep(RECONNECT_DELAY_SEC);
                if (!g_running) break;
                continue;
            }
            /* publish online status */
            mqtt_publish(mqtt_fd, "status", "{\"online\":true}");
            last_state[0] = '\0';
            ping_counter = 0;
            status_counter = 0;
        }

        /* read status JSON */
        char json[BUF_SIZE];
        if (read_file(g_status_path, json, sizeof(json)) > 0 && json[0] != '\0') {
            char state[32] = "";
            json_str_val(json, "state", state, sizeof(state));

            /* publish state on change */
            if (state[0] && strcmp(state, last_state) != 0) {
                mqtt_publish(mqtt_fd, "state", state);
                snprintf(last_state, sizeof(last_state), "%s", state);

                /* on alarm: publish full alarm detail */
                if (!strcmp(state, "ALARM") || !strcmp(state, "WARNING")
                    || !strcmp(state, "FAULT")) {
                    char reason[128] = "";
                    json_str_val(json, "alarm_reason", reason, sizeof(reason));
                    char alarm_json[512];
                    snprintf(alarm_json, sizeof(alarm_json),
                             "{\"state\":\"%s\",\"reason\":\"%s\"}",
                             state, reason[0] ? reason : "unknown");
                    mqtt_publish(mqtt_fd, "alarm", alarm_json);
                }
            }

            /* periodic full status (every ~10 cycles = 10s at 1s poll) */
            status_counter++;
            if (status_counter >= 10) {
                /* compact JSON to single line */
                char compact[BUF_SIZE];
                int wi = 0;
                for (int i = 0; json[i] && wi < (int)sizeof(compact) - 1; i++)
                    if (json[i] != '\n' && json[i] != '\r')
                        compact[wi++] = json[i];
                compact[wi] = '\0';
                mqtt_publish(mqtt_fd, "telemetry", compact);
                status_counter = 0;
            }
        }

        /* keepalive: ping every (keepalive/2) cycles */
        ping_counter++;
        if (ping_counter >= g_keepalive / 2) {
            if (mqtt_ping(mqtt_fd) < 0) {
                printf("[mqttd] ping failed, reconnecting\n");
                close(mqtt_fd);
                mqtt_fd = -1;
                continue;
            }
            ping_counter = 0;
        }

        sleep(1);
    }

    printf("[mqttd] shutting down\n");
    if (mqtt_fd >= 0) close(mqtt_fd);
    return 0;
}
