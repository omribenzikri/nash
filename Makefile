CC = gcc
CFLAGS = -c -Wall -Werror
LIBS = -luuid

SOURCE_DIR = src
BUILD_DIR = bin
TARGET = $(BUILD_DIR)/nash

SOURCES = $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS = $(subst $(SOURCE_DIR), $(BUILD_DIR), $(patsubst %.c, %.o, $(SOURCES)))

all: nash

nash: $(OBJECTS)
	$(CC) -o $(TARGET) $^ $(LIBS)

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
