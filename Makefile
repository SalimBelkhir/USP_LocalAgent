CC      := aarch64-openwrt-linux-gcc
FLAGS  := -Wall -fPIC
LDFLAGS := -L$(STAGING_DIR)/target-aarch64_cortex-a72_musl/usr/lib 
LDLIBS := -lamxc -lamxp -lamxd -lcap-ng 
CFLAGS += -I$(STAGING_DIR)/target-aarch64_cortex-a72_musl/usr/include
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

TARGET  := $(BIN_DIR)/output

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS)  $(LDFLAGS) -o $@ $(LDLIBS) $(FLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) -c $< $(CFLAGS) -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

.PHONY: all clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)










