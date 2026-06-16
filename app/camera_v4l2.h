// camera_v4l2.h — Minimal V4L2 MJPEG capture wrapper for embedded Linux
// Compatible with Linux 4.19 + UVC cameras (Logitech B525 / C270 / etc.)
#ifndef CAMERA_V4L2_H
#define CAMERA_V4L2_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct camera_ctx;

/* Caller owns the returned buffer — free with free() after each capture. */
struct camera_frame {
    uint8_t *data;
    size_t   size;
};

/*
 * Open a V4L2 capture device, negotiate MJPEG at the requested resolution.
 * Returns NULL on failure; call camera_error() for the reason.
 */
struct camera_ctx *camera_open(const char *device,
                               unsigned int width,
                               unsigned int height);

/*
 * Capture one frame.  The caller gets a malloc'd JPEG buffer in *frame.
 * frame->data is always NUL-terminated (beyond size) for safety.
 * Returns 0 on success, -1 on error.
 */
int camera_capture(struct camera_ctx *ctx, struct camera_frame *frame);

/* Return the last error string (static storage, never NULL). */
const char *camera_error(struct camera_ctx *ctx);

/* Return the negotiated pixelformat (V4L2_PIX_FMT_*). */
unsigned int camera_pixelformat(struct camera_ctx *ctx);

/* Close the device and free all resources. */
void camera_close(struct camera_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_V4L2_H */
