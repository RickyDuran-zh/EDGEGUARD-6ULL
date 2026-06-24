// face_detect.h — Face detection + recognition C/C++ bridge for EdgeGuard-6ULL
// Wraps ncnn models behind a pure-C interface so that C daemons (edgeguard_visiond)
// can call them without linking C++ themselves.
//
// Two build modes:
//   Stub  — compile with gcc, no ncnn dependency, always returns face_count=0
//   Full  — compile with -DEDGEGUARD_USE_NCNN, links ncnn + libjpeg
//
// Models (all in /etc/edgeguard/models/):
//   ultra_face.param / .bin       — face detection (320×240, ~300KB int8)
//   mobilefacenet.param / .bin    — face recognition (112×96, ~5MB fp16)
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
 * Face verification — detects a face and reports a match for login.
 * jpeg_data / len   — raw MJPEG frame from V4L2 capture
 * matched_user       — [out] username buffer (at least 64 bytes)
 * user_buf_size      — size of matched_user buffer
 * confidence         — [out] match confidence 0.0–1.0
 *
 * Returns 0 on success (check matched_user[0] — non-empty = matched).
 *
 * Transition mode (P2): any detected face → matched_user="detected".
 * Full mode (P3): MobileFaceNet feature extraction + cosine similarity
 * against registered face database.
 *
 * In stub mode always returns matched_user="" and confidence=0.0.
 */
int face_verify_run(const uint8_t *jpeg_data, int len,
                    char *matched_user, int user_buf_size,
                    float *confidence);

/*
 * Initialize face recognition engine.
 * model_path — directory containing mobilefacenet.param + mobilefacenet.bin
 * Returns 0 on success, -1 if model files cannot be loaded.
 *
 * In stub mode always returns -1.
 */
int face_recog_init(const char *model_path);

/*
 * Register a user face into the local database.
 * jpeg_path — path to a JPEG image file containing exactly one face
 * username   — user name to register (max 63 chars)
 *
 * Returns 0 on success, -1 on error.
 *
 * Workflow: decode JPEG → detect face → crop & align → extract 128-d
 * embedding → save to /etc/edgeguard/face_db.json.
 *
 * In stub mode always returns -1.
 */
int face_register_user(const char *jpeg_path, const char *username);

/*
 * Release all resources. Safe to call even if init was never called.
 */
void face_detect_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* FACE_DETECT_H */
