CC = gcc
CFLAGS = -c -Wall -Wextra -Werror -Wmissing-prototypes
STATIC_LIBS = -locispec -lblake3
DYNAMIC_LIBS = -luuid -lcap -ljson-c -lsodium -larchive
INCLUDE_DIRS = -Isrc

SOURCE_DIR = src
BUILD_DIR = bin
TARGET = $(BUILD_DIR)/nash

SOURCES = $(shell find $(SOURCE_DIR) -type f -name '*.c')
OBJECTS = $(patsubst $(SOURCE_DIR)/%, $(BUILD_DIR)/%, $(patsubst %.c, %.o, $(SOURCES)))

SOURCE_SUBDIRS = $(shell find $(SOURCE_DIR) -mindepth 1 -type d)
BUILD_SUBDIRS = $(patsubst $(SOURCE_DIR)/%, $(BUILD_DIR)/%/, $(SOURCE_SUBDIRS))

all: nash

nash: $(OBJECTS)
	$(CC) -o $(TARGET) $^ -Wl,-Bstatic $(STATIC_LIBS) -Wl,-Bdynamic $(DYNAMIC_LIBS)

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(INCLUDE_DIRS) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%/:
	mkdir -p $@

$(BUILD_DIR): $(BUILD_SUBDIRS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
