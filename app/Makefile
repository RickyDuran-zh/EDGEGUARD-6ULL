CFLAGS  = -O2 -Wall
CXXFLAGS = -O2 -Wall -std=c++11
SQLITE_FLAGS = -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION
LDFLAGS = -lpthread -ldl
LDFLAGS_HTTPD = -lpthread -ldl

.PHONY: all copy

all: sensor_hubd edgeguard_httpd edgeguard_mqttd edgeguard_visiond

sensor_hubd: sensor_hubd.c sqlite3.c sqlite3.h
	arm-linux-gnueabihf-gcc $(CFLAGS) $(SQLITE_FLAGS) sensor_hubd.c sqlite3.c -o sensor_hubd $(LDFLAGS)

edgeguard_httpd: edgeguard_httpd.c sqlite3.c sqlite3.h
	arm-linux-gnueabihf-gcc $(CFLAGS) $(SQLITE_FLAGS) edgeguard_httpd.c sqlite3.c -o edgeguard_httpd $(LDFLAGS_HTTPD)

edgeguard_mqttd: edgeguard_mqttd.c
	arm-linux-gnueabihf-gcc $(CFLAGS) edgeguard_mqttd.c -o edgeguard_mqttd

edgeguard_visiond: edgeguard_visiond.o camera_v4l2.o face_detect.o
	arm-linux-gnueabihf-gcc -o edgeguard_visiond edgeguard_visiond.o camera_v4l2.o face_detect.o $(LDFLAGS)

edgeguard_visiond.o: edgeguard_visiond.c camera_v4l2.h face_detect.h
	arm-linux-gnueabihf-gcc $(CFLAGS) -c edgeguard_visiond.c -o edgeguard_visiond.o

camera_v4l2.o: camera_v4l2.c camera_v4l2.h
	arm-linux-gnueabihf-gcc $(CFLAGS) -c camera_v4l2.c -o camera_v4l2.o

face_detect.o: face_detect.c face_detect.h
	arm-linux-gnueabihf-gcc $(CFLAGS) -c face_detect.c -o face_detect.o

# Full P2 build: ncnn-enabled face detection.
# Prerequisites: cross-compile ncnn + libjpeg-turbo + install g++-arm-linux-gnueabihf
#   NCNN_DIR  — path to ncnn source root
#   JPEG_DIR  — path to libjpeg-turbo install prefix (contains include/jpeglib.h + lib/libjpeg.a)
# Usage: make edgeguard_visiond_face NCNN_DIR=/home/user/ncnn JPEG_DIR=/opt/libjpeg-arm
edgeguard_visiond_face: edgeguard_visiond.c camera_v4l2.c face_detect.cpp camera_v4l2.h face_detect.h
	arm-linux-gnueabihf-gcc $(CFLAGS) -c edgeguard_visiond.c -o edgeguard_visiond.o
	arm-linux-gnueabihf-gcc $(CFLAGS) -c camera_v4l2.c -o camera_v4l2.o
	arm-linux-gnueabihf-g++ $(CXXFLAGS) -DEDGEGUARD_USE_NCNN \
	  -I$(NCNN_DIR)/src -I$(NCNN_DIR)/build-arm/src -I$(JPEG_DIR)/include \
	  -c face_detect.cpp -o face_detect_ncnn.o
	arm-linux-gnueabihf-g++ -o edgeguard_visiond_face \
	  edgeguard_visiond.o camera_v4l2.o face_detect_ncnn.o \
	  -L$(NCNN_DIR)/build-arm/src -L$(JPEG_DIR)/lib -lncnn -ljpeg \
	  -lpthread -ldl -lstdc++ -fopenmp

# Face enrollment CLI tool
# Usage: make face_register NCNN_DIR=/home/user/ncnn JPEG_DIR=/opt/libjpeg-arm
face_register: face_register.c face_detect.cpp face_detect.h
	arm-linux-gnueabihf-gcc $(CFLAGS) -c face_register.c -o face_register.o
	arm-linux-gnueabihf-g++ $(CXXFLAGS) -DEDGEGUARD_USE_NCNN \
	  -I$(NCNN_DIR)/src -I$(NCNN_DIR)/build-arm/src -I$(JPEG_DIR)/include \
	  -c face_detect.cpp -o face_detect_ncnn.o
	arm-linux-gnueabihf-g++ -o face_register \
	  face_register.o face_detect_ncnn.o \
	  -L$(NCNN_DIR)/build-arm/src -L$(JPEG_DIR)/lib -lncnn -ljpeg \
	  -lpthread -ldl -lstdc++ -fopenmp

copy:
	sudo cp sensor_hubd /home/rickyduran/sharedir
	sudo cp edgeguard_httpd /home/rickyduran/sharedir
	sudo cp edgeguard_mqttd /home/rickyduran/sharedir
	sudo cp edgeguard_visiond /home/rickyduran/sharedir
	sudo cp edgeguard_visiond_face /home/rickyduran/sharedir
	sudo cp face_register /home/rickyduran/sharedir

clean:
	rm -f sensor_hubd edgeguard_httpd edgeguard_mqttd edgeguard_visiond edgeguard_visiond_face face_register
	rm -f *.o
