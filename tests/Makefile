CC = cc
CFLAGS = -O2 -std=c99 -Wall -Wextra -Werror -pedantic
BUILD_DIR = ../work/linked-grid-tests
BIN = $(BUILD_DIR)/linked_grid_test$(EXEEXT)
RECOVERY = $(BUILD_DIR)/linked_grid_recovery_test$(EXEEXT)
WIN_STUBS = $(BUILD_DIR)/linked_grid_win_test$(EXEEXT)

.PHONY: test build clean
test: build
	$(BIN)
	$(RECOVERY)
	$(WIN_STUBS)

build: $(BIN) $(RECOVERY) $(WIN_STUBS)

$(BIN): linked_grid_test.c ../linked_grid.c ../linked_grid.h ../linked_grid_queue.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../linked_grid.c linked_grid_test.c $(LDFLAGS) -o $@

clean:
	rm -f $(BIN) $(RECOVERY) $(WIN_STUBS)

$(RECOVERY): linked_grid_recovery_test.c ../linked_grid_recovery.h ../linked_grid.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) linked_grid_recovery_test.c $(LDFLAGS) -o $@

$(WIN_STUBS): linked_grid_win_test.c linked_grid_win_stub.h ../linked_grid_win.h ../linked_grid.c ../linked_grid.h ../linked_grid_queue.h ../linked_grid_recovery.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) linked_grid_win_test.c $(LDFLAGS) -o $@
