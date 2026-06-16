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
// Model: Ultra-Light-Fast-Generic-Face-Detector-1MB (RFB-320)
//   Input:  320×240 RGB, normalized to [-1, 1]
//   Output: scores [4420×2] + boxes [4420×4] (SSD anchor offsets)
//   Decode: prior boxes + variance → pixel coords → NMS → face_count
//   Files:  ultra_face.param (~5KB) + ultra_face.bin (~300KB int8)

#ifdef EDGEGUARD_USE_NCNN

#include "face_detect.h"

#include "net.h"          // ncnn
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"    // public-domain single-header JPEG decoder

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- model constants ---- */
#define MODEL_INPUT_W      320
#define MODEL_INPUT_H      240
#define MODEL_INPUT_C      3       // RGB
#define FACE_CONF_THRESH   0.70f
#define NMS_IOU_THRESH     0.40f

/* ---- prior-box (anchor) configuration for RFB-320 ---- */
#define NUM_FEATURE_MAPS   4
#define NUM_PRIORS         4420    /* 40×30×3 + 20×15×2 + 10×8×2 + 5×4×3 */

/* Feature-map dimensions for 320×240 input */
static const int fm_w[NUM_FEATURE_MAPS] = { 40, 20, 10,  5 };
static const int fm_h[NUM_FEATURE_MAPS] = { 30, 15,  8,  4 };

/* Anchors-per-cell for each feature-map (must match model training) */
static const int fm_num_anchors[NUM_FEATURE_MAPS] = { 3, 2, 2, 3 };

/* Min sizes in pixels (for 320×240 input) — one per feature-map.
   Max size is the min size of the next layer (or 2× for the last). */
static const float fm_min_sizes[NUM_FEATURE_MAPS] = { 24.0f, 48.0f, 96.0f, 192.0f };

/* SSD decode variance — standard for RFB face detectors */
static const float prior_variance[2] = { 0.1f, 0.2f };

/* ---- global state ---- */
static ncnn::Net *g_net = nullptr;
static int      g_initialized = 0;

/* Pre-computed prior boxes: [cx, cy, w, h] in [0,1]×[0,1] normalised coords */
static float    g_priors[NUM_PRIORS][4];

/* ---- generate prior boxes (called once during init) ---- */
static void generate_priors(void)
{
    int idx = 0;

    for (int m = 0; m < NUM_FEATURE_MAPS; m++) {
        int fw = fm_w[m];
        int fh = fm_h[m];
        int na = fm_num_anchors[m];
        float stride_w = 1.0f / (float)fw;
        float stride_h = 1.0f / (float)fh;

        float s_k  = fm_min_sizes[m];                         // min size
        float s_kp = (m < NUM_FEATURE_MAPS - 1)
                     ? fm_min_sizes[m + 1]                     // max = next min
                     : s_k * 2.0f;                             // last FM: double
        float s_extra = sqrtf(s_k * s_kp);                     // extra square anchor

        for (int y = 0; y < fh; y++) {
            for (int x = 0; x < fw; x++) {
                float cx = ((float)x + 0.5f) * stride_w;
                float cy = ((float)y + 0.5f) * stride_h;

                /* anchor 0: square  s_k */
                float sw = s_k / (float)MODEL_INPUT_W;
                float sh = s_k / (float)MODEL_INPUT_H;
                g_priors[idx][0] = cx;  g_priors[idx][1] = cy;
                g_priors[idx][2] = sw;  g_priors[idx][3] = sh;
                idx++;

                /* anchor 1: aspect-ratio 2.0  (wider) */
                sw = (s_k * 1.414214f) / (float)MODEL_INPUT_W;
                sh = (s_k * 0.707107f) / (float)MODEL_INPUT_H;
                g_priors[idx][0] = cx;  g_priors[idx][1] = cy;
                g_priors[idx][2] = sw;  g_priors[idx][3] = sh;
                idx++;

                /* anchor 2 (only for FM with 3 anchors): extra square  s_extra */
                if (na >= 3) {
                    sw = s_extra / (float)MODEL_INPUT_W;
                    sh = s_extra / (float)MODEL_INPUT_H;
                    g_priors[idx][0] = cx;  g_priors[idx][1] = cy;
                    g_priors[idx][2] = sw;  g_priors[idx][3] = sh;
                    idx++;
                }
            }
        }
    }

    printf("[face_detect] priors generated: %d (expected %d)\n", idx, NUM_PRIORS);
}

/* ---- model init ---- */
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

    /* Pre-compute all 4420 prior boxes once */
    generate_priors();

    g_initialized = 1;
    printf("[face_detect] initialized  model=%s  input=%dx%dx%d  priors=%d\n",
           model_path, MODEL_INPUT_W, MODEL_INPUT_H, MODEL_INPUT_C, NUM_PRIORS);
    return 0;
}

/* ---- NMS (Non-Maximum Suppression) ---- */
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

/* ---- single-frame inference ---- */
int face_detect_run(const uint8_t *jpeg_data, int len, int *face_count)
{
    if (!face_count) return -1;
    *face_count = 0;

    if (!g_initialized || !g_net) return 0;

    /* 1. Decode JPEG → RGB */
    int im_w, im_h, channels;
    unsigned char *rgb = stbi_load_from_memory(jpeg_data, len,
                                                &im_w, &im_h, &channels, 3);
    if (!rgb) {
        fprintf(stderr, "[face_detect] JPEG decode failed  len=%d\n", len);
        return -1;
    }

    /* diagnostic: raw RGB pixel values */
    printf("[face_detect] raw_rgb[0..5]=(%d,%d,%d) (%d,%d,%d)  "
           "mid=(%d,%d,%d)  corner=(%d,%d,%d)\n",
           rgb[0], rgb[1], rgb[2], rgb[3], rgb[4], rgb[5],
           rgb[im_w*im_h/2*3], rgb[im_w*im_h/2*3+1], rgb[im_w*im_h/2*3+2],
           rgb[(im_w*im_h-1)*3], rgb[(im_w*im_h-1)*3+1], rgb[(im_w*im_h-1)*3+2]);

    /* 2. Resize to model input 320×240 */
    ncnn::Mat in = ncnn::Mat::from_pixels(rgb, ncnn::Mat::PIXEL_RGB, im_w, im_h);
    stbi_image_free(rgb);

    ncnn::Mat in_resized;
    ncnn::resize_bilinear(in, in_resized, MODEL_INPUT_W, MODEL_INPUT_H);

    /* 3. Normalise [0,255] → [-1, 1].
       In this ncnn version, substract_mean_normalize converts the Mat
       from uint8 to float32 IN PLACE (returns void). */
    const float mean_vals[3] = { 127.5f, 127.5f, 127.5f };
    const float norm_vals[3] = { 1.0f / 127.5f, 1.0f / 127.5f, 1.0f / 127.5f };
    printf("[face_detect] before norm: elemsize=%zu  c=%d h=%d w=%d\n",
           in_resized.elemsize, in_resized.c, in_resized.h, in_resized.w);
    in_resized.substract_mean_normalize(mean_vals, norm_vals);
    printf("[face_detect] after norm:  elemsize=%zu  "
           "sample[0..4]=%.4f %.4f %.4f %.4f %.4f\n",
           in_resized.elemsize,
           in_resized.channel(0)[0], in_resized.channel(0)[1],
           in_resized.channel(0)[2], in_resized.channel(0)[3],
           in_resized.channel(0)[4]);

    /* 4. ncnn inference */
    ncnn::Extractor ex = g_net->create_extractor();
    ex.input("input", in_resized);

    ncnn::Mat scores, boxes;
    if (ex.extract("scores", scores) != 0 || ex.extract("boxes", boxes) != 0) {
        fprintf(stderr, "[face_detect] extract failed — check model layer names\n");
        return -1;
    }

    int num_boxes = scores.h;   /* should be NUM_PRIORS = 4420 */

    /* 5. Decode SSD offsets → real pixel coordinates using prior boxes */
    float candidates[400];      /* up to 100 faces × 4 coords */
    float cand_scores[100];
    int   valid = 0;

    /* Find global max face-score for diagnostics */
    float max_face_score = 0.0f;
    int   max_face_idx   = -1;

    for (int i = 0; i < num_boxes && valid < 100; i++) {
        float face_conf = scores.row(i)[1];   /* class-1 = face */
        if (face_conf > max_face_score) {
            max_face_score = face_conf;
            max_face_idx   = i;
        }
        if (face_conf < FACE_CONF_THRESH) continue;

        /* SSD decode:  offset → centre-size */
        const float *off = boxes.row(i);       /* [ox, oy, ow, oh] */
        const float *pr  = g_priors[i];        /* [cx, cy, pw, ph] */

        float dcx = pr[0] + prior_variance[0] * off[0] * pr[2];
        float dcy = pr[1] + prior_variance[0] * off[1] * pr[3];
        float dw  = pr[2] * expf(prior_variance[1] * off[2]);
        float dh  = pr[3] * expf(prior_variance[1] * off[3]);

        /* centre-size → corner, scale to 320×240 pixels */
        float x1 = (dcx - dw * 0.5f) * (float)MODEL_INPUT_W;
        float y1 = (dcy - dh * 0.5f) * (float)MODEL_INPUT_H;
        float x2 = (dcx + dw * 0.5f) * (float)MODEL_INPUT_W;
        float y2 = (dcy + dh * 0.5f) * (float)MODEL_INPUT_H;

        /* clip to image bounds */
        if (x1 < 0.0f) x1 = 0.0f;
        if (y1 < 0.0f) y1 = 0.0f;
        if (x2 > (float)MODEL_INPUT_W)  x2 = (float)MODEL_INPUT_W;
        if (y2 > (float)MODEL_INPUT_H)  y2 = (float)MODEL_INPUT_H;

        if (x2 <= x1 || y2 <= y1) continue;

        candidates[valid * 4 + 0] = x1;
        candidates[valid * 4 + 1] = y1;
        candidates[valid * 4 + 2] = x2;
        candidates[valid * 4 + 3] = y2;
        cand_scores[valid] = face_conf;
        valid++;
    }

    printf("[face_detect] boxes=%d  max_face_score=%.4f@[%d]  "
           "above_thresh=%d\n",
           num_boxes, max_face_score, max_face_idx, valid);

    /* 6. NMS */
    nms(candidates, cand_scores, valid, NMS_IOU_THRESH);

    /* 7. Count surviving faces */
    int cnt = 0;
    for (int i = 0; i < valid; i++) {
        if (cand_scores[i] >= FACE_CONF_THRESH) {
            cnt++;
            printf("[face_detect] FACE #%d  score=%.4f  "
                   "box=[%.0f,%.0f,%.0f,%.0f]\n",
                   cnt, cand_scores[i],
                   candidates[i*4],   candidates[i*4+1],
                   candidates[i*4+2], candidates[i*4+3]);
        }
    }
    *face_count = cnt;

    return 0;
}

void face_detect_deinit(void)
{
    if (g_net) { delete g_net; g_net = nullptr; }
    g_initialized = 0;
}

#endif /* EDGEGUARD_USE_NCNN */
