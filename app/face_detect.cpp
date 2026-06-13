// face_detect.cpp — ncnn Ultra-Light-Face-Detector wrapper for EdgeGuard-6ULL
//
// This file is ONLY compiled for P2 (full ncnn face detection).
// For P1, link face_detect.c (pure-C stub) instead.
//
// Build requirements:
//   - ncnn cross-compiled for arm-linux-gnueabihf
//   - stb_image.h (single-header JPEG decoder) in include path
//   - arm-linux-gnueabihf-g++
//
// Usage:
//   make edgeguard_visiond_face NCNN_DIR=/path/to/ncnn STB_DIR=/path/to/stb
//
// Model: Ultra-Light-Fast-Generic-Face-Detector-1MB
//   Input:  320×240 RGB, normalized to [-1, 1]
//   Output: boxes (N×4) + scores (N×2) → NMS → face_count
//   Files:  ultra_face.param (~5KB) + ultra_face.bin (~300KB int8)

#ifdef EDGEGUARD_USE_NCNN

#include "face_detect.h"

#include "net.h"          // ncnn
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"    // public-domain single-header JPEG decoder

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- model constants ---- */
#define MODEL_INPUT_W      320
#define MODEL_INPUT_H      240
#define MODEL_INPUT_C      3       // RGB
#define FACE_CONF_THRESH   0.7f
#define NMS_IOU_THRESH     0.4f

static ncnn::Net *g_net = nullptr;
static int g_initialized = 0;

int face_detect_init(const char *model_path)
{
    if (g_initialized) return 0;

    if (!model_path) {
        fprintf(stderr, "[face_detect] model_path is NULL\n");
        return -1;
    }

    char param_path[512], bin_path[512];
    snprintf(param_path, sizeof(param_path), "%s/ultra_face.param", model_path);
    snprintf(bin_path,   sizeof(bin_path),   "%s/ultra_face.bin",   model_path);

    FILE *fp = fopen(param_path, "rb");
    if (!fp) {
        fprintf(stderr, "[face_detect] model not found: %s\n", param_path);
        return -1;
    }
    fclose(fp);

    fp = fopen(bin_path, "rb");
    if (!fp) {
        fprintf(stderr, "[face_detect] model not found: %s\n", bin_path);
        return -1;
    }
    fclose(fp);

    g_net = new ncnn::Net();
    g_net->opt.use_vulkan_compute = false;

    if (g_net->load_param(param_path) != 0) {
        fprintf(stderr, "[face_detect] load_param failed\n");
        delete g_net; g_net = nullptr;
        return -1;
    }
    if (g_net->load_model(bin_path) != 0) {
        fprintf(stderr, "[face_detect] load_model failed\n");
        delete g_net; g_net = nullptr;
        return -1;
    }

    g_initialized = 1;
    printf("[face_detect] initialized  model=%s  input=%dx%dx%d\n",
           model_path, MODEL_INPUT_W, MODEL_INPUT_H, MODEL_INPUT_C);
    return 0;
}

static void nms(float *boxes, float *scores, int n, float iou_thresh)
{
    for (int i = 0; i < n; i++) {
        if (scores[i] < FACE_CONF_THRESH) continue;
        float *bi = boxes + i * 4;
        float ai = (bi[2] - bi[0]) * (bi[3] - bi[1]);
        if (ai <= 0.0f) continue;

        for (int j = i + 1; j < n; j++) {
            if (scores[j] < FACE_CONF_THRESH) continue;
            float *bj = boxes + j * 4;

            float x1 = bi[0] > bj[0] ? bi[0] : bj[0];
            float y1 = bi[1] > bj[1] ? bi[1] : bj[1];
            float x2 = bi[2] < bj[2] ? bi[2] : bj[2];
            float y2 = bi[3] < bj[3] ? bi[3] : bj[3];

            float ow = x2 - x1;
            float oh = y2 - y1;
            if (ow <= 0.0f || oh <= 0.0f) continue;

            float inter = ow * oh;
            float aj = (bj[2] - bj[0]) * (bj[3] - bj[1]);
            float iou = inter / (ai + aj - inter);
            if (iou > iou_thresh)
                scores[j] = 0.0f;
        }
    }
}

int face_detect_run(const uint8_t *jpeg_data, int len, int *face_count)
{
    if (!face_count) return -1;
    *face_count = 0;

    if (!g_initialized || !g_net) return 0;

    /* Decode JPEG to RGB */
    int w, h, channels;
    unsigned char *rgb = stbi_load_from_memory(jpeg_data, len,
                                                &w, &h, &channels, 3);
    if (!rgb) {
        fprintf(stderr, "[face_detect] JPEG decode failed\n");
        return -1;
    }

    /* Resize + normalize */
    ncnn::Mat in = ncnn::Mat::from_pixels_resize(
        rgb, ncnn::Mat::PIXEL_RGB, w, h, MODEL_INPUT_W, MODEL_INPUT_H);
    stbi_image_free(rgb);
    in.substract_mean_normalize(127.5f, 0.007843f);

    /* Inference */
    ncnn::Extractor ex = g_net->create_extractor();
    ex.input("input", in);

    ncnn::Mat scores, boxes;
    ex.extract("scores", scores);
    ex.extract("boxes",  boxes);

    int num_boxes = scores.h;
    if (num_boxes <= 0 || boxes.h != num_boxes) return 0;

    /* Parse + NMS */
    float face_scores[100];
    float face_boxes[400];  // 100 × 4
    int valid = 0;

    for (int i = 0; i < num_boxes && valid < 100; i++) {
        float conf = scores.row(i)[1];
        if (conf < FACE_CONF_THRESH) continue;

        float cx = boxes.row(i)[0] * MODEL_INPUT_W;
        float cy = boxes.row(i)[1] * MODEL_INPUT_H;
        float bw = boxes.row(i)[2] * MODEL_INPUT_W;
        float bh = boxes.row(i)[3] * MODEL_INPUT_H;

        face_boxes[valid * 4 + 0] = cx - bw * 0.5f;
        face_boxes[valid * 4 + 1] = cy - bh * 0.5f;
        face_boxes[valid * 4 + 2] = cx + bw * 0.5f;
        face_boxes[valid * 4 + 3] = cy + bh * 0.5f;
        face_scores[valid] = conf;
        valid++;
    }

    nms(face_boxes, face_scores, valid, NMS_IOU_THRESH);

    int cnt = 0;
    for (int i = 0; i < valid; i++)
        if (face_scores[i] >= FACE_CONF_THRESH)
            cnt++;
    *face_count = cnt;

    return 0;
}

void face_detect_deinit(void)
{
    if (g_net) { delete g_net; g_net = nullptr; }
    g_initialized = 0;
}

#endif /* EDGEGUARD_USE_NCNN */
