CC ?= cc
CFLAGS ?= -O2 -std=c2x -Wall -Wextra -Wpedantic
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
LDFLAGS ?=
LDLIBS ?= -lm

BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/HeroineCLI
OBJECTS := $(BUILD_DIR)/HeroineCLI.o $(BUILD_DIR)/GPMF_parser.o $(BUILD_DIR)/GPMF_mp4reader.o
SOURCE_CONTEXT_TEST := $(BUILD_DIR)/test_source_context

.PHONY: all clean test
all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $@

$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD_DIR)/HeroineCLI.o: HeroineCLI.c GPMF_parser.h GPMF_common.h GPMF_mp4reader.h | $(BUILD_DIR)
$(BUILD_DIR)/GPMF_parser.o: GPMF_parser.c GPMF_parser.h GPMF_common.h GPMF_bitstream.h | $(BUILD_DIR)
$(BUILD_DIR)/GPMF_mp4reader.o: GPMF_mp4reader.c GPMF_mp4reader.h GPMF_parser.h | $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(SOURCE_CONTEXT_TEST): tests/test_source_context.c $(BUILD_DIR)/GPMF_mp4reader.o $(BUILD_DIR)/GPMF_parser.o | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. $(LDFLAGS) -o $@ $^ $(LDLIBS)

test: $(TARGET) $(SOURCE_CONTEXT_TEST)
	./tests/test_cli.sh $(TARGET) "$(GPMF_SAMPLE)" "$(GPMF_NO_GPS_SAMPLE)"
	@if [ -n "$(GPMF_SAMPLE)" ]; then ./$(SOURCE_CONTEXT_TEST) "$(GPMF_SAMPLE)"; fi

clean:
	rm -rf $(BUILD_DIR)
