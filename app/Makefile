CFLAGS  = -O2 -Wall
SQLITE_FLAGS = -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION
LDFLAGS = -lpthread -ldl
LDFLAGS_HTTPD = -lpthread -ldl

.PHONY: all copy

all: sensor_hubd edgeguard_httpd edgeguard_mqttd

sensor_hubd: sensor_hubd.c sqlite3.c sqlite3.h
	arm-linux-gnueabihf-gcc $(CFLAGS) $(SQLITE_FLAGS) sensor_hubd.c sqlite3.c -o sensor_hubd $(LDFLAGS)

edgeguard_httpd: edgeguard_httpd.c sqlite3.c sqlite3.h
	arm-linux-gnueabihf-gcc $(CFLAGS) $(SQLITE_FLAGS) edgeguard_httpd.c sqlite3.c -o edgeguard_httpd $(LDFLAGS_HTTPD)

edgeguard_mqttd: edgeguard_mqttd.c
	arm-linux-gnueabihf-gcc $(CFLAGS) edgeguard_mqttd.c -o edgeguard_mqttd

copy:
	sudo cp sensor_hubd /home/rickyduran/sharedir
	sudo cp edgeguard_httpd /home/rickyduran/sharedir
	sudo cp edgeguard_mqttd /home/rickyduran/sharedir
