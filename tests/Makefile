CC = cc
CFLAGS = -O2 -std=c99 -Wall -Wextra -Werror -pedantic
BUILD_DIR = ../work/linked-grid-tests
SHORTCUTS = $(BUILD_DIR)/kb_shortcuts_test$(EXEEXT)
BIN = $(BUILD_DIR)/linked_grid_test$(EXEEXT)
RECOVERY = $(BUILD_DIR)/linked_grid_recovery_test$(EXEEXT)
AUTO_TILE = $(BUILD_DIR)/auto_tile_grid_test$(EXEEXT)
AUTO_WIN = $(BUILD_DIR)/auto_tile_grid_win_test$(EXEEXT)
WIN_STUBS = $(BUILD_DIR)/linked_grid_win_test$(EXEEXT)
PROCESS_NAME = $(BUILD_DIR)/window_process_name_test$(EXEEXT)
PROCESS_MISSING = $(BUILD_DIR)/window_process_name_missing_test$(EXEEXT)
PROCESS_VARIANTS = $(addprefix $(BUILD_DIR)/window_process_name_,unicode limited_missing psapi_missing unicode_missing unicode_limited_missing unicode_psapi_missing)
PROCESS_VARIANTS := $(addsuffix _test$(EXEEXT),$(PROCESS_VARIANTS))

.PHONY: test build clean
test: build
	python3 shortcut_config_test.py
	$(SHORTCUTS)
	$(BIN)
	$(RECOVERY)
	$(AUTO_TILE)
	$(WIN_STUBS)
	$(AUTO_WIN)
	$(PROCESS_NAME)
	$(PROCESS_MISSING)
	set -e; for process_test in $(PROCESS_VARIANTS); do "$$process_test"; done

build: $(SHORTCUTS) $(BIN) $(RECOVERY) $(AUTO_TILE) $(WIN_STUBS) $(AUTO_WIN) $(PROCESS_NAME) $(PROCESS_MISSING) $(PROCESS_VARIANTS)

$(BIN): linked_grid_test.c ../linked_grid.c ../linked_grid.h ../linked_grid_queue.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../linked_grid.c linked_grid_test.c $(LDFLAGS) -o $@

clean:
	rm -f $(SHORTCUTS) $(BIN) $(RECOVERY) $(AUTO_TILE) $(WIN_STUBS) $(AUTO_WIN) $(PROCESS_NAME) $(PROCESS_MISSING) $(PROCESS_VARIANTS)

$(RECOVERY): linked_grid_recovery_test.c ../linked_grid_recovery.h ../linked_grid.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) linked_grid_recovery_test.c $(LDFLAGS) -o $@

$(AUTO_TILE): auto_tile_grid_test.c ../auto_tile_grid.c ../auto_tile_grid.h ../linked_grid.c ../linked_grid.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../auto_tile_grid.c ../linked_grid.c auto_tile_grid_test.c $(LDFLAGS) -o $@
$(WIN_STUBS): linked_grid_win_test.c linked_grid_win_stub.h ../linked_grid_win.h ../linked_grid.c ../linked_grid.h ../linked_grid_queue.h ../linked_grid_recovery.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) linked_grid_win_test.c $(LDFLAGS) -o $@

$(AUTO_WIN): ../linked_grid.c auto_tile_grid_win_test.c ../auto_tile_grid_win.h ../auto_tile_owned_guard.h ../auto_tile_owned_store.h ../auto_tile_owned_runtime.h ../auto_tile_owned_match.h ../window_process_name.h ../auto_tile_grid.c ../auto_tile_grid.h ../linked_grid.h ../linked_grid_queue.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../linked_grid.c auto_tile_grid_win_test.c $(LDFLAGS) -o $@

$(PROCESS_NAME): window_process_name_test.c ../window_process_name.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) window_process_name_test.c $(LDFLAGS) -o $@

$(PROCESS_MISSING): window_process_name_test.c ../window_process_name.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPROCESS_NAME_MISSING_APIS window_process_name_test.c $(LDFLAGS) -o $@

# Separate processes exercise each cached dynamic-loader result in A and W builds.
define process_variant
$(BUILD_DIR)/window_process_name_$(1)_test$(EXEEXT): window_process_name_test.c ../window_process_name.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(2) window_process_name_test.c $(LDFLAGS) -o $$@
endef
$(eval $(call process_variant,unicode,-D_UNICODE))
$(eval $(call process_variant,limited_missing,-DPROCESS_NAME_MISSING_LIMITED))
$(eval $(call process_variant,psapi_missing,-DPROCESS_NAME_MISSING_PSAPI))
$(eval $(call process_variant,unicode_missing,-D_UNICODE -DPROCESS_NAME_MISSING_APIS))
$(eval $(call process_variant,unicode_limited_missing,-D_UNICODE -DPROCESS_NAME_MISSING_LIMITED))
$(eval $(call process_variant,unicode_psapi_missing,-D_UNICODE -DPROCESS_NAME_MISSING_PSAPI))

$(SHORTCUTS): kb_shortcuts_test.c ../kb_shortcuts.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) kb_shortcuts_test.c $(LDFLAGS) -o $@
