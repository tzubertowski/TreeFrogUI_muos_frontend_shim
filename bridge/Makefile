ROOT := ../upstream
FROGUI := ../../FrogUI
CC ?= cc
CFLAGS += -std=c11 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE
INCLUDES := -I$(FROGUI) -I$(ROOT) -I$(ROOT)/vendor/lvgl -I$(ROOT)/common

all: retro_lvgl_bridge.o

retro_lvgl_bridge.o: retro_lvgl_bridge.c retro_lvgl_bridge.h bridge_logic.c bridge_logic.h
	$(CC) $(CFLAGS) $(INCLUDES) -c retro_lvgl_bridge.c -o $@
	$(CC) $(CFLAGS) $(INCLUDES) -c bridge_logic.c -o bridge_logic.o

check: test_bridge_logic
	./test_bridge_logic

test_bridge_logic: test_bridge_logic.c bridge_logic.c bridge_logic.h retro_lvgl_bridge.h
	$(CC) $(CFLAGS) $(INCLUDES) test_bridge_logic.c bridge_logic.c -o $@

clean:
	rm -f retro_lvgl_bridge.o bridge_logic.o test_bridge_logic

.PHONY: all check clean
