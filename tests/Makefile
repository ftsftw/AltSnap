CC = cc
CFLAGS = -O2 -std=c99 -Wall -Wextra -Werror -pedantic
BUILD_DIR = ../work/linked-grid-tests
BIN = $(BUILD_DIR)/linked_grid_test$(EXEEXT)
RECOVERY = $(BUILD_DIR)/linked_grid_recovery_test$(EXEEXT)
AUTO_TILE = $(BUILD_DIR)/auto_tile_grid_test$(EXEEXT)
AUTO_WIN = $(BUILD_DIR)/auto_tile_grid_win_test$(EXEEXT)
WIN_STUBS = $(BUILD_DIR)/linked_grid_win_test$(EXEEXT)
PROCESS_NAME = $(BUILD_DIR)/window_process_name_test$(EXEEXT)
PROCESS_MISSING = $(BUILD_DIR)/window_process_name_missing_test$(EXEEXT)

.PHONY: test build clean
test: build
	$(BIN)
	$(RECOVERY)
	$(AUTO_TILE)
	$(WIN_STUBS)
	$(AUTO_WIN)
	$(PROCESS_NAME)
	$(PROCESS_MISSING)

build: $(BIN) $(RECOVERY) $(AUTO_TILE) $(WIN_STUBS) $(AUTO_WIN) $(PROCESS_NAME) $(PROCESS_MISSING)

$(BIN): linked_grid_test.c ../linked_grid.c ../linked_grid.h ../linked_grid_queue.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../linked_grid.c linked_grid_test.c $(LDFLAGS) -o $@

clean:
	rm -f $(BIN) $(RECOVERY) $(AUTO_TILE) $(WIN_STUBS) $(AUTO_WIN) $(PROCESS_NAME) $(PROCESS_MISSING)

$(RECOVERY): linked_grid_recovery_test.c ../linked_grid_recovery.h ../linked_grid.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) linked_grid_recovery_test.c $(LDFLAGS) -o $@

$(AUTO_TILE): auto_tile_grid_test.c ../auto_tile_grid.c ../auto_tile_grid.h ../linked_grid.c ../linked_grid.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../auto_tile_grid.c ../linked_grid.c auto_tile_grid_test.c $(LDFLAGS) -o $@
$(WIN_STUBS): linked_grid_win_test.c linked_grid_win_stub.h ../linked_grid_win.h ../linked_grid.c ../linked_grid.h ../linked_grid_queue.h ../linked_grid_recovery.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) linked_grid_win_test.c $(LDFLAGS) -o $@

$(AUTO_WIN): auto_tile_grid_win_test.c ../auto_tile_grid_win.h ../auto_tile_grid.c ../auto_tile_grid.h ../linked_grid.h ../linked_grid_queue.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) auto_tile_grid_win_test.c $(LDFLAGS) -o $@

$(PROCESS_NAME): window_process_name_test.c ../window_process_name.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) window_process_name_test.c $(LDFLAGS) -o $@

$(PROCESS_MISSING): window_process_name_test.c ../window_process_name.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPROCESS_NAME_MISSING_APIS window_process_name_test.c $(LDFLAGS) -o $@
