// camera_v4l2.c — Minimal V4L2 MJPEG capture for embedded Linux 4.19
// Standard V4L2 mmap workflow: open → query → set format → request bufs →
// mmap → queue bufs → stream on → dequeue/enqueue → stream off → unmap → close

#include "camera_v4l2.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>

/* ---- internal context ---- */
struct buffer_info {
    void   *start;
    size_t  length;
};

struct camera_ctx {
    int                 fd;
    unsigned int        width;
    unsigned int        height;
    unsigned int        pixelformat;
    char                errmsg[256];

    struct buffer_info *buffers;
    unsigned int        num_buffers;
};

/* ---- helpers ---- */
static void set_error(struct camera_ctx *ctx, const char *fmt, ...)
{
    if (!ctx) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ctx->errmsg, sizeof(ctx->errmsg), fmt, ap);
    va_end(ap);
}

static int xioctl(int fd, unsigned long request, void *arg,
                  struct camera_ctx *ctx, const char *name)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r < 0 && errno == EINTR);

    if (r < 0 && ctx)
        set_error(ctx, "ioctl(%s) failed: %s", name, strerror(errno));
    return r;
}

/* ---- public API ---- */

const char *camera_error(struct camera_ctx *ctx)
{
    return ctx ? ctx->errmsg : "null context";
}

unsigned int camera_pixelformat(struct camera_ctx *ctx)
{
    return ctx ? ctx->pixelformat : 0;
}

struct camera_ctx *camera_open(const char *device,
                               unsigned int width,
                               unsigned int height)
{
    if (!device) device = "/dev/video0";

    struct camera_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->fd = -1;

    /* 1. open device — blocking mode: DQBUF will wait for a frame */
    ctx->fd = open(device, O_RDWR);
    if (ctx->fd < 0) {
        set_error(ctx, "open(%s): %s", device, strerror(errno));
        camera_close(ctx);
        return NULL;
    }

    /* 2. query capabilities */
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (xioctl(ctx->fd, VIDIOC_QUERYCAP, &cap, ctx, "QUERYCAP") < 0) {
        camera_close(ctx);
        return NULL;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        set_error(ctx, "%s: not a video capture device", device);
        camera_close(ctx);
        return NULL;
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        set_error(ctx, "%s: does not support streaming I/O", device);
        camera_close(ctx);
        return NULL;
    }

    /* 3. set format — prefer MJPEG */
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    if (xioctl(ctx->fd, VIDIOC_S_FMT, &fmt, ctx, "S_FMT MJPEG") < 0) {
        /* fallback: let driver pick */
        fmt.fmt.pix.pixelformat = 0;
        if (xioctl(ctx->fd, VIDIOC_S_FMT, &fmt, ctx, "S_FMT any") < 0) {
            camera_close(ctx);
            return NULL;
        }
    }

    ctx->width       = fmt.fmt.pix.width;
    ctx->height      = fmt.fmt.pix.height;
    ctx->pixelformat = fmt.fmt.pix.pixelformat;

    /* 4. request buffers — 4 buffers avoid races on slow USB hosts */
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(ctx->fd, VIDIOC_REQBUFS, &req, ctx, "REQBUFS") < 0) {
        camera_close(ctx);
        return NULL;
    }
    if (req.count < 2) {
        set_error(ctx, "insufficient buffers: got %u, need 2", req.count);
        camera_close(ctx);
        return NULL;
    }
    ctx->num_buffers = req.count;

    ctx->buffers = calloc(ctx->num_buffers, sizeof(struct buffer_info));
    if (!ctx->buffers) {
        set_error(ctx, "out of memory");
        camera_close(ctx);
        return NULL;
    }

    /* 5. mmap each buffer */
    for (unsigned int i = 0; i < ctx->num_buffers; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (xioctl(ctx->fd, VIDIOC_QUERYBUF, &buf, ctx, "QUERYBUF") < 0) {
            camera_close(ctx);
            return NULL;
        }

        ctx->buffers[i].length = buf.length;
        ctx->buffers[i].start  = mmap(NULL, buf.length,
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED, ctx->fd,
                                       buf.m.offset);
        if (ctx->buffers[i].start == MAP_FAILED) {
            set_error(ctx, "mmap buffer %u: %s", i, strerror(errno));
            camera_close(ctx);
            return NULL;
        }
    }

    /* 6. queue all buffers */
    for (unsigned int i = 0; i < ctx->num_buffers; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (xioctl(ctx->fd, VIDIOC_QBUF, &buf, ctx, "QBUF") < 0) {
            camera_close(ctx);
            return NULL;
        }
    }

    /* 7. start stream */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(ctx->fd, VIDIOC_STREAMON, &type, ctx, "STREAMON") < 0) {
        camera_close(ctx);
        return NULL;
    }

    /* 8. flush stale buffers — discard first 2 frames.
       After USB reset or hotplug, mmap buffers may contain old data
       from a previous streaming session.  Skipping the first couple
       of frames guarantees the pipeline starts with live frames. */
    for (int i = 0; i < 2; i++) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(ctx->fd, &fds);
        struct timeval tv = { 1, 0 };  /* 1 s timeout */

        int sel = select(ctx->fd + 1, &fds, NULL, NULL, &tv);
        if (sel <= 0) break;  /* no frame or error → stop flushing */

        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (xioctl(ctx->fd, VIDIOC_DQBUF, &buf, NULL, "DQBUF-flush") < 0)
            break;

        if (xioctl(ctx->fd, VIDIOC_QBUF, &buf, NULL, "QBUF-flush") < 0)
            break;
    }

    return ctx;
}

int camera_capture(struct camera_ctx *ctx, struct camera_frame *frame)
{
    if (!ctx || !frame || ctx->fd < 0) return -1;

    memset(frame, 0, sizeof(*frame));

    /* wait up to 3 s for a frame — avoids hanging forever if the camera
       stalls after STREAMON (e.g. after a hotplug or systemd restart) */
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(ctx->fd, &fds);
    struct timeval tv;
    tv.tv_sec  = 3;
    tv.tv_usec = 0;

    int sel = select(ctx->fd + 1, &fds, NULL, NULL, &tv);
    if (sel < 0) {
        set_error(ctx, "select: %s", strerror(errno));
        return -1;
    }
    if (sel == 0) {
        set_error(ctx, "DQBUF timeout — camera stalled");
        return -1;
    }

    /* dequeue filled buffer */
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(ctx->fd, VIDIOC_DQBUF, &buf, ctx, "DQBUF") < 0)
        return -1;

    if (buf.index >= ctx->num_buffers) {
        set_error(ctx, "bad buffer index %u", buf.index);
        return -1;
    }

    /* copy out */
    size_t used = buf.bytesused;
    if (used == 0) used = buf.length;
    frame->size = used;
    frame->data = malloc(used + 1);  /* +1 for safe NUL */
    if (!frame->data) {
        set_error(ctx, "out of memory");
        return -1;
    }
    memcpy(frame->data, ctx->buffers[buf.index].start, used);
    frame->data[used] = '\0';

    /* requeue */
    if (xioctl(ctx->fd, VIDIOC_QBUF, &buf, ctx, "QBUF") < 0) {
        free(frame->data);
        frame->data = NULL;
        frame->size = 0;
        return -1;
    }

    return 0;
}

void camera_close(struct camera_ctx *ctx)
{
    if (!ctx) return;

    if (ctx->fd >= 0) {
        /* stop streaming */
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(ctx->fd, VIDIOC_STREAMOFF, &type);

        /* unmap buffers */
        if (ctx->buffers) {
            for (unsigned int i = 0; i < ctx->num_buffers; i++) {
                if (ctx->buffers[i].start &&
                    ctx->buffers[i].start != MAP_FAILED)
                    munmap(ctx->buffers[i].start, ctx->buffers[i].length);
            }
            free(ctx->buffers);
        }
        close(ctx->fd);
    }

    free(ctx);
}
