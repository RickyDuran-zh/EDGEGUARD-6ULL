// face_detect.cpp — ncnn Ultra-Light-Face-Detector wrapper for EdgeGuard-6ULL
//
// This file is ONLY compiled for P2 (full ncnn face detection).
// For P1, link face_detect.c (pure-C stub) instead.
//
// Build requirements:
//   - ncnn cross-compiled for arm-linux-gnueabihf
//   - libjpeg or libjpeg-turbo cross-compiled for arm-linux-gnueabihf
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include <jpeglib.h>

/* ---- libjpeg custom error manager (extends jpeg_error_mgr with setjmp) ---- */
struct face_jpeg_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

static void face_jpeg_error_exit(j_common_ptr cinfo)
{
    struct face_jpeg_error_mgr *myerr = (struct face_jpeg_error_mgr *)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

/* ---- model constants ---- */
#define MODEL_INPUT_W      320
#define MODEL_INPUT_H      240
#define MODEL_INPUT_C      3       // RGB
#define FACE_CONF_THRESH   0.70f
#define NMS_IOU_THRESH     0.40f

/* ---- face recognition constants (MobileFaceNet) ---- */
#define FACE_RECOG_INPUT_W  112
#define FACE_RECOG_INPUT_H   96
#define FACE_EMBEDDING_DIM  128
#define FACE_MATCH_THRESH    0.55f
#define FACE_DB_PATH        "/etc/edgeguard/face_db.json"
#define FACE_DB_TMP         "/etc/edgeguard/face_db.json.tmp"
#define MAX_REG_USERS        10

/* registered user entry */
struct face_db_entry {
    char name[64];
    float embedding[FACE_EMBEDDING_DIM];
};

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

/* ---- global state (detection) ---- */
static ncnn::Net *g_net = nullptr;
static int      g_initialized = 0;

/* Pre-computed prior boxes: [cx, cy, w, h] in [0,1]×[0,1] normalised coords */
static float    g_priors[NUM_PRIORS][4];

/* ---- global state (recognition) ---- */
static ncnn::Net *g_recog_net = nullptr;
static int        g_recog_initialized = 0;

/* face database (loaded from /etc/edgeguard/face_db.json) */
static struct face_db_entry g_face_db[MAX_REG_USERS];
static int   g_face_db_count = 0;

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

    /* 1. Decode JPEG → RGB (libjpeg) */
    int im_w, im_h, channels;
    struct jpeg_decompress_struct cinfo;
    struct face_jpeg_error_mgr jerr;
    unsigned char *rgb = NULL;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = face_jpeg_error_exit;

    /* setjmp for libjpeg error handling — longjmp here on fatal decode error */
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        if (rgb) free(rgb);
        fprintf(stderr, "[face_detect] JPEG decode failed  len=%d\n", len);
        return -1;
    }

    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, jpeg_data, len);
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        fprintf(stderr, "[face_detect] JPEG header decode failed  len=%d\n", len);
        return -1;
    }

    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    im_w    = cinfo.output_width;
    im_h    = cinfo.output_height;
    channels = cinfo.output_components;

    rgb = (unsigned char *)malloc(im_w * im_h * channels);
    if (!rgb) {
        jpeg_destroy_decompress(&cinfo);
        fprintf(stderr, "[face_detect] malloc failed  %dx%dx%d\n",
                im_w, im_h, channels);
        return -1;
    }

    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned char *row = rgb + cinfo.output_scanline * im_w * channels;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    /* diagnostic: raw RGB pixel values */
    printf("[face_detect] raw_rgb[0..5]=(%d,%d,%d) (%d,%d,%d)  "
           "mid=(%d,%d,%d)  corner=(%d,%d,%d)\n",
           rgb[0], rgb[1], rgb[2], rgb[3], rgb[4], rgb[5],
           rgb[im_w*im_h/2*3], rgb[im_w*im_h/2*3+1], rgb[im_w*im_h/2*3+2],
           rgb[(im_w*im_h-1)*3], rgb[(im_w*im_h-1)*3+1], rgb[(im_w*im_h-1)*3+2]);

    /* 2. Resize to model input 320×240 */
    ncnn::Mat in = ncnn::Mat::from_pixels(rgb, ncnn::Mat::PIXEL_RGB, im_w, im_h);
    free(rgb);

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

/* ---- face database helpers ---- */
static int load_face_db(void)
{
    g_face_db_count = 0;

    FILE *fp = fopen(FACE_DB_PATH, "r");
    if (!fp) return 0;  /* no DB yet, not an error */

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0 || sz > 1048576) { fclose(fp); return 0; }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(fp); return -1; }
    fread(buf, 1, sz, fp);
    buf[sz] = '\0';
    fclose(fp);

    const char *p = buf;
    while (g_face_db_count < MAX_REG_USERS) {
        p = strstr(p, "\"name\"");
        if (!p) break;
        p = strchr(p, ':');
        if (!p) break;
        p++; while (*p == ' ' || *p == '"') p++;
        int i = 0;
        while (*p && *p != '"' && i < 63)
            g_face_db[g_face_db_count].name[i++] = *p++;
        g_face_db[g_face_db_count].name[i] = '\0';

        p = strstr(p, "\"embedding\"");
        if (!p) break;
        p = strchr(p, '[');
        if (!p) break;
        p++;
        for (int j = 0; j < FACE_EMBEDDING_DIM; j++) {
            while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r') p++;
            char *end;
            g_face_db[g_face_db_count].embedding[j] = strtof(p, &end);
            if (end == p) break;  /* parse error */
            p = end;
        }
        g_face_db_count++;
    }

    free(buf);
    printf("[face_detect] loaded %d registered user(s) from %s\n",
           g_face_db_count, FACE_DB_PATH);
    return 0;
}

static int save_face_db(void)
{
    FILE *fp = fopen(FACE_DB_TMP, "w");
    if (!fp) return -1;

    fprintf(fp, "{\n  \"users\": [\n");
    for (int i = 0; i < g_face_db_count; i++) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", g_face_db[i].name);
        fprintf(fp, "      \"embedding\": [");
        for (int j = 0; j < FACE_EMBEDDING_DIM; j++) {
            fprintf(fp, "%.6f", g_face_db[i].embedding[j]);
            if (j < FACE_EMBEDDING_DIM - 1) fprintf(fp, ", ");
            if (j % 8 == 7) fprintf(fp, "\n        ");
        }
        fprintf(fp, "]\n");
        fprintf(fp, "    }%s\n", (i < g_face_db_count - 1) ? "," : "");
    }
    fprintf(fp, "  ]\n}\n");
    fclose(fp);
    rename(FACE_DB_TMP, FACE_DB_PATH);
    return 0;
}

static float cosine_similarity(const float *a, const float *b)
{
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (int i = 0; i < FACE_EMBEDDING_DIM; i++) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    if (na <= 0.0f || nb <= 0.0f) return 0.0f;
    return dot / (sqrtf(na) * sqrtf(nb));
}

/* ---- face alignment: crop + resize to 112×96 ---- */
static int face_align(const unsigned char *rgb, int im_w, int im_h,
                       float *bbox, ncnn::Mat &out)
{
    float bw = bbox[2] - bbox[0];
    float bh = bbox[3] - bbox[1];
    if (bw < 8.0f || bh < 8.0f) return -1;

    float cx = (bbox[0] + bbox[2]) * 0.5f;
    float cy = (bbox[1] + bbox[3]) * 0.5f;
    float half = fmaxf(bw, bh) * 0.55f;  /* square with margin */

    int x1 = (int)(cx - half);
    int y1 = (int)(cy - half);
    int x2 = (int)(cx + half);
    int y2 = (int)(cy + half);

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > im_w) x2 = im_w;
    if (y2 > im_h) y2 = im_h;

    int cw = x2 - x1;
    int ch = y2 - y1;
    if (cw < 4 || ch < 4) return -1;

    unsigned char *crop = (unsigned char *)malloc(cw * ch * 3);
    if (!crop) return -1;

    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            int src = (y * im_w + x) * 3;
            int dst = ((y - y1) * cw + (x - x1)) * 3;
            crop[dst]     = rgb[src];
            crop[dst + 1] = rgb[src + 1];
            crop[dst + 2] = rgb[src + 2];
        }
    }

    ncnn::Mat crop_mat = ncnn::Mat::from_pixels(crop, ncnn::Mat::PIXEL_RGB, cw, ch);
    free(crop);

    ncnn::resize_bilinear(crop_mat, out, FACE_RECOG_INPUT_W, FACE_RECOG_INPUT_H);

    /* normalization is built into the model (minusscalar0 + mulscalar0 layers),
       so we pass raw [0,255] RGB directly and let the model handle it */
    return 0;
}

/* ---- extract 128-d embedding via MobileFaceNet ----
   The model's first two layers are BinaryOp _minusscalar0 (sub 127.5)
   and BinaryOp _mulscalar0 (mul 0.007812).  ncnn's BinaryOp forward
   returns -100 on this build, so we normalise manually and feed the
   result directly to blob "_mulscalar0" (which is the input of the
   first convolution). */
static int extract_embedding(const ncnn::Mat &aligned_face, float *embedding)
{
    if (!g_recog_net) return -1;

    /* Manual normalisation: (pixel - 127.5) * 0.007812
       aligned_face is [0,255] RGB, we need float32 [-0.992, 0.996] */
    int total_pixels = aligned_face.w * aligned_face.h * aligned_face.c;
    ncnn::Mat normalized;
    normalized.create(aligned_face.w, aligned_face.h, aligned_face.c, 4u);
    float *dst = normalized;
    for (int i = 0; i < total_pixels; i++)
        dst[i] = (aligned_face[i] - 127.5f) * 0.007812f;

    fprintf(stderr, "[face_detect] recog norm: w=%d h=%d c=%d elemsize=%zu  "
            "data[0..2]=%.4f %.4f %.4f\n",
            normalized.w, normalized.h, normalized.c, normalized.elemsize,
            dst[0], dst[1], dst[2]);

    ncnn::Extractor ex = g_recog_net->create_extractor();

    /* Feed into _mulscalar0 — the blob AFTER both BinaryOp normalisation
       layers, just before the first Convolution */
    int in_ret = ex.input("_mulscalar0", normalized);
    if (in_ret != 0) {
        fprintf(stderr, "[face_detect] ex.input('_mulscalar0') FAILED ret=%d\n",
                in_ret);
        return -1;
    }

    ncnn::Mat out;
    int ext_ret = ex.extract("fc1", out, 0);
    if (ext_ret != 0) {
        /* try type=1 fallback */
        ext_ret = ex.extract("fc1", out, 1);
    }
    if (ext_ret != 0) {
        fprintf(stderr, "[face_detect] ex.extract('fc1') FAILED ret=%d\n",
                ext_ret);
        return -1;
    }

    fprintf(stderr, "[face_detect] recog output: w=%d h=%d c=%d elemsize=%zu\n",
            out.w, out.h, out.c, out.elemsize);

    int dim = out.w * out.h * out.c;
    if (dim > FACE_EMBEDDING_DIM) dim = FACE_EMBEDDING_DIM;

    /* L2-normalise */
    float norm = 0.0f;
    for (int i = 0; i < dim; i++)
        norm += out[i] * out[i];
    norm = sqrtf(norm);
    if (norm < 1e-6f) {
        fprintf(stderr, "[face_detect] embedding norm too small: %.6f\n", norm);
        return -1;
    }

    for (int i = 0; i < dim; i++)
        embedding[i] = out[i] / norm;
    for (int i = dim; i < FACE_EMBEDDING_DIM; i++)
        embedding[i] = 0.0f;

    return 0;
}

/* ---- face_recog_init: load MobileFaceNet model ---- */
int face_recog_init(const char *model_path)
{
    if (g_recog_initialized) return 0;

    if (!model_path) return -1;

    char param[512], bin[512];
    snprintf(param, sizeof(param), "%s/mobilefacenet.param", model_path);
    snprintf(bin,   sizeof(bin),   "%s/mobilefacenet.bin",   model_path);

    if (fopen(param, "rb") == NULL || fopen(bin, "rb") == NULL) {
        fprintf(stderr, "[face_detect] MobileFaceNet model not found in %s\n",
                model_path);
        return -1;
    }

    g_recog_net = new ncnn::Net();
    g_recog_net->opt.use_vulkan_compute = false;

    if (g_recog_net->load_param(param) != 0 ||
        g_recog_net->load_model(bin)   != 0) {
        fprintf(stderr, "[face_detect] MobileFaceNet load failed\n");
        delete g_recog_net;
        g_recog_net = nullptr;
        return -1;
    }

    g_recog_initialized = 1;

    /* Dump blob names so we can confirm input/output layer names.
       If this fails to compile (ncnn API too old), delete this block
       and use: strings /etc/edgeguard/models/mobilefacenet.bin | head -50 */
    {
        const std::vector<const char *> &in_names  = g_recog_net->input_names();
        const std::vector<const char *> &out_names = g_recog_net->output_names();
        printf("[face_detect] recog input names (%zu): ", in_names.size());
        for (size_t i = 0; i < in_names.size(); i++)
            printf("'%s' ", in_names[i]);
        printf("\n[face_detect] recog output names (%zu): ", out_names.size());
        for (size_t i = 0; i < out_names.size(); i++)
            printf("'%s' ", out_names[i]);
        printf("\n");
    }

    /* load existing face DB */
    load_face_db();

    printf("[face_detect] face recognition ready  model=%s\n", model_path);
    return 0;
}

/* ---- face_verify_run: detect → extract → compare → match ---- */
int face_verify_run(const uint8_t *jpeg_data, int len,
                    char *matched_user, int user_buf_size,
                    float *confidence)
{
    if (!matched_user || !confidence) return -1;
    matched_user[0] = '\0';
    *confidence = 0.0f;

    if (!g_initialized || !g_net || !g_recog_initialized || !g_recog_net)
        return 0;

    /* 1. Decode JPEG → RGB (libjpeg) */
    int im_w, im_h, channels;
    struct jpeg_decompress_struct cinfo;
    struct face_jpeg_error_mgr jerr;
    unsigned char *rgb = NULL;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = face_jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        if (rgb) free(rgb);
        return -1;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpeg_data, len);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    im_w    = cinfo.output_width;
    im_h    = cinfo.output_height;
    channels = cinfo.output_components;

    rgb = (unsigned char *)malloc(im_w * im_h * channels);
    if (!rgb) { jpeg_destroy_decompress(&cinfo); return -1; }

    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned char *row = rgb + cinfo.output_scanline * im_w * channels;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    /* 2. Face detection (ultra_face) — same pipeline as face_detect_run */
    ncnn::Mat in = ncnn::Mat::from_pixels(rgb, ncnn::Mat::PIXEL_RGB, im_w, im_h);
    ncnn::Mat in_resized;
    ncnn::resize_bilinear(in, in_resized, MODEL_INPUT_W, MODEL_INPUT_H);

    const float mean_vals[3] = { 127.5f, 127.5f, 127.5f };
    const float norm_vals[3] = { 1.0f / 127.5f, 1.0f / 127.5f, 1.0f / 127.5f };
    in_resized.substract_mean_normalize(mean_vals, norm_vals);

    ncnn::Extractor dex = g_net->create_extractor();
    dex.input("input", in_resized);

    ncnn::Mat scores, boxes;
    if (dex.extract("scores", scores) != 0 || dex.extract("boxes", boxes) != 0) {
        free(rgb);
        return -1;
    }

    int num_boxes = scores.h;
    float best_bbox[4] = {0, 0, 0, 0};
    float best_score  = 0.0f;

    /* SSD decode + find best face box */
    for (int i = 0; i < num_boxes; i++) {
        float face_conf = scores.row(i)[1];
        if (face_conf < FACE_CONF_THRESH) continue;

        const float *off = boxes.row(i);
        const float *pr  = g_priors[i];

        float dcx = pr[0] + prior_variance[0] * off[0] * pr[2];
        float dcy = pr[1] + prior_variance[0] * off[1] * pr[3];
        float dw  = pr[2] * expf(prior_variance[1] * off[2]);
        float dh  = pr[3] * expf(prior_variance[1] * off[3]);

        /* scale to original image dimensions */
        float x1 = (dcx - dw * 0.5f) * (float)im_w;
        float y1 = (dcy - dh * 0.5f) * (float)im_h;
        float x2 = (dcx + dw * 0.5f) * (float)im_w;
        float y2 = (dcy + dh * 0.5f) * (float)im_h;

        if (x1 < 0.0f) x1 = 0.0f;
        if (y1 < 0.0f) y1 = 0.0f;
        if (x2 > (float)im_w) x2 = (float)im_w;
        if (y2 > (float)im_h) y2 = (float)im_h;
        if (x2 <= x1 || y2 <= y1) continue;

        if (face_conf > best_score) {
            best_score = face_conf;
            best_bbox[0] = x1; best_bbox[1] = y1;
            best_bbox[2] = x2; best_bbox[3] = y2;
        }
    }

    if (best_score < FACE_CONF_THRESH) {
        free(rgb);
        return 0;  /* no face detected */
    }

    /* 3. Face alignment: crop + resize to 112×96 */
    ncnn::Mat aligned;
    if (face_align(rgb, im_w, im_h, best_bbox, aligned) != 0) {
        free(rgb);
        return -1;
    }
    free(rgb);  /* RGB no longer needed */

    /* 4. Extract 128-d embedding */
    float embedding[FACE_EMBEDDING_DIM];
    if (extract_embedding(aligned, embedding) != 0)
        return -1;

    /* 5. Compare against face database */
    float best_match = 0.0f;
    int   best_idx   = -1;
    for (int i = 0; i < g_face_db_count; i++) {
        float sim = cosine_similarity(embedding, g_face_db[i].embedding);
        if (sim > best_match) {
            best_match = sim;
            best_idx   = i;
        }
    }

    if (best_idx >= 0 && best_match >= FACE_MATCH_THRESH) {
        snprintf(matched_user, user_buf_size, "%s", g_face_db[best_idx].name);
        *confidence = best_match;
        printf("[face_detect] face_verify MATCHED  user=%s  score=%.4f\n",
               matched_user, best_match);
    } else {
        printf("[face_detect] face_verify NO MATCH  best_score=%.4f  "
               "threshold=%.2f  db_count=%d\n",
               best_match, FACE_MATCH_THRESH, g_face_db_count);
    }

    return 0;
}

/* ---- face_register_user: register a new face ---- */
int face_register_user(const char *jpeg_path, const char *username)
{
    if (!g_initialized || !g_recog_initialized)
        return -1;
    if (!jpeg_path || !username) return -1;
    if (g_face_db_count >= MAX_REG_USERS) {
        fprintf(stderr, "[face_detect] face DB full (%d users)\n",
                MAX_REG_USERS);
        return -1;
    }

    /* Read JPEG from disk */
    FILE *fp = fopen(jpeg_path, "rb");
    if (!fp) { perror(jpeg_path); return -1; }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc(len);
    if (!data) { fclose(fp); return -1; }
    fread(data, 1, len, fp);
    fclose(fp);

    /* Re-use face_verify_run's detection+extraction pipeline.
       We detect the face, then extract embedding directly. */
    int im_w, im_h, channels;
    struct jpeg_decompress_struct cinfo;
    struct face_jpeg_error_mgr jerr;
    unsigned char *rgb = NULL;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = face_jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        if (rgb) free(rgb);
        free(data);
        return -1;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, len);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        free(data);
        return -1;
    }
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    im_w    = cinfo.output_width;
    im_h    = cinfo.output_height;
    channels = cinfo.output_components;

    rgb = (unsigned char *)malloc(im_w * im_h * channels);
    if (!rgb) {
        jpeg_destroy_decompress(&cinfo);
        free(data);
        return -1;
    }
    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned char *row = rgb + cinfo.output_scanline * im_w * channels;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free(data);  /* safe to free after decompress is done */

    /* Face detection */
    ncnn::Mat in = ncnn::Mat::from_pixels(rgb, ncnn::Mat::PIXEL_RGB, im_w, im_h);
    ncnn::Mat in_resized;
    ncnn::resize_bilinear(in, in_resized, MODEL_INPUT_W, MODEL_INPUT_H);

    const float mv[3] = { 127.5f, 127.5f, 127.5f };
    const float nv[3] = { 1.0f / 127.5f, 1.0f / 127.5f, 1.0f / 127.5f };
    in_resized.substract_mean_normalize(mv, nv);

    ncnn::Extractor dex = g_net->create_extractor();
    dex.input("input", in_resized);
    ncnn::Mat scores, boxes;
    if (dex.extract("scores", scores) != 0 ||
        dex.extract("boxes", boxes) != 0) {
        free(rgb);
        return -1;
    }

    float best_bbox[4] = {0, 0, 0, 0};
    float best_score  = 0.0f;
    int num_boxes = scores.h;

    for (int i = 0; i < num_boxes; i++) {
        float fc = scores.row(i)[1];
        if (fc < FACE_CONF_THRESH) continue;
        const float *off = boxes.row(i);
        const float *pr  = g_priors[i];
        float dcx = pr[0] + prior_variance[0] * off[0] * pr[2];
        float dcy = pr[1] + prior_variance[0] * off[1] * pr[3];
        float dw  = pr[2] * expf(prior_variance[1] * off[2]);
        float dh  = pr[3] * expf(prior_variance[1] * off[3]);
        float x1  = (dcx - dw * 0.5f) * (float)im_w;
        float y1  = (dcy - dh * 0.5f) * (float)im_h;
        float x2  = (dcx + dw * 0.5f) * (float)im_w;
        float y2  = (dcy + dh * 0.5f) * (float)im_h;
        if (x1 < 0.0f) x1 = 0.0f;
        if (y1 < 0.0f) y1 = 0.0f;
        if (x2 > (float)im_w) x2 = (float)im_w;
        if (y2 > (float)im_h) y2 = (float)im_h;
        if (x2 <= x1 || y2 <= y1) continue;
        if (fc > best_score) {
            best_score = fc;
            best_bbox[0] = x1; best_bbox[1] = y1;
            best_bbox[2] = x2; best_bbox[3] = y2;
        }
    }

    if (best_score < FACE_CONF_THRESH) {
        fprintf(stderr, "[face_detect] register: no face detected in %s\n",
                jpeg_path);
        free(rgb);
        return -1;
    }

    /* Face alignment */
    ncnn::Mat aligned;
    if (face_align(rgb, im_w, im_h, best_bbox, aligned) != 0) {
        free(rgb);
        return -1;
    }
    free(rgb);

    /* Extract embedding */
    float embedding[FACE_EMBEDDING_DIM];
    if (extract_embedding(aligned, embedding) != 0) return -1;

    /* Check duplicate */
    for (int i = 0; i < g_face_db_count; i++) {
        float sim = cosine_similarity(embedding, g_face_db[i].embedding);
        if (sim > 0.85f) {
            fprintf(stderr, "[face_detect] face already registered as '%s' "
                    "(similarity=%.3f)\n", g_face_db[i].name, sim);
            return -1;
        }
    }

    /* Add to DB */
    snprintf(g_face_db[g_face_db_count].name,
             sizeof(g_face_db[0].name), "%s", username);
    memcpy(g_face_db[g_face_db_count].embedding, embedding,
           sizeof(embedding));
    g_face_db_count++;

    if (save_face_db() != 0) return -1;

    printf("[face_detect] registered user '%s' (%d total)\n",
           username, g_face_db_count);
    return 0;
}

void face_detect_deinit(void)
{
    if (g_net) { delete g_net; g_net = nullptr; }
    g_initialized = 0;
    if (g_recog_net) { delete g_recog_net; g_recog_net = nullptr; }
    g_recog_initialized = 0;
}

#endif /* EDGEGUARD_USE_NCNN */
