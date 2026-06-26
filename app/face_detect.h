// face_detect.h — Face detection C/C++ bridge for EdgeGuard-6ULL
// Wraps ncnn ultra_face model behind a pure-C interface so that C daemons
// (edgeguard_visiond) can call it without linking C++ themselves.
//
// Two build modes:
//   Stub  — compile with gcc, no ncnn dependency, always returns face_count=0
//   Full  — compile with -DEDGEGUARD_USE_NCNN, links ncnn + libjpeg
//
// Model (in /etc/edgeguard/models/):
//   ultra_face.param / .bin       — face detection (320×240, ~300KB int8)
#ifndef FACE_DETECT_H
#define FACE_DETECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize face detection engine.
 * model_path — directory containing ultra_face.param + ultra_face.bin
 * Returns 0 on success, -1 if model files cannot be loaded.
 *
 * In stub mode always returns -1 (harmless — visiond continues without faces).
 */
int face_detect_init(const char *model_path);

/*
 * Run face detection on a JPEG buffer.
 * jpeg_data / len — raw MJPEG frame from V4L2 capture
 * face_count       — [out] number of detected faces (confidence ≥ 0.7)
 *
 * Returns 0 on success (check *face_count), -1 on error.
 * Caller owns all buffers; this function does not modify jpeg_data.
 *
 * In stub mode always sets *face_count = 0 and returns 0.
 */
int face_detect_run(const uint8_t *jpeg_data, int len, int *face_count);

/*
 * Release all resources. Safe to call even if init was never called.
 */
void face_detect_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* FACE_DETECT_H */
