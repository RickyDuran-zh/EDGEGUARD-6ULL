// face_register.c — EdgeGuard face enrollment CLI tool
// Usage: ./face_register <username> <jpeg_path>
//
// The JPEG must contain a single, clear, front-facing face.
// The tool detects the face, extracts a 128-d embedding via MobileFaceNet,
// and saves it to /etc/edgeguard/face_db.json.
//
// Build:  make face_register NCNN_DIR=~/ncnn JPEG_DIR=/opt/libjpeg-arm
// Deploy: copy face_register to /imx6ull/app/ on the board

#include "face_detect.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <username> <jpeg_path>\n", argv[0]);
        fprintf(stderr, "Example: %s rickyduran /tmp/myface.jpg\n", argv[0]);
        return 1;
    }

    const char *username  = argv[1];
    const char *jpeg_path = argv[2];

    /* 1. Load detection model */
    if (face_detect_init("/etc/edgeguard/models") != 0) {
        fprintf(stderr, "ERROR: face_detect_init failed — "
                "check /etc/edgeguard/models/ultra_face.param\n");
        return 1;
    }

    /* 2. Load recognition model */
    if (face_recog_init("/etc/edgeguard/models") != 0) {
        fprintf(stderr, "ERROR: face_recog_init failed — "
                "check /etc/edgeguard/models/mobilefacenet.param\n");
        return 1;
    }

    /* 3. Register */
    printf("[face_register] enrolling '%s' from %s ...\n", username, jpeg_path);

    if (face_register_user(jpeg_path, username) != 0) {
        fprintf(stderr, "ERROR: face_register_user failed — "
                "make sure the JPEG contains one clear face\n");
        face_detect_deinit();
        return 1;
    }

    printf("[face_register] SUCCESS — '%s' enrolled\n", username);
    printf("[face_register] database: /etc/edgeguard/face_db.json\n");

    face_detect_deinit();
    return 0;
}
