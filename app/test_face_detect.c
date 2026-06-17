// test_face_detect.c — offline test: read JPEG from disk, run face detection
// Build:  make test_face_detect NCNN_DIR=~/ncnn STB_DIR=~/stb
// Usage:  ./test_face_detect /tmp/face_test.jpg

#include "face_detect.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <jpeg_path>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror(path);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    unsigned char *data = malloc(len);
    if (!data) { fclose(fp); return 1; }
    fread(data, 1, len, fp);
    fclose(fp);

    printf("[test] loaded %s  size=%ld bytes  magic=%02X%02X\n",
           path, len, data[0], data[1]);

    face_detect_init("/etc/edgeguard/models");

    int face_count = 0;
    int ret = face_detect_run(data, (int)len, &face_count);

    printf("[test] ret=%d  face_count=%d\n", ret, face_count);

    face_detect_deinit();
    free(data);
    return 0;
}
