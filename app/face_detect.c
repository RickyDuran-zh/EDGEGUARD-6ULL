// face_detect.c — Pure-C stub for EdgeGuard-6ULL
// Provides the face_detect API surface without any AI dependency.
// Always returns face_count=0 so that edgeguard_visiond compiles and
// runs with only a C compiler (no g++ required).
//
// When ncnn is ready (P2), link face_detect.cpp instead, which contains
// the real Ultra-Light-Face-Detector inference under -DEDGEGUARD_USE_NCNN.

#include "face_detect.h"

#include <stdio.h>

int face_detect_init(const char *model_path)
{
    (void)model_path;
    fprintf(stderr, "[face_detect] STUB mode — ncnn not linked, face_count "
            "will always be 0\n");
    fprintf(stderr, "[face_detect] To enable: cross-compile ncnn, then "
            "build with 'make edgeguard_visiond_face'\n");
    return -1;
}

int face_detect_run(const uint8_t *jpeg_data, int len, int *face_count)
{
    (void)jpeg_data;
    (void)len;
    if (face_count) *face_count = 0;
    return 0;
}

int face_verify_run(const uint8_t *jpeg_data, int len,
                    char *matched_user, int user_buf_size,
                    float *confidence)
{
    (void)jpeg_data;
    (void)len;
    if (matched_user && user_buf_size > 0) matched_user[0] = '\0';
    if (confidence) *confidence = 0.0f;
    return 0;
}

int face_recog_init(const char *model_path)
{
    (void)model_path;
    return -1;  /* stub mode — no AI models */
}

int face_register_user(const char *jpeg_path, const char *username)
{
    (void)jpeg_path;
    (void)username;
    fprintf(stderr, "[face_detect] STUB — face_register_user not available\n");
    return -1;
}

void face_detect_deinit(void)
{
    /* nothing to release in stub mode */
}
